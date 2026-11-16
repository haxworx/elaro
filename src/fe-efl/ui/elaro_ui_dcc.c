#include "elaro_ui_private.h"
#include <Efreet.h>

typedef struct
{
   Elaro_Ui *app;
   unsigned int id;
   Eina_Bool ended, outgoing, started;
   char *server, *nick;
   Evas_Object *win, *path, *choose, *status, *progress, *accept, *cancel;
} Transfer_Window;

static void
_transfer_submit(Transfer_Window *t, Elaro_Engine_Command_Type type)
{
   Elaro_Engine_Command *c;

   c = calloc(1, sizeof(Elaro_Engine_Command));
   if (!c) goto allocation_failed;
   c->type = type;
   c->transfer_id = t->id;

   if (type == ELARO_ENGINE_TRANSFER_ACCEPT || type == ELARO_ENGINE_TRANSFER_SEND)
      c->text = elaro_ui_field_text_get(t->path);

   if (type == ELARO_ENGINE_TRANSFER_SEND)
     {
        c->server = strdup(t->server);
        c->target = strdup(t->nick);
     }

   if (((type == ELARO_ENGINE_TRANSFER_ACCEPT || type == ELARO_ENGINE_TRANSFER_SEND) && !c->text)
       || (type == ELARO_ENGINE_TRANSFER_SEND && !c->server)
       || (type == ELARO_ENGINE_TRANSFER_SEND && !c->target))
      goto allocation_failed;
   elaro_engine_submit(t->app->engine, c);
   return;

allocation_failed:
   elaro_engine_command_free(c);
}

static void
_transfer_window_deleted_cb(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   Transfer_Window *t;

   t = data;

   if (!t->ended) _transfer_submit(t, ELARO_ENGINE_TRANSFER_CANCEL);
   eina_hash_del_by_key(t->app->dcc.windows, &t->id);
   free(t->server);
   free(t->nick);
   free(t);
}

static void
_transfer_accept_cb(void *data, Evas_Object *obj, void *info)
{
   Transfer_Window *t;

   t = data;

   if (!t->ended)
     {
        t->started = EINA_TRUE;
        _transfer_submit(t,
                         t->outgoing ? ELARO_ENGINE_TRANSFER_SEND : ELARO_ENGINE_TRANSFER_ACCEPT);
        elm_object_disabled_set(t->accept, EINA_TRUE);
        elm_object_disabled_set(t->path, EINA_TRUE);
        elm_object_disabled_set(t->choose, EINA_TRUE);
     }
}

static void
_transfer_cancel_cb(void *data, Evas_Object *obj, void *info)
{
   Transfer_Window *t;

   t = data;

   if (t->ended || (t->outgoing && !t->started)) evas_object_del(t->win);
   else
     {
        _transfer_submit(t, ELARO_ENGINE_TRANSFER_CANCEL);
        elm_object_disabled_set(t->cancel, EINA_TRUE);
        elm_object_disabled_set(t->accept, EINA_TRUE);
     }
}

static void
_transfer_file_chosen_cb(void *data, Evas_Object *obj, void *info)
{
   Transfer_Window *t;
   char *safe;

   t = data;

   if (!info) return;

   safe = elm_entry_utf8_to_markup(info);
   elm_entry_entry_set(t->path, safe);
   free(safe);
}

static void
_transfer_window_add(Elaro_Ui *ui, unsigned int id, const char *server, const char *nick,
                     const char *filename, uint64_t size, Eina_Bool outgoing)
{
   Transfer_Window *t;
   Evas_Object *box, *details, *buttons;
   char *description, *safe, *path;
   const char *downloads;
   char *basename;

   t = calloc(1, sizeof(Transfer_Window));
   if (!t) return;
   t->app = ui;
   t->id = id;
   t->outgoing = outgoing;
   t->server = strdup(server);
   t->nick = strdup(nick);

   if (!t->server || !t->nick) goto allocation_failed;
   t->win = elm_win_add(ui->win, "elaro-transfer", ELM_WIN_DIALOG_BASIC);
   elm_win_title_set(t->win, t->outgoing ? _("Send file — Elaro") : _("Incoming file — Elaro"));
   elm_win_autodel_set(t->win, EINA_TRUE);

   evas_object_event_callback_add(t->win, EVAS_CALLBACK_DEL, _transfer_window_deleted_cb, t);
   eina_hash_add(ui->dcc.windows, &t->id, t);

   box = elaro_ui_window_content_add(t->win);
   elm_box_padding_set(box, 5, 6);

   details = elm_label_add(box);
   description = t->outgoing ? elaro_string_printf(_("Send a file to %s on %s"), nick, server)
                             : elaro_string_printf(_("%s on %s offers %s (%" PRIu64 " bytes)"),
                                                   nick, server, filename, size);
   safe = elm_entry_utf8_to_markup(description);
   elm_object_text_set(details, safe);
   free(safe);
   free(description);
   elm_label_line_wrap_set(details, ELM_WRAP_WORD);
   evas_object_size_hint_weight_set(details, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(details, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(box, details);
   evas_object_show(details);
   basename = strdup(ecore_file_file_get(filename));
   if (!basename) goto window_failed;

   if (!strcmp(basename, ".") || !strcmp(basename, "..") || !strcmp(basename, "/"))
     {
        free(basename);
        basename = strdup("download");
     }
   if (!basename) goto window_failed;
   downloads = efreet_download_dir_get();
   path = elaro_path_join(downloads && ecore_file_is_dir(downloads) ? downloads
                                                                    : eina_environment_home_get(),
                          basename, NULL);

   t->path = elaro_ui_field_add(box, t->outgoing ? _("File to send") : _("Save as"),
                                t->outgoing ? "" : path, EINA_FALSE);
   free(path);
   free(basename);

   t->choose = elm_fileselector_button_add(box);
   elm_object_text_set(t->choose, t->outgoing ? _("Choose file…") : _("Choose destination…"));
   elm_fileselector_is_save_set(t->choose, !t->outgoing);
   elm_fileselector_path_set(t->choose, downloads ? downloads : eina_environment_home_get());
   evas_object_smart_callback_add(t->choose, "file,chosen", _transfer_file_chosen_cb, t);
   elm_box_pack_end(box, t->choose);
   evas_object_show(t->choose);

   t->status = elm_label_add(box);
   elm_label_line_wrap_set(t->status, ELM_WRAP_WORD);
   evas_object_size_hint_weight_set(t->status, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(t->status, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(
       t->status, t->outgoing ? _("Choose a file, then Send to offer it to the recipient.")
                              : _("Accept this file? Existing files will not be overwritten."));
   elm_box_pack_end(box, t->status);
   evas_object_show(t->status);

   t->progress = elm_progressbar_add(box);
   evas_object_size_hint_weight_set(t->progress, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(t->progress, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(box, t->progress);
   evas_object_show(t->progress);

   buttons = elm_box_add(box);
   elm_box_horizontal_set(buttons, EINA_TRUE);

   t->accept = elm_button_add(buttons);
   elm_object_text_set(t->accept, t->outgoing ? _("Send") : _("Accept"));
   evas_object_smart_callback_add(t->accept, "clicked", _transfer_accept_cb, t);
   elm_box_pack_end(buttons, t->accept);
   evas_object_show(t->accept);

   t->cancel = elm_button_add(buttons);
   elm_object_text_set(t->cancel, t->outgoing ? _("Cancel") : _("Reject"));
   evas_object_smart_callback_add(t->cancel, "clicked", _transfer_cancel_cb, t);
   elm_box_pack_end(buttons, t->cancel);
   evas_object_show(t->cancel);
   elm_box_pack_end(box, buttons);
   evas_object_show(buttons);
   evas_object_resize(t->win, 420, 240);
   evas_object_show(t->win);
   return;

allocation_failed:
   free(t->server);
   free(t->nick);
   free(t);
   return;

window_failed:
   evas_object_del(t->win);
}

void
elaro_ui_dcc_event(Elaro_Ui *ui, const Elaro_Engine_Event *event)
{
   Transfer_Window *t;
   char *message, *safe;
   double fraction;

   t = eina_hash_find(ui->dcc.windows, &event->transfer_id);

   if (event->type == ELARO_ENGINE_TRANSFER_OFFER)
     {
        if (!t)
           _transfer_window_add(ui, event->transfer_id, event->server, event->target, event->text,
                                event->size, EINA_FALSE);

        return;
     }

   if (!t) return;

   message = elaro_string_printf(_("%s — %" PRIu64 " / %" PRIu64 " bytes"), event->text,
                                 event->transferred, event->size);
   safe = elm_entry_utf8_to_markup(message);
   elm_object_text_set(t->status, safe);
   free(safe);
   free(message);
   fraction
       = event->size ? (double)event->transferred / event->size
         : event->type == ELARO_ENGINE_TRANSFER_END && !strcmp(event->text, "Transfer complete")
             ? 1.0
             : 0.0;
   elm_progressbar_value_set(t->progress, ELARO_CLAMP(fraction, 0.0, 1.0));

   if (event->type == ELARO_ENGINE_TRANSFER_ERROR)
     {
        t->started = EINA_FALSE;
        elm_object_disabled_set(t->accept, EINA_FALSE);
        elm_object_disabled_set(t->path, EINA_FALSE);
        elm_object_disabled_set(t->choose, EINA_FALSE);
        elm_object_disabled_set(t->cancel, EINA_FALSE);
     }

   if (event->type == ELARO_ENGINE_TRANSFER_UPDATE || event->type == ELARO_ENGINE_TRANSFER_END)
     {
        elm_object_disabled_set(t->path, EINA_TRUE);
        elm_object_disabled_set(t->choose, EINA_TRUE);
        elm_object_disabled_set(t->accept, EINA_TRUE);
        elm_object_text_set(t->cancel, _("Cancel"));
     }

   if (event->type == ELARO_ENGINE_TRANSFER_END)
     {
        t->ended = EINA_TRUE;
        elm_object_disabled_set(t->cancel, EINA_FALSE);
        elm_object_text_set(t->cancel, _("Close"));
     }
}

void
elaro_ui_dcc_send_show(Elaro_Ui *ui, const char *server, const char *nick)
{
   unsigned int id;

   /* Outgoing compose windows own the upper half of the transfer ID space. */
   id = 0x80000000u | ++ui->dcc.next_send;
   _transfer_window_add(ui, id, server, nick, "", 0, EINA_TRUE);
}

void
elaro_ui_dcc_init(Elaro_Ui *ui)
{
   ui->dcc.windows = eina_hash_int32_new(NULL);
}

void
elaro_ui_dcc_shutdown(Elaro_Ui *ui)
{
   Eina_Iterator *iter;
   Eina_List *windows = NULL;
   Transfer_Window *transfer;

   iter = eina_hash_iterator_data_new(ui->dcc.windows);
   EINA_ITERATOR_FOREACH (iter, transfer)
      windows = eina_list_append(windows, transfer);
   eina_iterator_free(iter);
   EINA_LIST_FREE (windows, transfer)
      evas_object_del(transfer->win);
   eina_hash_free(ui->dcc.windows);
}
