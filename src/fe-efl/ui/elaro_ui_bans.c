#include "elaro_ui_private.h"

struct _Elaro_Ui_Bans
{
   Elaro_Ui *app;
   char *server, *channel;
   Evas_Object *win, *list, *status, *refresh, *unban;
   Elm_Genlist_Item_Class *item_class;
   unsigned int request_id, count;
   Eina_Bool loading, available;
};

static char *
_mask_text(void *data, Evas_Object *obj, const char *part)
{
   return elm_entry_utf8_to_markup(data);
}

static void
_mask_free(void *data, Evas_Object *obj)
{
   free(data);
}

static void
_controls(Elaro_Ui_Bans *b)
{
   elm_object_disabled_set(b->refresh, b->loading || !b->available);
   elm_object_disabled_set(b->unban,
                           b->loading || !b->available || !elm_genlist_selected_item_get(b->list));
}

static void
_selected(void *data, Evas_Object *obj, void *info)
{
   _controls(data);
}

static void
_request(Elaro_Ui_Bans *b, const char *mask)
{
   Elaro_Engine_Command *c = calloc(1, sizeof(*c));
   if (!c) goto allocation_failed;
   c->type = mask ? ELARO_ENGINE_UNBAN : ELARO_ENGINE_BANS;
   c->server = strdup(b->server);
   c->target = strdup(b->channel);
   c->text = mask ? strdup(mask) : NULL;
   if (!c->server || !c->target || (mask && !c->text)) goto allocation_failed;

   c->request_id = ++b->app->ban_request_id;
   b->request_id = c->request_id;
   b->loading = EINA_TRUE;
   b->count = 0;
   elm_genlist_clear(b->list);
   elm_object_text_set(b->status, _("Loading bans…"));
   _controls(b);
   elaro_engine_submit(b->app->engine, c);
   return;

allocation_failed:
   elaro_engine_command_free(c);
}

static void
_refresh(void *data, Evas_Object *obj, void *info)
{
   _request(data, NULL);
}

static void
_unban(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui_Bans *b = data;
   Elm_Object_Item *item = elm_genlist_selected_item_get(b->list);
   if (item && !b->loading && b->available) _request(b, elm_object_item_data_get(item));
}

static void
_deleted(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   Elaro_Ui_Bans *b = data;
   b->app->bans = NULL;
   evas_object_smart_callback_del_full(b->list, "selected", _selected, b);
   evas_object_smart_callback_del_full(b->list, "unselected", _selected, b);
   evas_object_del(b->list);
   elm_genlist_item_class_free(b->item_class);
   free(b->server);
   free(b->channel);
   free(b);
}

static void
_close(void *data, Evas_Object *obj, void *info)
{
   evas_object_del(((Elaro_Ui_Bans *)data)->win);
}

static void
_heading_clicked(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui = data;
   Elaro_Ui_Buffer *b = ui->current;
   Elaro_Engine_Command *c;

   if (!b) return;
   if (b->channel)
     {
        elaro_ui_bans_show(data, obj, info);
        return;
     }
   if (!*b->target) return;
   c = calloc(1, sizeof(*c));
   if (!c) goto allocation_failed;
   c->type = ELARO_ENGINE_USER_INFO;
   c->server = strdup(b->server);
   c->target = strdup(b->target);
   if (!c->server || !c->target) goto allocation_failed;

   elaro_engine_submit(ui->engine, c);
   return;

allocation_failed:
   elaro_engine_command_free(c);
}

/* Use a real button: labels render markup but do not expose anchor clicks. */
Evas_Object *
elaro_ui_bans_heading_add(Elaro_Ui *ui, Evas_Object *parent)
{
   Evas_Object *button = ui->title_button = elm_button_add(parent);
   elm_object_style_set(button, "blank");
   evas_object_size_hint_weight_set(button, 0, 0);
   evas_object_size_hint_align_set(button, 0, 0.5);
   evas_object_smart_callback_add(button, "clicked", _heading_clicked, ui);
   ui->title = elm_label_add(button);
   elm_object_style_set(ui->title, "elaro/title");
   evas_object_size_hint_weight_set(ui->title, 0, 0);
   evas_object_size_hint_align_set(ui->title, 0, 0.5);
   evas_object_pass_events_set(ui->title, EINA_TRUE);
   elm_object_content_set(button, ui->title);
   evas_object_show(ui->title);
   evas_object_show(button);
   return button;
}

void
elaro_ui_bans_show(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui = data;
   Elaro_Ui_Buffer *current = ui->current;
   Elaro_Ui_Bans *b;
   Evas_Object *box, *buttons, *close;
   char *title;
   if (!current || !current->channel) return;
   if (ui->bans)
     {
        if (ui->bans->available && !strcmp(ui->bans->server, current->server)
            && !strcmp(ui->bans->channel, current->target))
          {
             elm_win_activate(ui->bans->win);
             return;
          }
        elaro_ui_bans_shutdown(ui);
     }
   b = ui->bans = calloc(1, sizeof(*b));
   if (!b) return;
   b->app = ui;
   b->available = EINA_TRUE;
   b->server = strdup(current->server);
   b->channel = strdup(current->target);
   if (!b->server || !b->channel) goto allocation_failed;
   b->win = elm_win_add(ui->win, "elaro-channel-bans", ELM_WIN_DIALOG_BASIC);
   title = elaro_string_printf(_("Bans — %s — %s"), b->server, b->channel);
   elm_win_title_set(b->win, title);
   free(title);
   elm_win_autodel_set(b->win, EINA_TRUE);
   box = elaro_ui_window_content_add(b->win);
   elaro_ui_box_padding_set(box, 0, 6);
   evas_object_show(box);
   b->status = elm_label_add(box);
   evas_object_size_hint_align_set(b->status, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(box, b->status);
   evas_object_show(b->status);
   b->list = elm_genlist_add(box);
   elaro_ui_expand(b->list);
   elm_genlist_mode_set(b->list, ELM_LIST_COMPRESS);
   elm_box_pack_end(box, b->list);
   evas_object_show(b->list);
   b->item_class = elm_genlist_item_class_new();
   b->item_class->item_style = "default";
   b->item_class->func.text_get = _mask_text;
   b->item_class->func.del = _mask_free;
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
   b->unban = elm_button_add(buttons);
   elm_object_text_set(b->unban, _("Unban selected mask"));
   evas_object_smart_callback_add(b->unban, "clicked", _unban, b);
   elm_box_pack_end(buttons, b->unban);
   evas_object_show(b->unban);
   close = elm_button_add(buttons);
   elm_object_text_set(close, _("Close"));
   evas_object_smart_callback_add(close, "clicked", _close, b);
   elm_box_pack_end(buttons, close);
   evas_object_show(close);
   evas_object_event_callback_add(b->win, EVAS_CALLBACK_DEL, _deleted, b);
   evas_object_resize(b->win, ELM_SCALE_SIZE(405), ELM_SCALE_SIZE(285));
   evas_object_show(b->win);
   _request(b, NULL);
   return;

allocation_failed:
   free(b->server);
   free(b->channel);
   free(b);
   ui->bans = NULL;
}

void
elaro_ui_bans_event(Elaro_Ui *ui, const Elaro_Engine_Event *e)
{
   Elaro_Ui_Bans *b = ui->bans;
   char *safe;
   if (!b || strcmp(e->server, b->server)) return;
   if (e->type == ELARO_ENGINE_BUFFER_CLOSED && (!*e->target || !strcmp(e->target, b->channel)))
     {
        b->loading = EINA_FALSE;
        b->available = EINA_FALSE;
        elm_object_text_set(b->status, _("This channel is closed."));
        _controls(b);
        return;
     }
   if (!b->available || !b->loading || e->request_id != b->request_id
       || strcmp(e->target, b->channel))
      return;
   if (e->type == ELARO_ENGINE_BAN_ENTRY)
     {
        char *mask = strdup(e->text);
        Elm_Object_Item *item;
        if (!mask) return;
        item = elm_genlist_item_append(b->list, b->item_class, mask,
                                      NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);
        if (!item)
          {
             free(mask);
             return;
          }
        safe = elm_entry_utf8_to_markup(e->text);
        elm_object_item_tooltip_text_set(item, safe);
        free(safe);
        b->count++;
        return;
     }
   if (e->type != ELARO_ENGINE_BAN_END && e->type != ELARO_ENGINE_BAN_ERROR) return;
   b->loading = EINA_FALSE;
   if (e->type == ELARO_ENGINE_BAN_ERROR)
     {
        elm_genlist_clear(b->list);
        safe = elm_entry_utf8_to_markup(e->text);
        elm_object_text_set(b->status, safe);
        free(safe);
     }
   else
      elm_object_text_set(b->status, b->count ? _("Select a mask to remove its ban.")
                                              : _("No bans in this channel."));
   _controls(b);
}

void
elaro_ui_bans_shutdown(Elaro_Ui *ui)
{
   if (ui->bans) evas_object_del(ui->bans->win);
}
