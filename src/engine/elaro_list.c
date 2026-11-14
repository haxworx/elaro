#include <src/common.h>
#define MODULE_NAME "fe-efl"
#include <src/core/servers.h>
#include <src/core/signals.h>
#include <src/irc/core/irc.h>
#include <src/irc/core/irc-servers.h>
#include <src/fe-efl/elaro_intl.h>
#include "elaro_list.h"
#include "elaro_engine_private.h"

typedef struct
{
   Elaro_Engine_Channel channel;
   char *sort_key;
} Row;

typedef struct
{
   char *server;
   gboolean cancelled, expired, queued;
   unsigned int queued_id;
   unsigned int id;
   gint64 deadline;
   GPtrArray *rows;
} Request;

static GSList *requests;
static guint timer;
static void
_entry(IRC_SERVER_REC *s, const char *data);
static void
_end(IRC_SERVER_REC *s, const char *data);
static void
_error(IRC_SERVER_REC *s, const char *data);

static Elaro_Engine_Event *
_event(const Request *r, Elaro_Engine_Event_Type type, const char *text)
{
   Elaro_Engine_Event *e = g_new0(Elaro_Engine_Event, 1);
   e->type = type;
   e->request_id = r->id;
   e->server = g_strdup(r->server);
   e->target = g_strdup("");
   e->text = g_utf8_make_valid(text ? text : "", -1);
   return e;
}

static void
_row_free(gpointer data)
{
   Row *row = data;
   g_free(row->channel.name);
   g_free(row->channel.topic);
   g_free(row->sort_key);
   g_free(row);
}

static void
_free(Request *r)
{
   requests = g_slist_remove(requests, r);
   g_ptr_array_free(r->rows, TRUE);
   g_free(r->server);
   g_free(r);
}

static Request *
_find(const char *server)
{
   GSList *node;
   for (node = requests; node; node = node->next)
     {
        Request *r = node->data;
        if (!g_strcmp0(r->server, server)) return r;
     }
   return NULL;
}

static void
_discard(Request *r)
{
   r->cancelled = TRUE;
   g_ptr_array_free(r->rows, TRUE);
   r->rows = g_ptr_array_new_with_free_func(_row_free);
}

/* IRC LIST has no request IDs. Drain a cancelled response before starting
 * another request on that server; unrelated IRC traffic remains unaffected. */
static void
_finish(Request *r)
{
   gboolean queued = r->queued;
   Elaro_Engine_Command next
       = { .type = ELARO_ENGINE_LIST, .server = g_strdup(r->server), .request_id = r->queued_id };
   _free(r);
   if (queued) elaro_list_command(&next);
   g_free(next.server);
}

static void
_entry(IRC_SERVER_REC *s, const char *data)
{
   Request *r = _find(s->tag);
   char *params, *name, *users, *topic, *folded;
   Row *row;
   if (!r) return;
   r->deadline = g_get_monotonic_time() + 120 * G_TIME_SPAN_SECOND;
   if (r->cancelled) return;
   params = event_get_params(data, 4 | PARAM_FLAG_GETREST, NULL, &name, &users, &topic);
   if (server_ischannel(SERVER(s), name) && !strpbrk(name, " ,:\t\r\n"))
     {
        row = g_new0(Row, 1);
        row->channel.name = g_utf8_make_valid(name, -1);
        row->channel.topic = g_utf8_make_valid(topic, -1);
        row->channel.users = (unsigned int)MIN(g_ascii_strtoull(users, NULL, 10), G_MAXUINT);
        folded = g_utf8_casefold(row->channel.name, -1);
        row->sort_key = g_utf8_collate_key_for_filename(folded, -1);
        g_free(folded);
        g_ptr_array_add(r->rows, row);
     }
   g_free(params);
}

static gint
_compare(gconstpointer a, gconstpointer b)
{
   const Row *left = *(Row *const *)a, *right = *(Row *const *)b;
   int order = strcmp(left->sort_key, right->sort_key);
   return order ? order : strcmp(left->channel.name, right->channel.name);
}

static void
_end(IRC_SERVER_REC *s, const char *data)
{
   Request *r = _find(s->tag);
   guint offset, i;
   if (!r) return;
   if (r->cancelled)
     {
        _finish(r);
        return;
     }
   /* Sorting and parsing stay on the worker. The UI receives small sorted batches. */
   g_ptr_array_sort(r->rows, _compare);
   for (offset = 0; offset < r->rows->len; offset += 128)
     {
        Elaro_Engine_Event *e = _event(r, ELARO_ENGINE_LIST_BATCH, "");
        e->channel_count = MIN(128, r->rows->len - offset);
        e->channels = g_new0(Elaro_Engine_Channel, e->channel_count);
        for (i = 0; i < e->channel_count; i++)
          {
             Row *row = g_ptr_array_index(r->rows, offset + i);
             e->channels[i] = row->channel;
             row->channel.name = row->channel.topic = NULL;
          }
        elaro_engine_event_push(e);
     }
   elaro_engine_event_push(_event(r, ELARO_ENGINE_LIST_END, ""));
   _finish(r);
}

static void
_error(IRC_SERVER_REC *s, const char *data)
{
   Request *r = _find(s->tag);
   char *params, *reason;
   if (!r) return;
   params = event_get_params(data, 2 | PARAM_FLAG_GETREST, NULL, &reason);
   if (!r->cancelled)
      elaro_engine_event_push(_event(r, ELARO_ENGINE_LIST_ERROR,
                                     *reason ? reason : _("Could not retrieve the channel list.")));
   g_free(params);
   _finish(r);
}

static void
_truncated(IRC_SERVER_REC *s, const char *data)
{
   Request *r = _find(s->tag);
   char *params, *reason;
   if (!r) return;
   params = event_get_params(data, 2 | PARAM_FLAG_GETREST, NULL, &reason);
   if (!r->cancelled) elaro_engine_event_push(_event(r, ELARO_ENGINE_LIST_ERROR, reason));
   g_free(params);
   _discard(r);
}

static void
_disconnected(IRC_SERVER_REC *s)
{
   Request *r = _find(s->tag);
   if (!r) return;
   if (!r->cancelled)
      elaro_engine_event_push(_event(r, ELARO_ENGINE_LIST_ERROR, _("The server disconnected.")));
   if (r->queued)
     {
        Request waiting = { .server = r->server, .id = r->queued_id };
        elaro_engine_event_push(
            _event(&waiting, ELARO_ENGINE_LIST_ERROR, _("The server disconnected.")));
     }
   _free(r);
}

static void
_command_error(IRC_SERVER_REC *s, const char *data)
{
   char *params, *command;
   params = event_get_params(data, 2, NULL, &command);
   if (!g_ascii_strcasecmp(command, "LIST")) _error(s, data);
   g_free(params);
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
        if (!s || !s->connected)
          {
             if (!r->cancelled)
                elaro_engine_event_push(
                    _event(r, ELARO_ENGINE_LIST_ERROR, _("The server disconnected.")));
             _finish(r);
          }
        else if (!r->expired && g_get_monotonic_time() >= r->deadline)
          {
             if (!r->cancelled)
                elaro_engine_event_push(
                    _event(r, ELARO_ENGINE_LIST_ERROR, _("The channel list request timed out.")));
             if (r->queued)
               {
                  Request waiting = { .server = r->server, .id = r->queued_id };
                  elaro_engine_event_push(_event(&waiting, ELARO_ENGINE_LIST_ERROR,
                                                 _("The channel list request timed out.")));
               }
             _discard(r);
             r->queued = FALSE;
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
elaro_list_command(const Elaro_Engine_Command *c)
{
   SERVER_REC *server = server_find_tag(c->server ? c->server : "");
   Request *r;
   r = _find(c->server);
   if (r)
     {
        if (c->type == ELARO_ENGINE_LIST_CANCEL)
          {
             if (r->id == c->request_id) _discard(r);
             if (r->queued && r->queued_id == c->request_id) r->queued = FALSE;
             return;
          }
        if (r->cancelled && !r->expired && !r->queued)
          {
             r->queued = TRUE;
             r->queued_id = c->request_id;
             return;
          }
        Request temporary = { .server = c->server, .id = c->request_id };
        elaro_engine_event_push(
            _event(&temporary, ELARO_ENGINE_LIST_ERROR,
                   _("The previous channel list request has not ended. Try Refresh later.")));
        return;
     }
   if (c->type == ELARO_ENGINE_LIST_CANCEL) return;
   if (!server || !server->connected)
     {
        Request temporary = { .server = c->server ? c->server : "Status", .id = c->request_id };
        elaro_engine_event_push(
            _event(&temporary, ELARO_ENGINE_LIST_ERROR, _("Connect to a server first.")));
        return;
     }
   r = g_new0(Request, 1);
   r->server = g_strdup(server->tag);
   r->id = c->request_id;
   r->rows = g_ptr_array_new_with_free_func(_row_free);
   r->deadline = g_get_monotonic_time() + 120 * G_TIME_SPAN_SECOND;
   requests = g_slist_prepend(requests, r);
   if (!timer) timer = g_timeout_add_seconds(1, _tick, NULL);
   irc_send_cmd(IRC_SERVER(server), "LIST");
}

void
elaro_list_init(void)
{
   signal_add("event 322", (SIGNAL_FUNC)_entry);
   signal_add("event 323", (SIGNAL_FUNC)_end);
   signal_add("event 263", (SIGNAL_FUNC)_command_error);
   signal_add("event 461", (SIGNAL_FUNC)_command_error);
   signal_add("event 416", (SIGNAL_FUNC)_truncated);
   signal_add("event 421", (SIGNAL_FUNC)_command_error);
   signal_add("server disconnected", (SIGNAL_FUNC)_disconnected);
   signal_add("event 481", (SIGNAL_FUNC)_error);
}

void
elaro_list_deinit(void)
{
   signal_remove("event 322", (SIGNAL_FUNC)_entry);
   signal_remove("event 323", (SIGNAL_FUNC)_end);
   signal_remove("event 263", (SIGNAL_FUNC)_command_error);
   signal_remove("event 461", (SIGNAL_FUNC)_command_error);
   signal_remove("event 416", (SIGNAL_FUNC)_truncated);
   signal_remove("event 421", (SIGNAL_FUNC)_command_error);
   signal_remove("server disconnected", (SIGNAL_FUNC)_disconnected);
   signal_remove("event 481", (SIGNAL_FUNC)_error);
   if (timer) g_source_remove(timer);
   timer = 0;
   while (requests)
      _free(requests->data);
}
