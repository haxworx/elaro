#include <src/common.h>
#define MODULE_NAME "fe-efl"
#include <src/core/servers.h>
#include <src/core/channels.h>
#include <src/core/signals.h>
#include <src/irc/core/irc.h>
#include <src/irc/core/irc-servers.h>
#include <src/irc/core/servers-redirect.h>
#include <src/fe-efl/elaro_intl.h>
#include "elaro_bans.h"
#include "elaro_engine_private.h"

typedef struct
{
   char *server, *channel;
   unsigned int id;
   gint64 deadline;
   gboolean expired;
} Request;

static GSList *requests;
static guint timer;
static void
_entry(IRC_SERVER_REC *s, const char *data);
static void
_end(IRC_SERVER_REC *s, const char *data);
static void
_error(IRC_SERVER_REC *s, const char *data);
static void
_failed(IRC_SERVER_REC *s, const char *command, const char *channel);

static void
_emit(const Request *r, Elaro_Engine_Event_Type type, const char *text)
{
   Elaro_Engine_Event *e = g_new0(Elaro_Engine_Event, 1);
   e->type = type;
   e->request_id = r->id;
   e->server = g_strdup(r->server ? r->server : "Status");
   e->target = g_strdup(r->channel ? r->channel : "");
   e->text = g_utf8_make_valid(text ? text : "", -1);
   elaro_engine_event_push(e);
}

static void
_free(Request *r)
{
   requests = g_slist_remove(requests, r);
   g_free(r->server);
   g_free(r->channel);
   g_free(r);
}

static Request *
_find(IRC_SERVER_REC *s, const char *channel)
{
   GSList *node;
   for (node = requests; node; node = node->next)
     {
        Request *r = node->data;
        if (!strcmp(r->server, s->tag) && !s->nick_comp_func(r->channel, channel)) return r;
     }
   return NULL;
}

static void
_reply(IRC_SERVER_REC *s, const char *data, gboolean entry, gboolean error)
{
   char *channel, *mask, *params;
   Request *r;
   params = event_get_params(data, 3 | PARAM_FLAG_GETREST, NULL, &channel, &mask);
   r = _find(s, channel);
   if (r)
     {
        if (entry)
          {
             char *end = strchr(mask, ' ');
             if (end) *end = 0;
             if (!r->expired) _emit(r, ELARO_ENGINE_BAN_ENTRY, mask);
          }
        else
          {
             if (!r->expired)
                _emit(r, error ? ELARO_ENGINE_BAN_ERROR : ELARO_ENGINE_BAN_END,
                      error ? mask : "");
             _free(r);
          }
     }
   g_free(params);
}

static void
_entry(IRC_SERVER_REC *s, const char *data)
{
   _reply(s, data, TRUE, FALSE);
}

static void
_end(IRC_SERVER_REC *s, const char *data)
{
   _reply(s, data, FALSE, FALSE);
}

static void
_error(IRC_SERVER_REC *s, const char *data)
{
   _reply(s, data, FALSE, TRUE);
}

static void
_failed(IRC_SERVER_REC *s, const char *command, const char *channel)
{
   Request *r = channel ? _find(s, channel) : NULL;
   if (r && !r->expired)
     {
        _emit(r, ELARO_ENGINE_BAN_ERROR, _("Could not retrieve the ban list."));
        /* Keep the channel occupied until the old response has drained. */
        r->expired = TRUE;
     }
}

/* If Irssi aborted redirection after interleaved traffic, drain its ordinary
 * end reply without letting it complete a newer request for this channel. */
static void
_drained(IRC_SERVER_REC *s, const char *data)
{
   char *params, *channel;
   Request *r;
   params = event_get_params(data, 2, NULL, &channel);
   r = _find(s, channel);
   if (r && r->expired) _free(r);
   g_free(params);
}

static void
_disconnected(IRC_SERVER_REC *s)
{
   GSList *node = requests, *next;
   for (; node; node = next)
     {
        Request *r = node->data;
        next = node->next;
        if (strcmp(r->server, s->tag)) continue;
        if (!r->expired) _emit(r, ELARO_ENGINE_BAN_ERROR, _("The server disconnected."));
        _free(r);
     }
}

static gboolean
_tick(gpointer data)
{
   GSList *node = requests, *next;
   for (; node; node = next)
     {
        Request *r = node->data;
        SERVER_REC *s = server_find_tag(r->server);
        next = node->next;
        if (!s || !s->connected || !channel_find(s, r->channel))
          {
             if (!r->expired)
                _emit(r, ELARO_ENGINE_BAN_ERROR,
                      _("The channel is unavailable or the ban list request timed out."));
             _free(r);
          }
        else if (!r->expired && g_get_monotonic_time() >= r->deadline)
          {
             _emit(r, ELARO_ENGINE_BAN_ERROR,
                   _("The channel is unavailable or the ban list request timed out."));
             r->expired = TRUE;
          }
     }
   if (!requests)
     {
        timer = 0;
        return G_SOURCE_REMOVE;
     }
   return G_SOURCE_CONTINUE;
}

void
elaro_bans_command(const Elaro_Engine_Command *c)
{
   SERVER_REC *server = server_find_tag(c->server ? c->server : "");
   IRC_SERVER_REC *s;
   Request temporary = { .server = c->server, .channel = c->target, .id = c->request_id };
   Request *r;
   if (!server || !server->connected || !c->target || !*c->target || strpbrk(c->target, " ,:\t\r\n")
       || !channel_find(server, c->target))
     {
        _emit(&temporary, ELARO_ENGINE_BAN_ERROR, _("Join the channel before viewing its bans."));
        return;
     }
   s = IRC_SERVER(server);
   if (_find(s, c->target))
     {
        _emit(&temporary, ELARO_ENGINE_BAN_ERROR,
              _("A ban list request is already in progress. Try Refresh shortly."));
        return;
     }
   if (c->type == ELARO_ENGINE_UNBAN)
     {
        if (!c->text || !*c->text || *c->text == ':' || strpbrk(c->text, " \t\r\n"))
          {
             _emit(&temporary, ELARO_ENGINE_BAN_ERROR, _("Select a valid ban mask."));
             return;
          }
        /* Send the selected mask literally; nick-to-mask expansion is inappropriate here. */
        irc_send_cmdv(s, "MODE %s -b %s", c->target, c->text);
     }
   r = g_new0(Request, 1);
   r->server = g_strdup(c->server);
   r->channel = g_strdup(c->target);
   r->id = c->request_id;
   r->deadline = g_get_monotonic_time() + 60 * G_TIME_SPAN_SECOND;
   requests = g_slist_prepend(requests, r);
   if (!timer) timer = g_timeout_add_seconds(1, _tick, NULL);
   server_redirect_event(s, "elaro bans", 1, c->target, TRUE, "elaro bans failed", "event 367",
                         "elaro bans entry", "event 368", "elaro bans end", "event 403",
                         "elaro bans error", "event 442", "elaro bans error", "event 482",
                         "elaro bans error", NULL);
   irc_send_cmdv(s, "MODE %s b", c->target);
}

void
elaro_bans_init(void)
{
   server_redirect_register("elaro bans", TRUE, 60, "event 367", 1, NULL, "event 368", 1,
                            "event 403", 1, "event 442", 1, "event 482", 1, NULL, NULL);
   /* Signal IDs are interned by Irssi until shutdown. Reuse this fixed set
    * rather than interning four new names on every refresh. */
   signal_add("elaro bans entry", (SIGNAL_FUNC)_entry);
   signal_add("elaro bans end", (SIGNAL_FUNC)_end);
   signal_add("elaro bans error", (SIGNAL_FUNC)_error);
   signal_add("elaro bans failed", (SIGNAL_FUNC)_failed);
   signal_add("event 368", (SIGNAL_FUNC)_drained);
   signal_add("server disconnected", (SIGNAL_FUNC)_disconnected);
}

void
elaro_bans_deinit(void)
{
   signal_remove("elaro bans entry", (SIGNAL_FUNC)_entry);
   signal_remove("elaro bans end", (SIGNAL_FUNC)_end);
   signal_remove("elaro bans error", (SIGNAL_FUNC)_error);
   signal_remove("elaro bans failed", (SIGNAL_FUNC)_failed);
   signal_remove("event 368", (SIGNAL_FUNC)_drained);
   signal_remove("server disconnected", (SIGNAL_FUNC)_disconnected);
   if (timer) g_source_remove(timer);
   timer = 0;
   while (requests)
      _free(requests->data);
}
