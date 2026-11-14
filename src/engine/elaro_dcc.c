#include <src/common.h>
#define MODULE_NAME "fe-efl"
#include "elaro_dcc.h"
#include "elaro_engine_private.h"
#include <src/core/signals.h>
#include <src/core/servers.h>
#include <src/irc/core/irc.h>
#include <src/irc/core/irc-servers.h>
#include <src/irc/dcc/dcc-get.h>
#include <src/irc/dcc/dcc-send.h>

/* Accessed only by the engine worker, never by Elementary callbacks. */
static struct
{
   GHashTable *transfers;
   guint next_transfer;
   Elaro_Engine_Command *pending_send;
   gboolean send_started;
} state;

typedef struct
{
   guint id;
   DCC_REC *dcc;
   char *path;
   gboolean accepted, cancelled, failed, outgoing;
   gint64 last_update;
} Transfer;

static void
_transfer_free(gpointer data)
{
   Transfer *t;

   t = data;
   g_free(t->path);
   g_free(t);
}

static void
_transfer_event(Transfer *t, Elaro_Engine_Event_Type type, const char *text)
{
   Elaro_Engine_Event *e;

   e = g_new0(Elaro_Engine_Event, 1);
   e->type = type;
   e->transfer_id = t->id;
   e->server = g_strdup(t->dcc->servertag ? t->dcc->servertag : "");
   e->target = g_strdup(t->dcc->nick);
   e->text = g_utf8_make_valid(text ? text : "", -1);
   e->size = t->outgoing ? DCC_SEND(t->dcc)->size : DCC_GET(t->dcc)->size;
   e->transferred = t->dcc->transfd;
   elaro_engine_event_push(e);
}

static void
_transfer_request(DCC_REC *dcc)
{
   Transfer *t;

   if (!IS_DCC_GET(dcc)) return;

   t = g_new0(Transfer, 1);
   t->id = ++state.next_transfer;
   t->dcc = dcc;
   g_hash_table_insert(state.transfers, dcc, t);
   _transfer_event(t, ELARO_ENGINE_TRANSFER_OFFER, dcc->arg);
   /* Receiving a file always requires the frontend's explicit consent. */
   signal_stop();
}

static void
_transfer_send_request(DCC_REC *dcc)
{
   Elaro_Engine_Command *c;
   Transfer *t;

   c = state.pending_send;

   if (!IS_DCC_SEND(dcc) || !c) return;

   t = g_new0(Transfer, 1);
   t->id = c->transfer_id;
   t->dcc = dcc;
   t->accepted = TRUE;
   t->outgoing = TRUE;
   g_hash_table_insert(state.transfers, dcc, t);
   state.send_started = TRUE;
   _transfer_event(t, ELARO_ENGINE_TRANSFER_UPDATE, "Waiting for recipient");
}

static void
_transfer_send_error(Elaro_Engine_Command *c, const char *text)
{
   Elaro_Engine_Event *e;

   e = g_new0(Elaro_Engine_Event, 1);
   e->type = ELARO_ENGINE_TRANSFER_ERROR;
   e->transfer_id = c->transfer_id;
   e->server = g_strdup(c->server);
   e->target = g_strdup(c->target);
   e->text = g_strdup(text);
   elaro_engine_event_push(e);
}

static void
_transfer_send(Elaro_Engine_Command *c)
{
   SERVER_REC *server;
   GString *glob, *args;
   const char *p;

   server = server_find_tag(c->server ? c->server : "");

   if (!server || !server->connected)
     {
        _transfer_send_error(c, "Connect to the server first.");

        return;
     }

   if (!c->text || !g_path_is_absolute(c->text) || !g_file_test(c->text, G_FILE_TEST_IS_REGULAR)
       || strpbrk(c->text, "\r\n") || !c->target || !*c->target || strpbrk(c->target, " ,:\r\n"))
     {
        _transfer_send_error(c, "Choose a regular file using its full path.");

        return;
     }
   /* The command supports globs; escape them so this sends exactly one file. */
   glob = g_string_new("");

   for (p = c->text; *p; p++)
     {
        if (strchr("\\*?[]", *p)) g_string_append_c(glob, '\\');
        g_string_append_c(glob, *p);
     }
   args = g_string_new("-- ");
   g_string_append_printf(args, "%s \"", c->target);

   for (p = glob->str; *p; p++)
     {
        if (*p == '\\' || *p == '\"') g_string_append_c(args, '\\');
        g_string_append_c(args, *p);
     }
   g_string_append_c(args, '\"');
   state.pending_send = c;
   state.send_started = FALSE;
   signal_emit("command dcc send", 3, args->str, server, NULL);
   state.pending_send = NULL;

   if (!state.send_started)
      _transfer_send_error(
          c, "Could not offer the file. Check that it is readable and not already being sent.");
   g_string_free(args, TRUE);
   g_string_free(glob, TRUE);
}

static void
_transfer_path(GET_DCC_REC *dcc)
{
   Transfer *t;

   t = g_hash_table_lookup(state.transfers, dcc);

   if (!t || !t->path) return;

   g_free(dcc->file);
   dcc->file = g_strdup(t->path);
}

static void
_transfer_update(DCC_REC *dcc)
{
   Transfer *t;
   gint64 now;
   guint64 size;

   t = g_hash_table_lookup(state.transfers, dcc);

   if (!t) return;

   now = g_get_monotonic_time();
   size = t->outgoing ? DCC_SEND(dcc)->size : DCC_GET(dcc)->size;

   if (now - t->last_update < 100000 && dcc->transfd != size) return;

   t->last_update = now;
   _transfer_event(t, ELARO_ENGINE_TRANSFER_UPDATE,
                   t->outgoing ? "Sending file" : "Receiving file");
}

static void
_transfer_error(DCC_REC *dcc)
{
   Transfer *t;

   t = g_hash_table_lookup(state.transfers, dcc);

   if (t) t->failed = TRUE;
}

static void
_transfer_destroyed(DCC_REC *dcc)
{
   Transfer *t;
   gboolean complete;

   t = g_hash_table_lookup(state.transfers, dcc);

   if (!t) return;

   complete = t->accepted && !t->cancelled && !t->failed && dcc->starttime
              && (t->outgoing ? DCC_SEND(dcc)->gotalldata : dcc->transfd == DCC_GET(dcc)->size);
   _transfer_event(t, ELARO_ENGINE_TRANSFER_END,
                   t->cancelled  ? "Transfer cancelled"
                   : complete    ? "Transfer complete"
                   : t->accepted ? "Transfer failed or interrupted"
                                 : "Offer expired or withdrawn");
   g_hash_table_remove(state.transfers, dcc);
}

static void
_transfer_command(Elaro_Engine_Command *c)
{
   GHashTableIter iter;
   gpointer value;
   Transfer *t;
   char *dir;
   gboolean valid;
   GET_DCC_REC *dcc;

   g_hash_table_iter_init(&iter, state.transfers);

   while (g_hash_table_iter_next(&iter, NULL, &value))
     {
        t = value;

        if (t->id != c->transfer_id) continue;

        if (c->type == ELARO_ENGINE_TRANSFER_CANCEL)
          {
             t->cancelled = TRUE;

             if (t->accepted) dcc_close(t->dcc);
             else dcc_reject(t->dcc, t->dcc->server);

             return;
          }

        if (t->accepted) return;

        if (!c->text || !g_path_is_absolute(c->text) || !*c->text)
          {
             _transfer_event(t, ELARO_ENGINE_TRANSFER_ERROR,
                             "Choose an absolute destination filename.");

             return;
          }

        if (g_file_test(c->text, G_FILE_TEST_EXISTS)
            || g_file_test(c->text, G_FILE_TEST_IS_SYMLINK))
          {
             _transfer_event(t, ELARO_ENGINE_TRANSFER_ERROR,
                             "That file already exists. Choose a new filename.");

             return;
          }
        dir = g_path_get_dirname(c->text);
        valid = g_file_test(dir, G_FILE_TEST_IS_DIR);
        g_free(dir);

        if (!valid)
          {
             _transfer_event(t, ELARO_ENGINE_TRANSFER_ERROR,
                             "The destination directory does not exist.");

             return;
          }
        t->path = g_strdup(c->text);
        t->accepted = TRUE;
        /* Retain the engine's collision handling rather than overwrite files. */
        dcc = DCC_GET(t->dcc);
        dcc->get_type = DCC_GET_RENAME;
        _transfer_event(t, ELARO_ENGINE_TRANSFER_UPDATE, "Connecting");

        if (dcc_is_passive(dcc)) dcc_get_passive(dcc);
        else dcc_get_connect(dcc);

        return;
     }
}

void
elaro_dcc_init(void)
{
   state.transfers = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, _transfer_free);
   signal_add_first("dcc request", (SIGNAL_FUNC)_transfer_request);
   signal_add("dcc request send", (SIGNAL_FUNC)_transfer_send_request);
   signal_add("dcc get receive", (SIGNAL_FUNC)_transfer_path);
   signal_add("dcc connected", (SIGNAL_FUNC)_transfer_update);
   signal_add("dcc transfer update", (SIGNAL_FUNC)_transfer_update);
   signal_add("dcc error connect", (SIGNAL_FUNC)_transfer_error);
   signal_add("dcc error write", (SIGNAL_FUNC)_transfer_error);
   signal_add("dcc error file create", (SIGNAL_FUNC)_transfer_error);
   signal_add_last("dcc destroyed", (SIGNAL_FUNC)_transfer_destroyed);
}

void
elaro_dcc_shutdown(void)
{
   signal_remove("dcc request", (SIGNAL_FUNC)_transfer_request);
   signal_remove("dcc request send", (SIGNAL_FUNC)_transfer_send_request);
   signal_remove("dcc get receive", (SIGNAL_FUNC)_transfer_path);
   signal_remove("dcc connected", (SIGNAL_FUNC)_transfer_update);
   signal_remove("dcc transfer update", (SIGNAL_FUNC)_transfer_update);
   signal_remove("dcc error connect", (SIGNAL_FUNC)_transfer_error);
   signal_remove("dcc error write", (SIGNAL_FUNC)_transfer_error);
   signal_remove("dcc error file create", (SIGNAL_FUNC)_transfer_error);
   signal_remove("dcc destroyed", (SIGNAL_FUNC)_transfer_destroyed);
   g_hash_table_destroy(state.transfers);
   state.transfers = NULL;
}

void
elaro_dcc_command(Elaro_Engine_Command *command)
{
   if (command->type == ELARO_ENGINE_TRANSFER_SEND) _transfer_send(command);
   else _transfer_command(command);
}
