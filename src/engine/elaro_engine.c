#include <src/common.h>
#include "elaro_engine.h"
#include <glib-unix.h>
#include <src/fe-efl/elaro_intl.h>
#define MODULE_NAME "fe-efl"
#include <src/core/core.h>
#include <src/core/signals.h>
#include <src/core/servers.h>
#include <src/core/servers-setup.h>
#include <src/core/servers-reconnect.h>
#include <src/core/channels.h>
#include <src/core/nicklist.h>
#include <src/core/settings.h>
#include <src/irc/core/irc.h>
#include <src/irc/core/irc-servers.h>
#include <src/irc/core/irc-queries.h>
#include <src/irc/core/sasl.h>
#include "elaro_dcc.h"
#include "elaro_ctcp.h"
#include "elaro_bans.h"
#include "elaro_list.h"
#include "elaro_engine_private.h"

void
irc_init(void);
void
irc_deinit(void);

typedef struct
{
   SERVER_CONNECT_REC *conn;
   SERVER_REC *server;
   char *tag, *profile_nick;
   guint retry_timer, timeout_timer, attempts;
   gboolean enabled, registered;
} Connection;

struct _Elaro_Engine
{
   GAsyncQueue *commands, *events;
   int command_pipe[2], event_pipe[2];
   GThread *thread;
   GSList *connections;
   GMainLoop *loop;
   char *binary, *selected_server, *selected_target, *last_nicks;
   GHashTable *whois_presence;
};

typedef struct
{
   gboolean previous, away;
} Whois_Presence;

/* Irssi is a singleton. This pointer is accessed only on its worker thread. */
static Elaro_Engine *active;

/* One byte means nonempty. Queue and readiness changes share the queue lock,
 * so a producer cannot race the consumer clearing the last notification.
 * Bounded consumers leave the byte in place while work remains. */
static void
_queue_push(GAsyncQueue *queue, int *fds, gpointer item)
{
   char byte = 1;
   ssize_t result;

   g_async_queue_lock(queue);
   if (g_async_queue_length_unlocked(queue) == 0)
     {
        do result = write(fds[1], &byte, 1); while (result < 0 && errno == EINTR);
        if (result != 1) g_error("Cannot signal engine queue: %s", g_strerror(errno));
     }
   g_async_queue_push_unlocked(queue, item);
   g_async_queue_unlock(queue);
}

static gpointer
_queue_pop(GAsyncQueue *queue, int *fds)
{
   gpointer item;
   char byte;
   ssize_t result;

   g_async_queue_lock(queue);
   item = g_async_queue_try_pop_unlocked(queue);
   if (item && g_async_queue_length_unlocked(queue) == 0)
     {
        do result = read(fds[0], &byte, 1); while (result < 0 && errno == EINTR);
        if (result != 1) g_error("Cannot clear engine queue signal: %s", g_strerror(errno));
     }
   g_async_queue_unlock(queue);
   return item;
}

static gboolean
_wakeup_pipe(int *fds)
{
   if (!g_unix_open_pipe(fds, FD_CLOEXEC, NULL)) return FALSE;
   if (g_unix_set_fd_nonblocking(fds[0], TRUE, NULL)
       && g_unix_set_fd_nonblocking(fds[1], TRUE, NULL)) return TRUE;
   close(fds[0]);
   close(fds[1]);
   return FALSE;
}

void
elaro_engine_event_push(Elaro_Engine_Event *event)
{
   _queue_push(active->events, active->event_pipe, event);
}

/* Nickname characters count as word characters, including RFC1459 punctuation. */
static gboolean
_nick_character(unsigned char c)
{
   return c >= 128 || g_ascii_isalnum(c) || (c && strchr("-[]\\`^{}_|", c));
}

static gboolean
_nick_mentioned(IRC_SERVER_REC *server, const char *text)
{
   size_t length = strlen(server->nick);
   if (!length || !text) return FALSE;
   size_t remaining = strlen(text);
   for (const char *p = text; remaining >= length; p++, remaining--)
     {
        if (p != text && _nick_character((unsigned char)p[-1])) continue;
        if (_nick_character((unsigned char)p[length])) continue;
        char *candidate = g_strndup(p, length);
        gboolean match = !server->nick_comp_func(candidate, server->nick);
        g_free(candidate);
        if (match) return TRUE;
     }
   return FALSE;
}

static void
_emit_event_full(Elaro_Engine_Event_Type type, const char *server, const char *target,
                 const char *text, bool private_message, bool mention)
{
   Elaro_Engine_Event *e;

   e = g_new0(Elaro_Engine_Event, 1);
   e->type = type;
   e->private_message = private_message;
   e->mention = mention;
   e->server = g_strdup(server ? server : "Status");
   e->target = g_strdup(target ? target : "");
   e->text = g_utf8_make_valid(text ? text : "", -1);
   elaro_engine_event_push(e);
}

static void
_emit_event(Elaro_Engine_Event_Type type, const char *server, const char *target, const char *text)
{
   _emit_event_full(type, server, target, text, false, false);
}

void
elaro_engine_event_free(Elaro_Engine_Event *e)
{
   for (unsigned int i = 0; i < e->channel_count; i++)
     {
        g_free(e->channels[i].name);
        g_free(e->channels[i].topic);
     }
   g_free(e->channels);
   for (unsigned int i = 0; i < e->user_count; i++)
      g_free(e->users[i].nick);
   g_free(e->users);
   g_free(e->host);
   g_free(e->nick);
   g_free(e->server);
   g_free(e->target);
   g_free(e->text);
   g_free(e);
}

/* Presence is authoritative protocol state, independent of chat activity. */
static void
_presence_emit(SERVER_REC *server, const char *nick, gboolean away)
{
   Elaro_Engine_Event *event = g_new0(Elaro_Engine_Event, 1);

   event->type = ELARO_ENGINE_USER_PRESENCE;
   event->server = g_strdup(server->tag);
   event->target = g_strdup(nick);
   event->text = g_strdup("");
   event->away = away;
   elaro_engine_event_push(event);
}

static char *
_whois_presence_key(SERVER_REC *server, const char *nick)
{
   char *lower = g_ascii_strdown(nick, -1);
   char *key = g_strdup_printf("%s\n%s", server->tag, lower);

   g_free(lower);
   return key;
}

static Whois_Presence *
_whois_presence_find(SERVER_REC *server, const char *nick)
{
   char *key = _whois_presence_key(server, nick);
   Whois_Presence *state = g_hash_table_lookup(active->whois_presence, key);

   g_free(key);
   return state;
}

static void
_presence_known(SERVER_REC *server, const char *nick)
{
   GSList *matches = nicklist_get_same(server, nick);

   if (matches)
     {
        NICK_REC *user = matches->next->data;
        Whois_Presence *pending = _whois_presence_find(server, nick);
        _presence_emit(server, nick, pending ? pending->previous : user->gone);
     }
   g_slist_free(matches);
}

static void
_nick_gone_changed(CHANNEL_REC *channel, NICK_REC *nick)
{
   /* Irssi clears gone at 311, before a possible away reply arrives. */
   if (!_whois_presence_find(channel->server, nick->nick))
      _presence_emit(channel->server, nick->nick, nick->gone);
}

static void
_away_notify(IRC_SERVER_REC *server, const char *data, const char *nick)
{
   char *reason, *params;
   Whois_Presence *state;

   if (!nick) return;
   params = event_get_params(data, 1 | PARAM_FLAG_GETREST, &reason);
   state = _whois_presence_find(SERVER(server), nick);
   /* A live notification takes precedence over an in-flight WHOIS. */
   if (state) state->previous = state->away = *reason != '\0';
   _presence_emit(SERVER(server), nick, *reason != '\0');
   g_free(params);
}

/* A WHOIS user reply alone does not mean back: the away reply follows it.
 * Publish the completed result at 318, including peers outside shared channels. */
static void
_whois_presence_begin(IRC_SERVER_REC *server, const char *data)
{
   char *nick, *params;
   GSList *matches;
   Whois_Presence *state = g_new0(Whois_Presence, 1);

   params = event_get_params(data, 2, NULL, &nick);
   matches = nicklist_get_same(SERVER(server), nick);
   if (matches) state->previous = ((NICK_REC *)matches->next->data)->gone;
   g_slist_free(matches);
   g_hash_table_replace(active->whois_presence, _whois_presence_key(SERVER(server), nick), state);
   g_free(params);
}

static void
_whois_presence_away(IRC_SERVER_REC *server, const char *data)
{
   char *nick, *params;
   Whois_Presence *state;

   params = event_get_params(data, 2, NULL, &nick);
   state = _whois_presence_find(SERVER(server), nick);
   if (state) state->previous = state->away = TRUE;
   /* 301 is also an away reply to a private message, without a WHOIS. */
   nicklist_update_flags(SERVER(server), nick, TRUE, -1);
   _presence_emit(SERVER(server), nick, TRUE);
   g_free(params);
}

static void
_whois_presence_end(IRC_SERVER_REC *server, const char *data)
{
   char *nick, *key, *params;
   Whois_Presence *state;

   params = event_get_params(data, 2, NULL, &nick);
   key = _whois_presence_key(SERVER(server), nick);
   state = g_hash_table_lookup(active->whois_presence, key);
   if (state) _presence_emit(SERVER(server), nick, state->away);
   g_hash_table_remove(active->whois_presence, key);
   g_free(key);
   g_free(params);
}

static Connection *
_connection_find(SERVER_CONNECT_REC *conn)
{
   GSList *node;
   for (node = active->connections; node; node = node->next)
     {
        Connection *connection = node->data;
        if (connection->conn == conn) return connection;
     }
   return NULL;
}

static void
_connection_timers_cancel(Connection *connection)
{
   if (connection->retry_timer) g_source_remove(connection->retry_timer);
   if (connection->timeout_timer) g_source_remove(connection->timeout_timer);
   connection->retry_timer = connection->timeout_timer = 0;
}

static void
_connection_start(Connection *connection);

static gboolean
_connection_retry(gpointer data)
{
   Connection *connection = data;
   char *message;

   connection->retry_timer = 0;
   connection->attempts++;
   message = g_strdup_printf("Reconnecting (attempt %u of 5)…", connection->attempts);
   _emit_event(ELARO_ENGINE_LINE, connection->tag, "", message);
   g_free(message);
   _connection_start(connection);
   return G_SOURCE_REMOVE;
}

static void
_connection_schedule(Connection *connection)
{
   _connection_timers_cancel(connection);
   connection->server = NULL;
   if (!connection->enabled) return;
   if (connection->attempts >= 5)
     {
        _emit_event(ELARO_ENGINE_LINE, connection->tag, "",
                    "Auto-reconnect stopped after 5 retries");
        return;
     }
   _emit_event(ELARO_ENGINE_LINE, connection->tag, "", "Reconnecting in 5 seconds…");
   connection->retry_timer = g_timeout_add(5000, _connection_retry, connection);
}

static gboolean
_connection_timeout(gpointer data)
{
   Connection *connection = data;
   SERVER_REC *server = connection->server;

   connection->timeout_timer = 0;
   if (server)
     {
        _emit_event(ELARO_ENGINE_LINE, connection->tag, "",
                    "Connection timed out after 30 seconds");
        server->connection_lost = TRUE;
        server_disconnect(server);
     }
   return G_SOURCE_REMOVE;
}

static void
_connection_start(Connection *connection)
{
   CHAT_PROTOCOL_REC *protocol = CHAT_PROTOCOL(connection->conn);
   SERVER_REC *server = protocol->server_init_connect(connection->conn);

   if (!server)
     {
        _connection_schedule(connection);
        return;
     }
   /* The adapter owns retry timing and limits; suppress Irssi's retry queue. */
   server->no_reconnect = TRUE;
   connection->server = server;
   g_free(connection->tag);
   connection->tag = g_strdup(server->tag);
   g_free(connection->conn->tag);
   connection->conn->tag = g_strdup(server->tag);
   connection->timeout_timer = g_timeout_add(30000, _connection_timeout, connection);
   protocol->server_connect(server);
}

static void
_connection_state(SERVER_REC *s, bool connected)
{
   Connection *connection = _connection_find(s->connrec);
   Elaro_Engine_Event *event = g_new0(Elaro_Engine_Event, 1);

   event->type = ELARO_ENGINE_CONNECTION;
   event->server = g_strdup(s->tag);
   event->target = g_strdup("");
   event->text = g_strdup("");
   event->host = g_strdup(s->connrec->address);
   event->nick = g_strdup(connection ? connection->profile_nick : s->connrec->nick);
   event->port = s->connrec->port;
   event->connected = connected;
   elaro_engine_event_push(event);
}

static void
_connected(SERVER_REC *s)
{
   _connection_state(s, true);
   _emit_event(ELARO_ENGINE_LINE, s->tag, "", "Connected");
}

static void
_registered(SERVER_REC *s)
{
   Connection *connection = _connection_find(s->connrec);
   if (connection)
     {
        _connection_timers_cancel(connection);
        connection->attempts = 0;
        connection->registered = TRUE;
     }
   if (s->connrec->channels && *s->connrec->channels)
      s->channels_join(s, s->connrec->channels, FALSE);
}

static void
_disconnected(SERVER_REC *s)
{
   Connection *connection = _connection_find(s->connrec);
   s->no_reconnect = TRUE;
   _connection_state(s, false);
   _emit_event(ELARO_ENGINE_LINE, s->tag, "", "Disconnected");
   _emit_event(ELARO_ENGINE_PRESENCE_RESET, s->tag, "", "");
   GHashTableIter presence_iter;
   gpointer presence_key;
   char *prefix = g_strdup_printf("%s\n", s->tag);
   g_hash_table_iter_init(&presence_iter, active->whois_presence);
   while (g_hash_table_iter_next(&presence_iter, &presence_key, NULL))
      if (g_str_has_prefix(presence_key, prefix)) g_hash_table_iter_remove(&presence_iter);
   g_free(prefix);
   if (!connection) return;
   if (connection->registered && s->connection_lost)
     {
        char *channels = irc_server_get_channels(IRC_SERVER(s), REJOIN_CHANNELS_MODE_ON);
        g_free(connection->conn->channels);
        connection->conn->channels = channels;
        connection->registered = FALSE;
     }
   if (!s->connection_lost) connection->enabled = FALSE;
   _connection_schedule(connection);
}

static void
_failed(SERVER_REC *s, const char *reason)
{
   Connection *connection = _connection_find(s->connrec);
   s->no_reconnect = TRUE;
   _emit_event(ELARO_ENGINE_LINE, s->tag, "", reason ? reason : "Connection failed");
   if (connection) _connection_schedule(connection);
}

static void
_auth_failed(SERVER_REC *s, const char *reason)
{
   _emit_event(ELARO_ENGINE_LINE, s->tag, "", reason ? reason : "Authentication failed");
}

static void
_dialog(const char *type, const char *text)
{
   _emit_event(ELARO_ENGINE_LINE, NULL, "", text);
}

/* CTCP has already removed the protocol frame before emitting this signal. */
static void
_action(IRC_SERVER_REC *s, const char *body, const char *nick, const char *address,
        const char *target)
{
   gboolean channel = server_ischannel(SERVER(s), target);
   char *text;

   if (channel && !channel_find(SERVER(s), target)) return;
   text = g_strdup_printf("* %s %s", nick ? nick : s->tag, body);
   _emit_event_full(ELARO_ENGINE_LINE, s->tag, channel ? target : nick, text,
                    !channel && nick && address && *address && !s->nick_comp_func(target, s->nick)
                        && s->nick_comp_func(nick, s->nick),
                    channel && nick && s->nick_comp_func(nick, s->nick)
                        && _nick_mentioned(s, body));
   if (!channel && nick) _presence_known(SERVER(s), nick);
   g_free(text);
}

/* QUIT and NICK affect every shared channel. Observe membership before
 * Irssi removes or renames the nick, without changing protocol handling. */
static void
_membership_event(IRC_SERVER_REC *s, const char *line, const char *nick, const char *address)
{
   gboolean quit = !g_ascii_strncasecmp(line, "QUIT", 4) && (!line[4] || line[4] == ' ');
   gboolean rename = !g_ascii_strncasecmp(line, "NICK ", 5);
   char *params, *value, *text;
   const char *args;
   GSList *matches, *node;
   QUERY_REC *query;
   gboolean emitted = FALSE;

   if ((!quit && !rename) || !nick) return;
   _emit_event(ELARO_ENGINE_PRESENCE_RESET, s->tag, nick, "");
   args = line + (quit ? 4 : 5);
   while (*args == ' ')
      args++;
   params = event_get_params(args, 1 | PARAM_FLAG_GETREST, &value);
   if (rename && !*value)
     {
        g_free(params);
        return;
     }
   if (rename) text = g_strdup_printf(_("* %s is now known as %s"), nick, value);
   else if (*value) text = g_strdup_printf(_("* %s has quit (%s)"), nick, value);
   else text = g_strdup_printf(_("* %s has quit"), nick);

   matches = nicklist_get_same(SERVER(s), nick);
   for (node = matches; node; node = node->next->next)
     {
        CHANNEL_REC *channel = node->data;
        _emit_event(ELARO_ENGINE_LINE, s->tag, channel->name, text);
        emitted = TRUE;
     }
   g_slist_free(matches);
   query = query_find(SERVER(s), nick);
   if (query)
     {
        _emit_event(ELARO_ENGINE_LINE, s->tag, query->name, text);
        emitted = TRUE;
     }
   if (!emitted || !s->nick_comp_func(nick, s->nick))
      _emit_event(ELARO_ENGINE_LINE, s->tag, "", text);
   g_free(text);
   g_free(params);
}

/* Observe parsed IRC events after the protocol handlers update engine state. */
static void
_server_event(IRC_SERVER_REC *s, const char *line, const char *nick, const char *address)
{
   char *end;
   g_autofree char *membership_params = NULL;

   char *copy, *args, *target, *text, *body, *victim;
   gboolean own_part, own_kick = FALSE, mention = FALSE;

   copy = g_strdup(line);
   args = strchr(copy, ' ');
   target = NULL;

   if (args) *args++ = 0;
   else args = copy + strlen(copy);

   if (!strcmp(copy, "321") || !strcmp(copy, "322") || !strcmp(copy, "323"))
     {
        /* LIST replies belong to the channel browser, never chat scrollback. */
        g_free(copy);
        return;
     }
   if (!g_ascii_strcasecmp(copy, "QUIT") || !g_ascii_strcasecmp(copy, "NICK"))
     {
        /* The membership observer emitted readable lines before state changed. */
        g_free(copy);
        return;
     }
   if (!g_ascii_strcasecmp(copy, "PRIVMSG") || !g_ascii_strcasecmp(copy, "NOTICE"))
     {
        body = strchr(args, ' ');

        if (body)
          {
             *body++ = 0;

             if (*body == ':') body++;
             target = server_ischannel(SERVER(s), args)
                          ? args
                          : (address && *address ? (char *)nick : NULL);

             if (*body == '\001')
               {
                  /* Parsed CTCP signals handle actions and replies. */
                  g_free(copy);
                  return;
               }
             mention = !g_ascii_strcasecmp(copy, "PRIVMSG") && target && nick
                       && server_ischannel(SERVER(s), target)
                       && s->nick_comp_func(nick, s->nick) && _nick_mentioned(s, body);
             text = g_strdup_printf("<%s> %s", nick ? nick : s->tag, body);
          }
        else text = g_strdup(line);
     }
   else if (!strcmp(copy, "404"))
     {
        char *params = event_get_params(args, 3 | PARAM_FLAG_GETREST, NULL, &victim, &body);
        CHANNEL_REC *channel = *victim ? channel_find(SERVER(s), victim) : NULL;

        if (*body)
           text = g_strdup_printf(_("Cannot send to channel %s (%s)"), victim, body);
        else
           text = g_strdup_printf(_("Cannot send to channel %s"), victim);
        /* Put rejection feedback where the message was sent. An error for an
         * unjoined channel belongs to the server, without reopening a buffer. */
        _emit_event(ELARO_ENGINE_LINE, s->tag, channel ? channel->name : NULL, text);
        g_free(text);
        g_free(params);
        g_free(copy);
        return;
     }
   else if (!strcmp(copy, "401"))
     {
        char *params = event_get_params(args, 3, NULL, &victim, &body);
        QUERY_REC *query = *victim && !server_ischannel(SERVER(s), victim)
                               ? query_find(SERVER(s), victim)
                               : NULL;
        if (query)
          {
             text = g_strdup_printf("%s: %s", victim, *body ? body : "No such nick");
             _emit_event(ELARO_ENGINE_LINE, s->tag, query->name, text);
             g_free(text);
             g_free(params);
             g_free(copy);
             return;
          }
        g_free(params);
        text = g_strdup_printf("%s%s%s", nick ? nick : "", nick ? ": " : "", line);
     }
   else if (!g_ascii_strcasecmp(copy, "JOIN") || !g_ascii_strcasecmp(copy, "PART"))
     {
        char *reason;
        gboolean join = !g_ascii_strcasecmp(copy, "JOIN");

        membership_params = event_get_params(args, 2 | PARAM_FLAG_GETREST, &target, &reason);
        if (join) text = g_strdup_printf(_("* %s has joined %s"), nick ? nick : "", target);
        else if (*reason)
           text = g_strdup_printf(_("* %s has left %s (%s)"), nick ? nick : "", target, reason);
        else text = g_strdup_printf(_("* %s has left %s"), nick ? nick : "", target);
        /* Shared-channel membership also provides context in an existing
         * private conversation, without opening queries for other users. */
        if (nick && s->nick_comp_func(nick, s->nick))
          {
             QUERY_REC *query = query_find(SERVER(s), nick);
             if (query) _emit_event(ELARO_ENGINE_LINE, s->tag, query->name, text);
          }
     }
   else if (!g_ascii_strcasecmp(copy, "KICK") || !g_ascii_strcasecmp(copy, "TOPIC"))
     {

        target = args + (*args == ':');
        end = strchr(target, ' ');
        text = g_strdup_printf("* %s %s %s", nick ? nick : "", copy, args);

        if (end && !g_ascii_strcasecmp(copy, "KICK"))
          {
             victim = g_strndup(end + 1, strcspn(end + 1, " "));
             own_kick = !s->nick_comp_func(victim, s->nick);
             g_free(victim);
          }
        if (end) *end = 0;
     }
   else
     {
        text = g_strdup_printf("%s%s%s", nick ? nick : "", nick ? ": " : "", line);
     }
   own_part = !g_ascii_strcasecmp(copy, "PART") && nick && !s->nick_comp_func(nick, s->nick);
   /* Protocol handlers have already removed channels we left. Late channel
    * traffic must not recreate their UI buffers and retained histories. */
   if (target && server_ischannel(SERVER(s), target) && !channel_find(SERVER(s), target)
       && !own_part && !own_kick)
     {
        g_free(text);
        g_free(copy);
        return;
     }
   _emit_event_full(ELARO_ENGINE_LINE, s->tag, own_part || own_kick ? NULL : target, text,
                    !g_ascii_strcasecmp(copy, "PRIVMSG") && target && nick && address && *address
                        && !server_ischannel(SERVER(s), args) && !s->nick_comp_func(args, s->nick)
                        && s->nick_comp_func(nick, s->nick), mention);

   if (target && !server_ischannel(SERVER(s), target)) _presence_known(SERVER(s), target);

   if (own_part || own_kick) _emit_event(ELARO_ENGINE_BUFFER_CLOSED, s->tag, target, "");

   if (!g_ascii_strcasecmp(copy, "JOIN") && nick && !s->nick_comp_func(nick, s->nick))
      _emit_event(ELARO_ENGINE_JOINED, s->tag, target, "");
   g_free(text);
   g_free(copy);
}

static void
_user_details(IRC_SERVER_REC *server, const char *data)
{
   char *params, *nick, *realname;

   params = event_get_params(data, 6, NULL, &nick, NULL, NULL, NULL, &realname);
   _emit_event(ELARO_ENGINE_USER_DETAILS, server->tag, nick, realname);
   g_free(params);
}

static void
_topic_changed(CHANNEL_REC *channel)
{
   _emit_event(ELARO_ENGINE_TOPIC, channel->server->tag, channel->name, channel->topic);
}

static gboolean
_clean_field(const char *s)
{
   return s && !strpbrk(s, "\r\n");
}

static void
_connect_command(Elaro_Engine_Command *c)
{
   SERVER_CONNECT_REC *conn;
   IRC_SERVER_CONNECT_REC *irc_conn;
   Connection *connection;

   if (!_clean_field(c->host) || !*c->host || strpbrk(c->host, " \t") || !_clean_field(c->nick)
       || !*c->nick || strpbrk(c->nick, " ,:\t") || !_clean_field(c->password)
       || (c->username && (!*c->username || strpbrk(c->username, " @:\t\r\n")))
       || (c->realname && (!_clean_field(c->realname) || strlen(c->realname) > 128))
       || (c->email
           && (!_clean_field(c->email) || strlen(c->email) > 254 || strpbrk(c->email, " <>\t")))
       || !_clean_field(c->channels) || c->port < 1 || c->port > 65535)
     {
        _emit_event(ELARO_ENGINE_LINE, NULL, "", "Invalid connection fields");

        return;
     }

   if (c->sasl
       && (!_clean_field(c->sasl_username) || !*c->sasl_username || !_clean_field(c->sasl_password)
           || !*c->sasl_password))
     {
        _emit_event(ELARO_ENGINE_LINE, NULL, "", "SASL requires an account name and password");

        return;
     }
   conn = server_create_conn(chat_protocol_lookup("IRC"), c->host, c->port, NULL, c->password,
                             c->nick);

   if (!conn) return;

   g_free(conn->username);
   conn->username = g_strdup(c->username ? c->username : "elaro");
   g_free(conn->realname);
   conn->realname
       = c->email && *c->email
             ? g_strdup_printf("%s <%s>", c->realname && *c->realname ? c->realname : c->nick,
                               c->email)
             : g_strdup(c->realname && *c->realname ? c->realname : "elaro user");
   irc_conn = IRC_SERVER_CONNECT(conn);
   irc_conn->sasl_mechanism = c->sasl ? SASL_MECHANISM_PLAIN : SASL_MECHANISM_NONE;
   g_free(irc_conn->sasl_username);
   g_free(irc_conn->sasl_password);
   irc_conn->sasl_username = g_strdup(c->sasl_username);
   irc_conn->sasl_password = g_strdup(c->sasl_password);
   conn->use_tls = c->tls;
   conn->tls_verify = TRUE;
   conn->no_autojoin_channels = TRUE;
   conn->no_autosendcmd = TRUE;
   g_free(conn->channels);
   conn->channels = g_strdup(c->channels);
   _emit_event(ELARO_ENGINE_LINE, NULL, "", "Connecting…");
   connection = g_new0(Connection, 1);
   connection->conn = conn; /* Own the creation reference through retries. */
   connection->profile_nick = g_strdup(c->nick);
   connection->enabled = c->auto_reconnect;
   active->connections = g_slist_append(active->connections, connection);
   _connection_start(connection);
}

static void
_open_query(const char *server_tag, const char *nick)
{
   SERVER_REC *s;
   QUERY_REC *query;

   s = server_find_tag(server_tag ? server_tag : "");

   if (!s || !s->connected)
     {
        _emit_event(ELARO_ENGINE_LINE, server_tag, "", "Connect to a server first");

        return;
     }

   if (!nick || !*nick || strpbrk(nick, " ,:\t\r\n") || server_ischannel(s, nick))
     {
        _emit_event(ELARO_ENGINE_LINE, s->tag, "", "Usage: /query nickname");

        return;
     }
   query = query_find(s, nick);

   if (!query) query = irc_query_create(s->tag, nick, FALSE);

   if (query)
     {
        _emit_event(ELARO_ENGINE_QUERY_OPENED, s->tag, query->name, "");
        _presence_known(s, query->name);
     }
}

static void
_connection_cancel(const char *server_tag)
{
   GSList *node;
   for (node = active->connections; node; node = node->next)
     {
        Connection *connection = node->data;
        if (g_ascii_strcasecmp(connection->tag ? connection->tag : "",
                               server_tag ? server_tag : ""))
           continue;
        connection->enabled = FALSE;
        _connection_timers_cancel(connection);
     }
}

static void
_close_server(const char *server_tag)
{
   SERVER_REC *s;
   QUERY_REC *query;
   RECONNECT_REC *reconnect;
   GSList *node, *next;

   if (!server_tag || !*server_tag || !strcmp(server_tag, "Status")) return;

   _connection_cancel(server_tag);
   /* Queries survive ordinary disconnects; explicitly close them here. */
   for (node = queries; node; node = next)
     {
        next = node->next;
        query = node->data;
        if (query->server_tag && !g_ascii_strcasecmp(query->server_tag, server_tag))
           query_destroy(query);
     }
   for (node = reconnects; node; node = next)
     {
        next = node->next;
        reconnect = node->data;
        if (reconnect->conn->tag && !g_ascii_strcasecmp(reconnect->conn->tag, server_tag))
           server_reconnect_destroy(reconnect);
     }
   s = server_find_tag(server_tag);
   if (!s)
     {
        for (node = lookup_servers; node; node = node->next)
          {
             s = node->data;
             if (s->tag && !g_ascii_strcasecmp(s->tag, server_tag)) break;
             s = NULL;
          }
     }
   if (s)
     {
        s->no_reconnect = TRUE;
        if (s->connected) irc_send_cmd_now(IRC_SERVER(s), "QUIT :Server closed");
        server_disconnect(s);
     }
   if (!g_strcmp0(active->selected_server, server_tag))
     {
        g_clear_pointer(&active->selected_server, g_free);
        g_clear_pointer(&active->selected_target, g_free);
        g_clear_pointer(&active->last_nicks, g_free);
     }
   /* This follows disconnect notifications so none can recreate the row. */
   _emit_event(ELARO_ENGINE_BUFFER_CLOSED, server_tag, "", "");
}

/* A channel closed during the retry delay must not return on reconnect.
 * Preserve positional channel keys while removing it from the saved JOIN. */
static void
_connection_forget_channel(const char *server_tag, const char *target)
{
   GSList *node;
   for (node = active->connections; node; node = node->next)
     {
        Connection *connection = node->data;
        char **parts, **names, **keys;
        GString *channels, *passwords;
        guint i, key_count;

        if (g_ascii_strcasecmp(connection->tag ? connection->tag : "",
                               server_tag ? server_tag : ""))
           continue;
        parts = g_strsplit(connection->conn->channels ? connection->conn->channels : "", " ", 2);
        if (!parts[0])
          {
             g_strfreev(parts);
             continue;
          }
        names = g_strsplit(parts[0], ",", -1);
        keys = parts[1] ? g_strsplit(parts[1], ",", -1) : NULL;
        key_count = keys ? g_strv_length(keys) : 0;
        channels = g_string_new(NULL);
        passwords = g_string_new(NULL);
        for (i = 0; names[i]; i++)
          {
             if (!*names[i] || !g_ascii_strcasecmp(names[i], target)) continue;
             if (channels->len)
               {
                  g_string_append_c(channels, ',');
                  g_string_append_c(passwords, ',');
               }
             g_string_append(channels, names[i]);
             g_string_append(passwords, i < key_count ? keys[i] : "x");
          }
        if (keys && channels->len) g_string_append_printf(channels, " %s", passwords->str);
        g_free(connection->conn->channels);
        connection->conn->channels = g_string_free(channels, FALSE);
        g_string_free(passwords, TRUE);
        g_strfreev(names);
        g_strfreev(keys);
        g_strfreev(parts);
     }
}

static void
_close_buffer(const char *server_tag, const char *target)
{
   SERVER_REC *s;
   CHANNEL_REC *channel;
   QUERY_REC *query;

   if (!target || !_clean_field(target)) return;
   if (!*target)
     {
        _close_server(server_tag);
        return;
     }

   s = server_find_tag(server_tag ? server_tag : "");
   if (!s || !s->connected) _connection_forget_channel(server_tag, target);

   if (s && server_ischannel(s, target))
     {
        channel = channel_find(s, target);

        if (s->connected && channel && channel->joined)
          {
             irc_send_cmdv(IRC_SERVER(s), "PART %s :Leaving", target);

             return; /* Remove the row when the server confirms our PART. */
          }

        if (channel) channel_destroy(channel);
     }
   else
     {
        query = s ? query_find(s, target) : NULL;

        if (query) query_destroy(query);
     }
   _emit_event(ELARO_ENGINE_BUFFER_CLOSED, server_tag, target, "");
}

static void
_input_command(Elaro_Engine_Command *c)
{
   char *body, *echo, *lower, *command_signal;
   const char *message;
   SERVER_REC *s;
   char *cmd, *args;
   CHANNEL_REC *channel;
   QUERY_REC *query;

   if (!_clean_field(c->text) || !_clean_field(c->target)) return;
   s = server_find_tag(c->server ? c->server : "");
   if (!g_ascii_strncasecmp(c->text, "/quit", 5)
       && (!c->text[5] || c->text[5] == ' ' || c->text[5] == '\t'))
     {
        _connection_cancel(c->server);
        if (!s) s = server_find_lookup_tag(c->server ? c->server : "");
        if (s)
          {
             if (s->connected)
               {
                  const char *reason = c->text + 5;
                  char *quit;

                  while (*reason == ' ' || *reason == '\t') reason++;
                  quit = g_strdup_printf("QUIT :%s", *reason ? reason : "Leaving");
                  irc_send_cmd_now(IRC_SERVER(s), quit);
                  g_free(quit);
               }
             s->connection_lost = FALSE;
             server_disconnect(s);
          }
        _emit_event(ELARO_ENGINE_LINE, c->server, "", "Auto-reconnect cancelled");
        return;
     }

   if (!s || !s->connected)
     {
        _emit_event(ELARO_ENGINE_LINE, c->server, c->target, "Connect to a server first");

        return;
     }

   if (c->text[0] == '/' && c->text[1] != '/')
     {
        /* Small explicit command surface; all execution stays on this thread. */
        cmd = g_strdup(c->text + 1);
        args = strchr(cmd, ' ');

        if (args) *args++ = 0;
        else args = cmd + strlen(cmd);

        if (!g_ascii_strcasecmp(cmd, "list")) _emit_event(ELARO_ENGINE_LIST_OPEN, s->tag, "", "");
        else if (!g_ascii_strcasecmp(cmd, "close")) _close_buffer(s->tag, c->target);
        else if (!g_ascii_strcasecmp(cmd, "query")) _open_query(s->tag, args);
        else if (!g_ascii_strcasecmp(cmd, "away")) signal_emit("command away", 3, args, s, NULL);
        else if (!g_ascii_strcasecmp(cmd, "join") && *args) s->channels_join(s, args, FALSE);
        else if (!g_ascii_strcasecmp(cmd, "part") && *c->target)
           irc_send_cmdv(IRC_SERVER(s), "PART %s :%s", c->target, args);
        else if (!g_ascii_strcasecmp(cmd, "me"))
          {
             if (!*c->target || !*args)
                _emit_event(ELARO_ENGINE_LINE, s->tag, c->target,
                            _("Usage: /me action (in a channel or private conversation)"));
             else
               {
                  if (!server_ischannel(s, c->target) && !query_find(s, c->target))
                     irc_query_create(s->tag, c->target, FALSE);
                  body = g_strdup_printf("\001ACTION %s\001", args);
                  s->send_message(s, c->target, body,
                                  server_ischannel(s, c->target) ? SEND_TARGET_CHANNEL
                                                                 : SEND_TARGET_NICK);
                  g_free(body);
                  echo = g_strdup_printf("* %s %s", s->nick, args);
                  _emit_event(ELARO_ENGINE_LINE, s->tag, c->target, echo);
                  _presence_known(s, c->target);
                  g_free(echo);
               }
          }
        else if (!g_ascii_strcasecmp(cmd, "nick"))
          {
             if (*args) signal_emit("command nick", 3, args, s, NULL);
             else _emit_event(ELARO_ENGINE_LINE, s->tag, c->target, "Usage: /nick nickname");
          }
        else if (!g_ascii_strcasecmp(cmd, "msg") && strchr(args, ' '))
          {

             body = strchr(args, ' ');
             *body++ = 0;

             if (*args && *body)
               {

                  if (!server_ischannel(s, args) && !query_find(s, args))
                     irc_query_create(s->tag, args, FALSE);
                  s->send_message(s, args, body, SEND_TARGET_NICK);
                  echo = g_strdup_printf("<%s> %s", s->nick, body);
                  _emit_event(ELARO_ENGINE_LINE, s->tag, args, echo);
                  _presence_known(s, args);
                  g_free(echo);
               }
          }
        else if (!g_ascii_strcasecmp(cmd, "mode") || !g_ascii_strcasecmp(cmd, "topic")
                 || !g_ascii_strcasecmp(cmd, "kick") || !g_ascii_strcasecmp(cmd, "ban")
                 || !g_ascii_strcasecmp(cmd, "unban") || !g_ascii_strcasecmp(cmd, "kickban")
                 || !g_ascii_strcasecmp(cmd, "op") || !g_ascii_strcasecmp(cmd, "deop")
                 || !g_ascii_strcasecmp(cmd, "voice") || !g_ascii_strcasecmp(cmd, "devoice"))
          {

             lower = g_ascii_strdown(cmd, -1);
             command_signal = g_strconcat("command ", lower, NULL);
             /* Supply the real channel item, as the original frontend did. */
             channel = channel_find(s, c->target);
             signal_emit(command_signal, 3, args, s, channel);
             g_free(command_signal);
             g_free(lower);
          }
        else if (!g_ascii_strcasecmp(cmd, "whois") || !g_ascii_strcasecmp(cmd, "whowas")
                 || !g_ascii_strcasecmp(cmd, "who") || !g_ascii_strcasecmp(cmd, "userhost")
                 || !g_ascii_strcasecmp(cmd, "ctcp") || !g_ascii_strcasecmp(cmd, "ping"))
          {

             lower = g_ascii_strdown(cmd, -1);
             command_signal = g_strconcat("command ", lower, NULL);
             query = *c->target && !server_ischannel(s, c->target) ? query_find(s, c->target)
                                                                   : NULL;

             if (!query && *c->target && !server_ischannel(s, c->target))
                query = irc_query_create(s->tag, c->target, FALSE);
             signal_emit(command_signal, 3, args, s, query);
             echo = g_strdup_printf("Query sent: /%s %s", lower, args);
             _emit_event(ELARO_ENGINE_LINE, s->tag, c->target, echo);
             g_free(echo);
             g_free(command_signal);
             g_free(lower);
          }
        else
           _emit_event(ELARO_ENGINE_LINE, s->tag, c->target,
                       "Commands: /join #channel, /part [reason], /nick name, /msg nick text, "
                       "/query nick, /me action, /away [-one|-all] [reason], /list, /quit [message], "
                       "/whois, /whowas, /who, /userhost, /ctcp, /ping, "
                       "/mode [channel|*] [+|-modes [parameters]], /topic [-delete] [topic], "
                       "/kick nick [reason], /ban nick|mask, /unban "
                       "mask, /kickban nick [reason], /op nick, /deop nick, /voice nick, "
                       "/devoice nick");
        g_free(cmd);
     }
   else if (*c->target)
     {

        if (!server_ischannel(s, c->target) && !query_find(s, c->target))
           irc_query_create(s->tag, c->target, FALSE);
        message = c->text + (c->text[0] == '/' && c->text[1] == '/');
        s->send_message(s, c->target, message,
                        server_ischannel(s, c->target) ? SEND_TARGET_CHANNEL : SEND_TARGET_NICK);
        echo = g_strdup_printf("<%s> %s", s->nick, message);
        _emit_event(ELARO_ENGINE_LINE, s->tag, c->target, echo);
        _presence_known(s, c->target);
        g_free(echo);
     }
   else _emit_event(ELARO_ENGINE_LINE, s->tag, "", "Use /join #channel or /msg nick text");
}

static gboolean
_dispatch(gint fd, GIOCondition condition, gpointer data)
{
   Elaro_Engine *e;
   Elaro_Engine_Command *c;
   int i;
   gboolean stop;
   SERVER_REC *server;
   CHANNEL_REC *channel;

   e = data;

   /* Bound each batch so network sources cannot be starved by UI input. */

   for (i = 0; i < 64 && (c = _queue_pop(e->commands, e->command_pipe)); i++)
     {
        stop = c->type == ELARO_ENGINE_STOP;
        switch (c->type)
          {
          case ELARO_ENGINE_CONNECT:
             _connect_command(c);
             break;
          case ELARO_ENGINE_INPUT:
             _input_command(c);
             break;
          case ELARO_ENGINE_QUERY:
             _open_query(c->server, c->target);
             break;
          case ELARO_ENGINE_CLOSE:
             _close_buffer(c->server, c->target);
             break;
          case ELARO_ENGINE_USER_INFO:
             server = server_find_tag(c->server ? c->server : "");
             if (server && server->connected && c->target && *c->target
                 && !strpbrk(c->target, " ,:\t\r\n"))
                irc_send_cmdv(IRC_SERVER(server), "WHOIS %s %s", c->target, c->target);
             break;
          case ELARO_ENGINE_SELECT:
             g_free(e->selected_server);
             g_free(e->selected_target);
             g_free(e->last_nicks);
             e->selected_server = g_strdup(c->server);
             e->selected_target = g_strdup(c->target);
             e->last_nicks = NULL;
             server = server_find_tag(c->server);
             channel = server ? channel_find(server, c->target) : NULL;

             if (channel) _topic_changed(channel);
             break;
          case ELARO_ENGINE_TRANSFER_SEND:
          case ELARO_ENGINE_TRANSFER_ACCEPT:
          case ELARO_ENGINE_TRANSFER_CANCEL:
             elaro_dcc_command(c);
             break;
          case ELARO_ENGINE_LIST:
          case ELARO_ENGINE_LIST_CANCEL:
             elaro_list_command(c);
             break;
          case ELARO_ENGINE_BANS:
          case ELARO_ENGINE_UNBAN:
             elaro_bans_command(c);
             break;
          case ELARO_ENGINE_STOP:
             g_main_loop_quit(e->loop);
             break;
          }
        elaro_engine_command_free(c);

        if (stop) return G_SOURCE_REMOVE;
     }

   return G_SOURCE_CONTINUE;
}

static gint
_compare_nicks(gconstpointer left, gconstpointer right)
{
   const NICK_REC *a, *b;
   int a_rank, b_rank, order;

   a = left;
   b = right;
   a_rank = a->op ? 0 : a->voice ? 1 : 2;
   b_rank = b->op ? 0 : b->voice ? 1 : 2;

   if (a_rank != b_rank) return a_rank - b_rank;

   order = g_ascii_strcasecmp(a->nick, b->nick);

   return order ? order : strcmp(a->nick, b->nick);
}

static gboolean
_snapshot(gpointer data)
{
   Elaro_Engine *e;
   SERVER_REC *s;
   CHANNEL_REC *ch;
   GString *names, *state;
   Elaro_Engine_Event *event;
   GArray *users;
   GSList *nicks, *p;
   NICK_REC *n;

   e = data;

   if (!e->selected_server) return G_SOURCE_CONTINUE;

   s = server_find_tag(e->selected_server);
   ch = s ? channel_find(s, e->selected_target) : NULL;
   names = g_string_new("");
   state = g_string_new("");
   users = g_array_new(FALSE, FALSE, sizeof(Elaro_Engine_User));

   if (ch)
     {
        nicks = g_slist_sort(nicklist_getnicks(ch), _compare_nicks);

        for (p = nicks; p; p = p->next)
          {
             n = p->data;
             Whois_Presence *pending = _whois_presence_find(s, n->nick);
             gboolean away = pending ? pending->previous : n->gone;
             /* Borrow nicknames while comparing; copy only for a new event. */
             Elaro_Engine_User user = { n->nick, n->op ? '@' : n->voice ? '+' : 0, away };
             g_array_append_val(users, user);
             g_string_append_printf(state, "%c%s\n", away ? 'G' : 'H', n->nick);
             g_string_append_printf(names, "%s%s\n", n->op ? "@" : n->voice ? "+" : "", n->nick);
          }
        g_slist_free(nicks);
     }

   g_string_append(state, names->str);
   if (g_strcmp0(state->str, e->last_nicks))
     {
        event = g_new0(Elaro_Engine_Event, 1);
        event->type = ELARO_ENGINE_NICKS;
        event->server = g_strdup(e->selected_server);
        event->target = g_strdup(e->selected_target);
        event->text = g_string_free(names, FALSE);
        for (guint i = 0; i < users->len; i++)
          {
             Elaro_Engine_User *user = &g_array_index(users, Elaro_Engine_User, i);
             user->nick = g_utf8_make_valid(user->nick, -1);
          }
        event->user_count = users->len;
        event->users = (Elaro_Engine_User *)g_array_free(users, FALSE);
        elaro_engine_event_push(event);
        g_free(e->last_nicks);
        e->last_nicks = g_string_free(state, FALSE);
     }
   else
     {
        g_array_free(users, TRUE);
        g_string_free(names, TRUE);
        g_string_free(state, TRUE);
     }

   return G_SOURCE_CONTINUE;
}

static gpointer
_run(gpointer data)
{
   Elaro_Engine *e;
   guint dispatch_id, snapshot_id;
   SERVER_REC *s;

   e = active = data;
   e->whois_presence = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
   core_preinit(e->binary);
   elaro_gui = ELARO_GUI_EFL;
   core_init();
   irc_init();
   elaro_dcc_init();
   elaro_ctcp_init();
   elaro_bans_init();
   elaro_list_init();
   signal_add("server connected", (SIGNAL_FUNC)_connected);
   signal_add("event connected", (SIGNAL_FUNC)_registered);
   signal_add_first("server disconnected", (SIGNAL_FUNC)_disconnected);
   signal_add_first("server connect failed", (SIGNAL_FUNC)_failed);
   signal_add("server sasl failure", (SIGNAL_FUNC)_auth_failed);
   signal_add("gui dialog", (SIGNAL_FUNC)_dialog);
   signal_add("channel topic changed", (SIGNAL_FUNC)_topic_changed);
   signal_add("event 311", (SIGNAL_FUNC)_user_details);
   signal_add_last("nicklist gone changed", (SIGNAL_FUNC)_nick_gone_changed);
   signal_add_last("event away", (SIGNAL_FUNC)_away_notify);
   signal_add_full(MODULE_NAME, SIGNAL_PRIORITY_HIGH - 1, "event 311",
                   (SIGNAL_FUNC)_whois_presence_begin, NULL);
   signal_add_last("event 301", (SIGNAL_FUNC)_whois_presence_away);
   signal_add_last("whois away", (SIGNAL_FUNC)_whois_presence_away);
   signal_add_last("event 318", (SIGNAL_FUNC)_whois_presence_end);
   signal_add_last("whois end", (SIGNAL_FUNC)_whois_presence_end);
   signal_add("ctcp action", (SIGNAL_FUNC)_action);
   signal_add_first("server event", (SIGNAL_FUNC)_membership_event);
   signal_add_last("server event", (SIGNAL_FUNC)_server_event);
   signal_emit("elaro init finished", 0);
   _emit_event(ELARO_ENGINE_LINE, NULL, "", "Ready — open Connection to connect");
   e->loop = g_main_loop_new(NULL, FALSE);
   dispatch_id = g_unix_fd_add(e->command_pipe[0], G_IO_IN, _dispatch, e);
   snapshot_id = g_timeout_add(300, _snapshot, e);
   g_main_loop_run(e->loop);
   /* _dispatch removed itself on STOP. */
   (void)dispatch_id;
   g_source_remove(snapshot_id);
   for (GSList *node = e->connections; node; node = node->next)
     {
        Connection *connection = node->data;
        connection->enabled = FALSE;
        _connection_timers_cancel(connection);
     }

   while (servers)
     {
        s = servers->data;
        irc_send_cmd_now(IRC_SERVER(s), "QUIT :Client closed");
        server_disconnect(s);
     }

   while (lookup_servers)
      server_disconnect(lookup_servers->data);
   signal_remove("server connected", (SIGNAL_FUNC)_connected);
   signal_remove("event connected", (SIGNAL_FUNC)_registered);
   signal_remove("server disconnected", (SIGNAL_FUNC)_disconnected);
   signal_remove("server connect failed", (SIGNAL_FUNC)_failed);
   signal_remove("server sasl failure", (SIGNAL_FUNC)_auth_failed);
   signal_remove("gui dialog", (SIGNAL_FUNC)_dialog);
   signal_remove("channel topic changed", (SIGNAL_FUNC)_topic_changed);
   signal_remove("event 311", (SIGNAL_FUNC)_user_details);
   signal_remove("nicklist gone changed", (SIGNAL_FUNC)_nick_gone_changed);
   signal_remove("event away", (SIGNAL_FUNC)_away_notify);
   signal_remove("event 311", (SIGNAL_FUNC)_whois_presence_begin);
   signal_remove("event 301", (SIGNAL_FUNC)_whois_presence_away);
   signal_remove("whois away", (SIGNAL_FUNC)_whois_presence_away);
   signal_remove("event 318", (SIGNAL_FUNC)_whois_presence_end);
   signal_remove("whois end", (SIGNAL_FUNC)_whois_presence_end);
   signal_remove("ctcp action", (SIGNAL_FUNC)_action);
   signal_remove("server event", (SIGNAL_FUNC)_membership_event);
   signal_remove("server event", (SIGNAL_FUNC)_server_event);
   for (GSList *node = e->connections; node; node = node->next)
     {
        Connection *connection = node->data;
        server_connect_unref(connection->conn);
        g_free(connection->profile_nick);
        g_free(connection->tag);
        g_free(connection);
     }
   g_slist_free(e->connections);
   e->connections = NULL;
   /* Keep observers alive while the engine closes outstanding transfers. */
   elaro_list_deinit();
   elaro_bans_deinit();
   elaro_ctcp_deinit();
   irc_deinit();
   elaro_dcc_shutdown();
   core_deinit();
   g_main_loop_unref(e->loop);
   g_hash_table_destroy(e->whois_presence);
   active = NULL;

   return NULL;
}

Elaro_Engine *
elaro_engine_start(const char *binary)
{
   Elaro_Engine *e;

   e = g_new0(Elaro_Engine, 1);
   if (!_wakeup_pipe(e->command_pipe))
     {
        g_free(e);
        return NULL;
     }
   if (!_wakeup_pipe(e->event_pipe))
     {
        close(e->command_pipe[0]);
        close(e->command_pipe[1]);
        g_free(e);
        return NULL;
     }
   e->commands = g_async_queue_new_full((GDestroyNotify)elaro_engine_command_free);
   e->events = g_async_queue_new_full((GDestroyNotify)elaro_engine_event_free);
   e->binary = g_strdup(binary);
   e->thread = g_thread_new("irc-engine", _run, e);

   return e;
}

void
elaro_engine_submit(Elaro_Engine *e, Elaro_Engine_Command *c)
{
   _queue_push(e->commands, e->command_pipe, c);
}

Elaro_Engine_Event *
elaro_engine_poll(Elaro_Engine *e)
{
   return _queue_pop(e->events, e->event_pipe);
}

int
elaro_engine_event_fd(const Elaro_Engine *e)
{
   return e->event_pipe[0];
}

void
elaro_engine_stop(Elaro_Engine *e)
{
   Elaro_Engine_Command *c;

   c = g_new0(Elaro_Engine_Command, 1);
   c->type = ELARO_ENGINE_STOP;
   elaro_engine_submit(e, c);
   g_thread_join(e->thread);
   g_async_queue_unref(e->commands);
   g_async_queue_unref(e->events);
   close(e->command_pipe[0]);
   close(e->command_pipe[1]);
   close(e->event_pipe[0]);
   close(e->event_pipe[1]);
   g_free(e->binary);
   g_free(e->selected_server);
   g_free(e->selected_target);
   g_free(e->last_nicks);
   g_free(e);
}
