#include "elaro_ui_private.h"

struct _Elaro_Ui_List
{
   Elaro_Ui *app;
   char *server;
   Evas_Object *win, *box, *list, *status, *progress, *refresh, *join;
   Elm_Genlist_Item_Class *item_class;
   Eina_List *pending;
   Ecore_Idler *idler;
   unsigned int request_id, count;
   Eina_Bool loading, ended, available;
};

static void
_row_free(void *data, Evas_Object *obj)
{
   Elaro_Engine_Channel *row = data;
   free(row->name);
   free(row->topic);
   free(row);
}

static char *
_row_text(void *data, Evas_Object *obj, const char *part)
{
   Elaro_Engine_Channel *row = data;
   char *plain, *safe;
   if (!strcmp(part, "elm.text.sub")) return elaro_ui_text_markup(row->topic);
   plain = elaro_string_printf(row->users == 1 ? _("%s — %u user") : _("%s — %u users"), row->name,
                               row->users);
   safe = elm_entry_utf8_to_markup(plain);
   free(plain);
   return safe;
}

static void
_controls(Elaro_Ui_List *b)
{
   if (b->loading && !evas_object_visible_get(b->progress))
     {
        elm_box_pack_before(b->box, b->progress, b->status);
        evas_object_show(b->progress);
        elm_progressbar_pulse(b->progress, EINA_TRUE);
     }
   else if (!b->loading && evas_object_visible_get(b->progress))
     {
        elm_progressbar_pulse(b->progress, EINA_FALSE);
        evas_object_hide(b->progress);
        elm_box_unpack(b->box, b->progress);
     }
   elm_object_disabled_set(b->refresh, !b->available || b->loading);
   elm_object_disabled_set(b->join, !b->available || !elm_genlist_selected_item_get(b->list));
}

static void
_selected(void *data, Evas_Object *obj, void *info)
{
   _controls(data);
}

static void
_status(Elaro_Ui_List *b)
{
   char *text;
   if (b->loading && !b->ended) return;
   text = elaro_string_printf(b->loading ? _("Adding channels… %u") : _("Channels: %u"), b->count);
   elm_object_text_set(b->status, text);
   evas_object_show(b->status);
   free(text);
}

static Eina_Bool
_populate(void *data)
{
   Elaro_Ui_List *b = data;
   unsigned int i;
   double until = ecore_time_get() + 0.004;
   for (i = 0; b->pending && i < 128 && ecore_time_get() < until; i++)
     {
        Elaro_Engine_Channel *row = b->pending->data;
        b->pending = eina_list_remove_list(b->pending, b->pending);
        elm_genlist_item_append(b->list, b->item_class, row, NULL, ELM_GENLIST_ITEM_NONE, NULL,
                                NULL);
        b->count++;
     }
   if (!b->pending && b->ended) b->loading = EINA_FALSE;
   _status(b);
   _controls(b);
   if (!b->pending)
     {
        b->idler = NULL;
        return ECORE_CALLBACK_CANCEL;
     }
   return ECORE_CALLBACK_RENEW;
}

static void
_pending_clear(Elaro_Ui_List *b)
{
   Elaro_Engine_Channel *row;
   if (b->idler) ecore_idler_del(b->idler);
   b->idler = NULL;
   EINA_LIST_FREE (b->pending, row)
      _row_free(row, NULL);
}

static void
_cancel(Elaro_Ui_List *b)
{
   Elaro_Engine_Command *c = calloc(1, sizeof(*c));
   if (!c) goto allocation_failed;
   c->type = ELARO_ENGINE_LIST_CANCEL;
   c->server = strdup(b->server);
   if (!c->server) goto allocation_failed;

   c->request_id = b->request_id;
   elaro_engine_submit(b->app->engine, c);
   return;

allocation_failed:
   elaro_engine_command_free(c);
}

static void
_refresh(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui_List *b = data;
   Elaro_Engine_Command *c;
   if (b->loading || !b->available) return;
   c = calloc(1, sizeof(*c));
   if (!c) goto allocation_failed;
   c->type = ELARO_ENGINE_LIST;
   c->server = strdup(b->server);
   if (!c->server) goto allocation_failed;

   _pending_clear(b);
   elm_genlist_clear(b->list);
   b->count = 0;
   b->ended = EINA_FALSE;
   b->loading = EINA_TRUE;
   b->request_id = ++b->app->list_request_id;
   elm_object_text_set(b->status, "");
   evas_object_hide(b->status);
   _controls(b);
   c->request_id = b->request_id;
   elaro_engine_submit(b->app->engine, c);
   return;

allocation_failed:
   elaro_engine_command_free(c);
}

static void
_join(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui_List *b = data;
   Elm_Object_Item *item = elm_genlist_selected_item_get(b->list);
   Elaro_Engine_Channel *row;
   Elaro_Engine_Command *c;
   if (!b->available || !item) return;
   row = elm_object_item_data_get(item);
   c = calloc(1, sizeof(*c));
   if (!c) goto allocation_failed;
   c->type = ELARO_ENGINE_INPUT;
   c->server = strdup(b->server);
   c->target = strdup("");
   c->text = elaro_string_printf("/join %s", row->name);
   if (!c->server || !c->target || !c->text) goto allocation_failed;

   elaro_engine_submit(b->app->engine, c);
   return;

allocation_failed:
   elaro_engine_command_free(c);
}

static void
_deleted(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   Elaro_Ui_List *b = data;
   b->app->channel_list = NULL;
   _cancel(b);
   _pending_clear(b);
   evas_object_smart_callback_del_full(b->list, "selected", _selected, b);
   evas_object_smart_callback_del_full(b->list, "unselected", _selected, b);
   evas_object_del(b->list);
   elm_genlist_item_class_free(b->item_class);
   free(b->server);
   free(b);
}

static void
_close(void *data, Evas_Object *obj, void *info)
{
   evas_object_del(((Elaro_Ui_List *)data)->win);
}

static void
_show(Elaro_Ui *ui, const char *server)
{
   Elaro_Ui_List *b;
   Evas_Object *box, *buttons, *close;
   char *title;
   if (ui->channel_list)
     {
        if (ui->channel_list->available && !strcmp(ui->channel_list->server, server))
          {
             elm_win_activate(ui->channel_list->win);
             return;
          }
        elaro_ui_list_shutdown(ui);
     }
   b = ui->channel_list = calloc(1, sizeof(*b));
   if (!b) return;
   b->app = ui;
   b->server = strdup(server);
   if (!b->server) goto allocation_failed;
   b->available = EINA_TRUE;
   b->win = elm_win_add(ui->win, "elaro-channel-list", ELM_WIN_DIALOG_BASIC);
   title = elaro_string_printf(_("Channels — %s"), server);
   elm_win_title_set(b->win, title);
   free(title);
   elm_win_autodel_set(b->win, EINA_TRUE);
   box = b->box = elaro_ui_window_content_add(b->win);
   elaro_ui_box_padding_set(box, 0, 6);
   evas_object_show(box);
   b->progress = elm_progressbar_add(box);
   evas_object_size_hint_weight_set(b->progress, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(b->progress, EVAS_HINT_FILL, 0.5);
   elm_progressbar_unit_format_set(b->progress, NULL);
   elm_progressbar_pulse_set(b->progress, EINA_TRUE);
   b->status = elm_label_add(box);
   evas_object_size_hint_align_set(b->status, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(box, b->status);
   b->list = elm_genlist_add(box);
   elaro_ui_expand(b->list);
   elm_genlist_mode_set(b->list, ELM_LIST_COMPRESS);
   elm_genlist_homogeneous_set(b->list, EINA_TRUE);
   elm_box_pack_end(box, b->list);
   evas_object_show(b->list);
   b->item_class = elm_genlist_item_class_new();
   b->item_class->item_style = "double_label";
   b->item_class->func.text_get = _row_text;
   b->item_class->func.del = _row_free;
   evas_object_smart_callback_add(b->list, "selected", _selected, b);
   evas_object_smart_callback_add(b->list, "unselected", _selected, b);
   buttons = elm_box_add(box);
   elm_box_horizontal_set(buttons, EINA_TRUE);
   elm_box_pack_end(box, buttons);
   evas_object_show(buttons);
   b->refresh = elm_button_add(buttons);
   elm_object_text_set(b->refresh, _("Refresh"));
   evas_object_smart_callback_add(b->refresh, "clicked", _refresh, b);
   elm_box_pack_end(buttons, b->refresh);
   evas_object_show(b->refresh);
   b->join = elm_button_add(buttons);
   elm_object_text_set(b->join, _("Join selected channel"));
   evas_object_smart_callback_add(b->join, "clicked", _join, b);
   elm_box_pack_end(buttons, b->join);
   evas_object_show(b->join);
   close = elm_button_add(buttons);
   elm_object_text_set(close, _("Close"));
   evas_object_smart_callback_add(close, "clicked", _close, b);
   elm_box_pack_end(buttons, close);
   evas_object_show(close);
   evas_object_event_callback_add(b->win, EVAS_CALLBACK_DEL, _deleted, b);
   evas_object_resize(b->win, ELM_SCALE_SIZE(480), ELM_SCALE_SIZE(345));
   evas_object_show(b->win);
   _refresh(b, NULL, NULL);
   return;

allocation_failed:
   free(b);
   ui->channel_list = NULL;
}

void
elaro_ui_list_event(Elaro_Ui *ui, Elaro_Engine_Event *e)
{
   Elaro_Ui_List *b;
   char *safe;
   unsigned int i;
   if (e->type == ELARO_ENGINE_LIST_OPEN)
     {
        _show(ui, e->server);
        return;
     }
   b = ui->channel_list;
   if (!b || strcmp(b->server, e->server)) return;
   if (e->type == ELARO_ENGINE_BUFFER_CLOSED && !*e->target)
     {
        _pending_clear(b);
        b->available = b->loading = EINA_FALSE;
        elm_object_text_set(b->status, _("This server is closed."));
        evas_object_show(b->status);
        _controls(b);
        return;
     }
   if (!b->available || !b->loading || b->request_id != e->request_id) return;
   if (e->type == ELARO_ENGINE_LIST_BATCH)
     {
        for (i = 0; i < e->channel_count; i++)
          {
             Elaro_Engine_Channel *row = malloc(sizeof(*row));
             if (!row) return;
             *row = e->channels[i];
             e->channels[i].name = e->channels[i].topic = NULL;
             b->pending = eina_list_append(b->pending, row);
          }
        if (b->pending && !b->idler) b->idler = ecore_idler_add(_populate, b);
     }
   else if (e->type == ELARO_ENGINE_LIST_END)
     {
        b->ended = EINA_TRUE;
        if (!b->pending) b->loading = EINA_FALSE;
        _status(b);
        _controls(b);
     }
   else if (e->type == ELARO_ENGINE_LIST_ERROR)
     {
        _pending_clear(b);
        elm_genlist_clear(b->list);
        b->loading = EINA_FALSE;
        safe = elm_entry_utf8_to_markup(e->text);
        elm_object_text_set(b->status, safe);
        evas_object_show(b->status);
        free(safe);
        _controls(b);
     }
}

void
elaro_ui_list_shutdown(Elaro_Ui *ui)
{
   if (ui->channel_list) evas_object_del(ui->channel_list->win);
}
