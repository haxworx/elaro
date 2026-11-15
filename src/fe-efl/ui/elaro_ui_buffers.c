#include "elaro_ui_private.h"
#include <assert.h>

static void
_buffer_free(void *data);

static void
_select_buffer(void *data, Evas_Object *obj, void *info);

/* The model outlives rows removed when a server is collapsed. */
static void
_buffer_item_deleted(void *data, Evas_Object *obj)
{
   Elaro_Ui_Buffer *b = data;

   b->item = NULL;
}

static void
_buffer_item_append(Elaro_Ui_Buffer *b)
{
   b->item = elm_genlist_item_append(
       b->app->buffers, b->app->buffer_class, b, b->parent ? b->parent->item : NULL,
       !b->parent && strcmp(b->server, "Status") ? ELM_GENLIST_ITEM_TREE : ELM_GENLIST_ITEM_NONE,
       _select_buffer, b);
}

static void
_server_expanded_cb(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui = data;
   Elaro_Ui_Buffer *server = elm_object_item_data_get(info);
   Elaro_Ui_Buffer *b;
   Eina_List *node;

   EINA_LIST_FOREACH (ui->views, node, b)
      if (b->parent == server && !b->item) _buffer_item_append(b);
   elm_genlist_item_update(info);
   if (server->last_selected && ui->current
       && (ui->current == server || ui->current->parent == server))
      elm_genlist_item_selected_set(server->last_selected->item, EINA_TRUE);
}

static void
_server_contracted_cb(void *data, Evas_Object *obj, void *info)
{
   /* Keep history, drafts, unread state and the active conversation intact. */
   elm_genlist_item_subitems_clear(info);
   elm_genlist_item_update(info);
}

static void
_server_expand_request_cb(void *data, Evas_Object *obj, void *info)
{
   elm_genlist_item_expanded_set(info, EINA_TRUE);
}

static void
_buffer_realized_cb(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui_Buffer *b = elm_object_item_data_get(info);
   Evas_Object *view;

   if (b->parent || !strcmp(b->server, "Status")) return;
   view = elm_object_item_part_content_get(info, "elm.swallow.end");
   for (; view; view = evas_object_smart_parent_get(view))
     {
        if (strcmp(evas_object_type_get(view), "edje")) continue;
        Evas_Object *arrow = (Evas_Object *)edje_object_part_object_get(view, "arrow");
        if (!arrow) continue;
        /* Keep the theme's expand signal, but don't select the row. */
        evas_object_propagate_events_set(arrow, EINA_FALSE);
        break;
     }
}

static void
_server_contract_request_cb(void *data, Evas_Object *obj, void *info)
{
   elm_genlist_item_expanded_set(info, EINA_FALSE);
}

static Eina_Bool
_buffer_unread(Elaro_Ui_Buffer *b)
{
   Elaro_Ui_Buffer *child;
   Eina_List *node;

   if (b->unread) return EINA_TRUE;
   if (!b->parent)
      EINA_LIST_FOREACH (b->app->views, node, child)
         if (child->parent == b && child->unread) return EINA_TRUE;
   return EINA_FALSE;
}

static Eina_Bool
_buffer_mention(Elaro_Ui_Buffer *b)
{
   Elaro_Ui_Buffer *child;
   Eina_List *node;

   if (b->mention) return EINA_TRUE;
   if (!b->parent && !elm_genlist_item_expanded_get(b->item))
      EINA_LIST_FOREACH (b->app->views, node, child)
         if (child->parent == b && child->mention) return EINA_TRUE;
   return EINA_FALSE;
}

void
elaro_ui_buffer_select(Elaro_Ui_Buffer *b)
{
   if (b->parent) elm_genlist_item_expanded_set(b->parent->item, EINA_TRUE);
   elm_genlist_item_selected_set(b->item, EINA_TRUE);
   elm_genlist_item_show(b->item, ELM_GENLIST_ITEM_SCROLLTO_IN);
}

typedef struct
{
   Elaro_Ui *app;
   char *server;
} Server_Menu;

static void
_server_menu_deleted_cb(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   Server_Menu *menu = data;
   if (menu->app->buffer_menu == obj) menu->app->buffer_menu = NULL;
   free(menu->server);
   free(menu);
}

static void
_server_menu_dismissed_cb(void *data, Evas_Object *obj, void *info)
{
   evas_object_del(obj);
}

static void
_server_list_clicked_cb(void *data, Evas_Object *obj, void *info)
{
   Server_Menu *menu = data;
   Elaro_Ui *ui = menu->app;
   Elaro_Engine_Command *c = calloc(1, sizeof(*c));
   if (!c) goto allocation_failed;
   /* Keep the clicked server, even if the current conversation has changed. */
   c->type = ELARO_ENGINE_INPUT;
   c->server = strdup(menu->server);
   c->target = strdup("");
   c->text = strdup("/list");
   if (!c->server || !c->target || !c->text) goto allocation_failed;

   evas_object_del(obj);
   elaro_engine_submit(ui->engine, c);
   return;

allocation_failed:
   elaro_engine_command_free(c);
}

static void
_buffer_right_clicked_cb(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui = data;
   Elaro_Ui_Buffer *b;
   Server_Menu *menu;
   int x, y;

   if (ui->buffer_menu) evas_object_del(ui->buffer_menu);
   if (!info) return;
   b = elm_object_item_data_get(info);
   if (!b || *b->target || !strcmp(b->server, "Status")) return;
   menu = calloc(1, sizeof(*menu));
   if (!menu) return;
   menu->app = ui;
   menu->server = strdup(b->server);
   if (!menu->server) goto allocation_failed;
   ui->buffer_menu = elm_menu_add(ui->win);
   evas_object_event_callback_add(ui->buffer_menu, EVAS_CALLBACK_DEL, _server_menu_deleted_cb,
                                  menu);
   evas_object_smart_callback_add(ui->buffer_menu, "dismissed", _server_menu_dismissed_cb, NULL);
   elm_menu_item_add(ui->buffer_menu, NULL, NULL, _("List channels"), _server_list_clicked_cb,
                     menu);
   evas_pointer_canvas_xy_get(evas_object_evas_get(obj), &x, &y);
   elm_menu_move(ui->buffer_menu, x, y);
   evas_object_show(ui->buffer_menu);
   return;

allocation_failed:
   free(menu);
}

static char *
_buffer_text(void *data, Evas_Object *obj, const char *part)
{
   Elaro_Ui_Buffer *b;
   char *name, *text;

   b = data;
   name = *b->target ? strdup(b->target)
                     : strdup(!strcmp(b->server, "Status") ? _("Status") : b->server);
   if (!name) return NULL;
   text = elm_entry_utf8_to_markup(name);
   free(name);
   if (_buffer_unread(b))
     {
        char *bold = elaro_string_printf("<b>%s</b>", text);
        free(text);
        text = bold;
     }

   if (_buffer_mention(b))
     {
        char *highlighted = elaro_string_printf("<color=#e8b866>%s</color>", text);
        free(text);
        text = highlighted;
     }
   return text;
}

static void
_close_buffer_clicked_cb(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui_Buffer *b;
   Elaro_Engine_Command *c;

   b = data;
   c = calloc(1, sizeof(Elaro_Engine_Command));
   if (!c) goto allocation_failed;
   c->type = ELARO_ENGINE_CLOSE;
   c->server = strdup(b->server);
   c->target = strdup(b->target);
   if (!c->server || !c->target) goto allocation_failed;

   elaro_engine_submit(b->app->engine, c);
   return;

allocation_failed:
   elaro_engine_command_free(c);
}

static Evas_Object *
_buffer_control(void *data, Evas_Object *obj, const char *part)
{
   Elaro_Ui_Buffer *b;
   const char *name;
   Evas_Object *button, *icon, *holder;

   b = data;

   if (!strcmp(part, "elm.swallow.icon"))
     {

        if (!*b->target && strcmp(b->server, "Status")) return NULL;
        name = *b->target ? (b->channel ? "chat"
                             : b->away  ? "user-idle"
                                        : "user-available")
                          : "info";

        icon = elm_icon_add(obj);

        if (!elaro_ui_icon_set(b->app, icon, name))
          {
             evas_object_del(icon);

             icon = elm_label_add(obj);
             elm_object_text_set(icon, !strcmp(b->server, "Status") ? "ⓘ"
                                         : b->channel ? "#" : "•");
          }
        elaro_ui_size_min_set(icon, 20, 20);
        elaro_ui_size_max_set(icon, 20, 20);
        elm_object_tooltip_text_set(icon, _buffer_unread(b)              ? _("Unread messages")
                                          : !strcmp(b->server, "Status") ? _("Status")
                                          : b->away                      ? _("Away")
                                          : b->channel ? _("Channel")
                                                       : _("Private conversation"));

        return icon;
     }

   if (strcmp(part, "elm.swallow.end") || !strcmp(b->server, "Status")) return NULL;

   button = elm_button_add(obj);
   /* Closing a row must not also select it through its genlist parent. */
   evas_object_propagate_events_set(button, EINA_FALSE);
   elm_object_style_set(button, "blank");

   icon = elm_icon_add(button);

   if (elm_icon_standard_set(icon, "window-close"))
     {
        /* Preserve the button's footprint while reducing only its artwork. */
        holder = elm_table_add(button);
        elaro_ui_size_min_set(holder, 20, 20);
        elaro_ui_size_min_set(icon, 12, 12);
        elaro_ui_size_max_set(icon, 12, 12);
        evas_object_size_hint_align_set(icon, 0.5, 0.5);
        elm_table_pack(holder, icon, 0, 0, 1, 1);
        evas_object_show(icon);
        elm_object_content_set(button, holder);
     }
   else
     {
        evas_object_del(icon);
        elm_object_text_set(button, "×");
     }
   elm_object_tooltip_text_set(button, *b->target
                                           ? _("Leave channel / close private chat")
                                           : _("Disconnect server and close its conversations"));
   evas_object_smart_callback_add(button, "clicked", _close_buffer_clicked_cb, b);

   return button;
}

static Eina_Bool
_user_panel_animate(void *data, double position)
{
   Elaro_Ui *ui = data;
   double eased = ecore_animator_pos_map(position, ECORE_POS_MAP_SINUSOIDAL, 0, 0);
   double target = ui->user_panel_collapsed ? 0.0 : ui->user_panel_saved_ratio;

   elm_panes_content_right_size_set(ui->user_panes,
       ui->user_panel_animation_from + (target - ui->user_panel_animation_from) * eased);
   if (position < 1.0) return ECORE_CALLBACK_RENEW;
   ui->user_panel_animator = NULL;
   if (!ui->user_panel_collapsed)
     {
        elm_object_part_content_set(ui->user_panes, "right", ui->nicks);
        evas_object_show(ui->nicks);
        elm_panes_fixed_set(ui->user_panes, EINA_FALSE);
        elm_panes_content_right_size_set(ui->user_panes, target);
     }
   return ECORE_CALLBACK_CANCEL;
}

static void
_topic_update(Elaro_Ui *ui)
{
   Elaro_Ui_Buffer *b;
   char *safe;

   b = ui->current;
   if (ui->user_panes)
     {
        Eina_Bool collapse = !b || !b->channel;
        if (collapse != ui->user_panel_collapsed)
          {
             double current = elm_panes_content_right_size_get(ui->user_panes);
             if (collapse && !ui->user_panel_animator) ui->user_panel_saved_ratio = current;
             if (ui->user_panel_animator) ecore_animator_del(ui->user_panel_animator);
             ui->user_panel_collapsed = collapse;
             ui->user_panel_animation_from = current;
             /* Detach the list so its minimum width cannot stop the animation. */
             elm_object_part_content_unset(ui->user_panes, "right");
             evas_object_hide(ui->nicks);
             elm_panes_fixed_set(ui->user_panes, EINA_TRUE);
             ui->user_panel_animator = ecore_animator_timeline_add(0.18, _user_panel_animate, ui);
             if (!ui->user_panel_animator) _user_panel_animate(ui, 1.0);
          }
     }
   safe = elm_entry_utf8_to_markup(b && b->topic ? b->topic : "");
   elm_entry_entry_set(ui->topic, safe);
   free(safe);
   elm_object_part_text_set(ui->topic, "guide", b && b->channel ? _("Topic — Enter to set") : "");
   elm_object_disabled_set(ui->topic, !b || !b->channel);
}

/* Keep sent input separate from the rendered conversation history. */
void
elaro_ui_input_record(Elaro_Ui *ui, const char *text)
{
   Elaro_Ui_Buffer *b = ui->current;
   char *old, *copy;

   if (!b || !text || !*text) return;
   copy = strdup(text);
   if (!copy) return;
   b->input_history = eina_list_append(b->input_history, copy);
   if (eina_list_count(b->input_history) > 200)
     {
        old = eina_list_data_get(b->input_history);
        b->input_history = eina_list_remove_list(b->input_history, b->input_history);
        free(old);
     }
   b->input_position = NULL;
   free(b->input_draft);
   b->input_draft = NULL;
}

Eina_Bool
elaro_ui_input_key_down(void *data, Evas_Object *obj, Evas_Event_Key_Down *ev)
{
   Elaro_Ui *ui = data;
   Elaro_Ui_Buffer *b = ui->current;
   Eina_Bool up;
   char *safe;
   const char *text;

   if (elaro_ui_users_complete(data, obj, ev)) return EINA_TRUE;
   up = !strcmp(ev->key, "Up");
   if ((!up && strcmp(ev->key, "Down")) || !b || evas_key_modifier_is_set(ev->modifiers, "Shift")
       || evas_key_modifier_is_set(ev->modifiers, "Control")
       || evas_key_modifier_is_set(ev->modifiers, "Alt"))
      return EINA_FALSE;
   if (!b->input_history) return EINA_TRUE;
   if (up)
     {
        if (!b->input_position)
          {
             free(b->input_draft);
             b->input_draft = elm_entry_markup_to_utf8(elm_entry_entry_get(obj));
             b->input_position = eina_list_last(b->input_history);
          }
        else if (eina_list_prev(b->input_position))
           b->input_position = eina_list_prev(b->input_position);
     }
   else
     {
        if (!b->input_position) return EINA_TRUE;
        b->input_position = eina_list_next(b->input_position);
     }
   text = b->input_position ? eina_list_data_get(b->input_position) : b->input_draft;
   safe = elm_entry_utf8_to_markup(text ? text : "");
   elm_entry_entry_set(obj, safe);
   elm_entry_cursor_end_set(obj);
   free(safe);
   return EINA_TRUE;
}

void
elaro_ui_buffer_title_update(Elaro_Ui *ui)
{
   Elaro_Ui_Buffer *b = ui->current;
   char *label, *safe, *title;

   if (!b) return;
   if (b->channel && b->user_count_known)
      label = elaro_string_printf(b->user_count == 1 ? _("%s — %s (%u user)")
                                                     : _("%s — %s (%u users)"),
                                  b->server, b->target, b->user_count);
   else
      label = *b->target ? elaro_string_printf("%s — %s", b->server, b->target)
                         : strdup(!strcmp(b->server, "Status") ? _("Status") : b->server);
   safe = elm_entry_utf8_to_markup(label);
   title = elaro_string_printf("<align=center><b>%s</b></align>", safe);
   elm_object_text_set(ui->title, title);
   if (ui->title_button)
     {
        elm_object_disabled_set(ui->title_button, !b->channel && !*b->target);
        if (b->channel) elm_object_tooltip_text_set(ui->title_button, _("View channel bans"));
        else if (*b->target) elm_object_tooltip_text_set(ui->title_button, _("Look up user (WHOIS)"));
        else elm_object_tooltip_unset(ui->title_button);
     }
   free(title);
   free(safe);
   free(label);
}

static void
_select_buffer(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui;
   Elaro_Ui_Buffer *b;
   char *safe;
   Elaro_Engine_Command *c;

   ui = evas_object_data_get(obj, "app");
   b = data;
   if (!b->parent && !elm_genlist_item_expanded_get(b->item) && b->last_selected)
      b = b->last_selected;
   if (b->parent) b->parent->last_selected = b;
   else b->last_selected = NULL;
   elm_object_focus_set(ui->input, EINA_TRUE);
   if (ui->current == b) return;
   elaro_ui_users_save(ui);
   if (ui->current)
     {
        free(ui->current->input_draft);
        ui->current->input_draft = elm_entry_markup_to_utf8(elm_entry_entry_get(ui->input));
        ui->current->input_position = NULL;
     }
   safe = elm_entry_utf8_to_markup(b->input_draft ? b->input_draft : "");
   elm_entry_entry_set(ui->input, safe);
   elm_entry_cursor_end_set(ui->input);
   free(safe);
   ui->current = b;

   if (b->unread)
     {
        b->unread = EINA_FALSE;
        b->mention = EINA_FALSE;
        if (b->item) elm_genlist_item_update(b->item);
        if (b->parent) elm_genlist_item_update(b->parent->item);
     }
   _topic_update(ui);
   elaro_ui_links_reset(ui);
   elm_genlist_item_show(b->item ? b->item : b->parent->item, ELM_GENLIST_ITEM_SCROLLTO_IN);
   elaro_ui_history_refresh(ui);
   elaro_ui_users_clear(ui);
   ui->user_scroll_restore = EINA_TRUE;
   elaro_ui_buffer_title_update(ui);
   c = calloc(1, sizeof(Elaro_Engine_Command));
   if (!c) goto allocation_failed;
   c->type = ELARO_ENGINE_SELECT;
   c->server = strdup(b->server);
   c->target = strdup(b->target);
   if (!c->server || !c->target) goto allocation_failed;

   elaro_engine_submit(ui->engine, c);
   return;

allocation_failed:
   elaro_engine_command_free(c);
}

Elaro_Ui_Buffer *
elaro_ui_buffer_get(Elaro_Ui *ui, const char *server, const char *target)
{
   Eina_List *node;
   Elaro_Ui_Buffer *b, *parent = NULL;

   EINA_LIST_FOREACH (ui->views, node, b)
     {

        if (!strcasecmp(b->server, server) && !strcasecmp(b->target, target)) return b;
     }
   if (*target)
     {
        parent = elaro_ui_buffer_get(ui, server, "");
        if (!parent) return NULL;
     }
   b = calloc(1, sizeof(Elaro_Ui_Buffer));
   if (!b) return NULL;
   b->app = ui;
   b->server = strdup(server);
   b->target = strdup(target);
   b->history = eina_strbuf_new();
   if (!b->server || !b->target || !b->history) goto allocation_failed;
   b->parent = parent;
   ui->views = eina_list_append(ui->views, b);
   if (!parent || elm_genlist_item_expanded_get(parent->item)) _buffer_item_append(b);
   if (!parent && strcmp(server, "Status")) elm_genlist_item_expanded_set(b->item, EINA_TRUE);

   if (!ui->current || (strcmp(server, "Status") && !strcmp(ui->current->server, "Status"))
       || (parent && elm_genlist_item_expanded_get(parent->item) && !*ui->current->target
           && !strcasecmp(ui->current->server, server)))
      elaro_ui_buffer_select(b);

   return b;

allocation_failed:
   free(b->server);
   free(b->target);
   if (b->history) eina_strbuf_free(b->history);
   free(b);
   return NULL;
}

static void
_server_remove(Elaro_Ui *ui, const char *server)
{
   Eina_List *node, *next;
   Elaro_Ui_Buffer *b, *fallback;

   if (!strcmp(server, "Status")) return;
   if (ui->current && !strcasecmp(ui->current->server, server))
     {
        fallback = NULL;
        EINA_LIST_FOREACH (ui->views, node, b)
           if (strcasecmp(b->server, server))
             {
                fallback = b;
                break;
             }
        if (!fallback) fallback = elaro_ui_buffer_get(ui, "Status", "");
        elaro_ui_buffer_select(fallback);
     }
   EINA_LIST_FOREACH_SAFE (ui->views, node, next, b)
     {
        if (strcasecmp(b->server, server)) continue;
        if (b->item) elm_object_item_del(b->item);
        ui->views = eina_list_remove_list(ui->views, node);
        _buffer_free(b);
     }
}

static void
_buffer_remove(Elaro_Ui *ui, const char *server, const char *target)
{
   Eina_List *node;
   Elaro_Ui_Buffer *b;
   Elm_Object_Item *fallback;

   if (!*target)
     {
        _server_remove(ui, server);
        return;
     }

   EINA_LIST_FOREACH (ui->views, node, b)
     {

        if (strcasecmp(b->server, server) || strcasecmp(b->target, target)) continue;

        if (b->parent->last_selected == b) b->parent->last_selected = NULL;
        if (ui->current == b)
          {
             Elaro_Ui_Buffer *candidate;
             Eina_List *other;

             fallback = b->parent->item;
             /* Sidebar siblings retain opening order, regardless of type. */
             EINA_LIST_REVERSE_FOREACH (ui->views, other, candidate)
                if (candidate != b && candidate->parent == b->parent)
                  {
                     elaro_ui_buffer_select(candidate);
                     fallback = NULL;
                     break;
                  }
             if (fallback) elaro_ui_buffer_select(b->parent);
          }
        if (b->item) elm_object_item_del(b->item);
        ui->views = eina_list_remove_list(ui->views, node);
        if (b->parent) elm_genlist_item_update(b->parent->item);
        _buffer_free(b);

        return;
     }
}

void
elaro_ui_topic_apply(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui;
   char *text;
   Elaro_Engine_Command *c;

   ui = data;

   if (!ui->current || !ui->current->channel) return;

   text = elaro_ui_field_text_get(ui->topic);

   if (!text) return;
   if (!strpbrk(text, "\r\n") && strcmp(text, ui->current->topic ? ui->current->topic : ""))
     {
        c = calloc(1, sizeof(Elaro_Engine_Command));
        if (!c) goto allocation_failed;
        c->type = ELARO_ENGINE_INPUT;
        c->server = strdup(ui->current->server);
        c->target = strdup(ui->current->target);
        if (!c->server || !c->target) goto allocation_failed;

        c->text = *text ? elaro_string_printf("/topic -- %s %s", c->target, text)
                        : elaro_string_printf("/topic -delete %s", c->target);
        if (!c->text) goto allocation_failed;
        elaro_engine_submit(ui->engine, c);
     }
   free(text);
   /* Display confirmed state until the server accepts the change. */
   _topic_update(ui);
   return;

allocation_failed:
   elaro_engine_command_free(c);
   free(text);
}

static void
_buffer_free(void *data)
{
   Elaro_Ui_Buffer *b;
   char *text;

   b = data;
   EINA_LIST_FREE (b->input_history, text)
      free(text);
   free(b->input_draft);
   free(b->server);
   free(b->target);
   free(b->topic);
   free(b->connection_host);
   free(b->connection_nick);
   if (b->history) eina_strbuf_free(b->history);
   free(b);
}

void
elaro_ui_buffers_init(Elaro_Ui *ui)
{
   /* The widget must exist before its callbacks are registered. */
   assert(ui->buffers);
   elm_genlist_select_mode_set(ui->buffers, ELM_OBJECT_SELECT_MODE_ALWAYS);
   ui->views = NULL;
   evas_object_smart_callback_add(ui->buffers, "clicked,right", _buffer_right_clicked_cb, ui);
   ui->buffer_class = elm_genlist_item_class_new();
   ui->buffer_class->item_style = "default_style";
   ui->buffer_class->func.text_get = _buffer_text;
   ui->buffer_class->func.content_get = _buffer_control;
   ui->buffer_class->func.del = _buffer_item_deleted;
   /* Row deletion must finish before a close event frees its buffer model. */
   elm_genlist_tree_effect_enabled_set(ui->buffers, EINA_FALSE);
   evas_object_smart_callback_add(ui->buffers, "expand,request", _server_expand_request_cb, ui);
   evas_object_smart_callback_add(ui->buffers, "contract,request", _server_contract_request_cb, ui);
   evas_object_smart_callback_add(ui->buffers, "expanded", _server_expanded_cb, ui);
   evas_object_smart_callback_add(ui->buffers, "realized", _buffer_realized_cb, ui);
   evas_object_smart_callback_add(ui->buffers, "contracted", _server_contracted_cb, ui);
}

void
elaro_ui_buffers_shutdown(Elaro_Ui *ui)
{
   evas_object_smart_callback_del_full(ui->buffers, "realized", _buffer_realized_cb, ui);
   Elaro_Ui_Buffer *b;

   if (ui->user_panel_animator) ecore_animator_del(ui->user_panel_animator);
   ui->user_panel_animator = NULL;
   if (ui->buffer_menu) evas_object_del(ui->buffer_menu);
   evas_object_smart_callback_del_full(ui->buffers, "clicked,right", _buffer_right_clicked_cb, ui);
   evas_object_smart_callback_del_full(ui->buffers, "expand,request", _server_expand_request_cb,
                                       ui);
   evas_object_smart_callback_del_full(ui->buffers, "contract,request", _server_contract_request_cb,
                                       ui);
   evas_object_smart_callback_del_full(ui->buffers, "expanded", _server_expanded_cb, ui);
   evas_object_smart_callback_del_full(ui->buffers, "contracted", _server_contracted_cb, ui);
   ui->current = NULL;
   elm_genlist_clear(ui->buffers);
   EINA_LIST_FREE (ui->views, b)
      _buffer_free(b);
   elm_genlist_item_class_free(ui->buffer_class);
}

void
elaro_ui_buffer_event(Elaro_Ui *ui, const Elaro_Engine_Event *e)
{
   Elaro_Ui_Buffer *b;
   Eina_Bool changed;
   time_t now;
   struct tm local;
   char stamp[6];
   char *plain, *safe;
   const char *cut;

   if (e->type == ELARO_ENGINE_CONNECTION)
     {
        b = elaro_ui_buffer_get(ui, e->server, "");
        if (!b) return;
        free(b->connection_host);
        free(b->connection_nick);
        b->connection_host = e->host ? strdup(e->host) : NULL;
        b->connection_nick = e->nick ? strdup(e->nick) : NULL;
        b->connection_port = e->port;
        b->connected = e->connected;
        return;
     }
   if (e->type == ELARO_ENGINE_USER_PRESENCE || e->type == ELARO_ENGINE_PRESENCE_RESET)
     {
        Eina_List *node;

        /* Presence never creates a chat or selects a hidden row. */
        EINA_LIST_FOREACH (ui->views, node, b)
           if (*b->target && !b->channel && !strcasecmp(b->server, e->server)
               && (!*e->target || !strcasecmp(b->target, e->target)))
             {
                Eina_Bool away = e->type == ELARO_ENGINE_USER_PRESENCE && e->away;
                if (b->away != away)
                  {
                     b->away = away;
                     if (b->item) elm_genlist_item_update(b->item);
                  }
             }
        return;
     }
   if (e->type == ELARO_ENGINE_BUFFER_CLOSED)
     {
        _buffer_remove(ui, e->server, e->target);
     }
   else if (e->type == ELARO_ENGINE_JOINED || e->type == ELARO_ENGINE_QUERY_OPENED)
     {

        b = elaro_ui_buffer_get(ui, e->server, e->target);
        if (!b) return;
        changed = b->channel && e->type == ELARO_ENGINE_JOINED;
        b->channel = e->type == ELARO_ENGINE_JOINED;
        if (b->item) elm_genlist_item_update(b->item);

        if (b == ui->current) _topic_update(ui);
        if (!changed)
          {
             elaro_ui_buffer_select(b);
             elm_object_focus_set(ui->input, EINA_TRUE);
          }
     }
   else if (e->type == ELARO_ENGINE_TOPIC)
     {

        b = elaro_ui_buffer_get(ui, e->server, e->target);
        if (!b) return;
        if (!b->channel)
          {
             b->channel = EINA_TRUE;
             if (b->item) elm_genlist_item_update(b->item);
          }
        /* Metadata updates must not discard an edit for an unchanged topic. */
        changed = strcmp(b->topic ? b->topic : "", e->text) != 0;
        free(b->topic);
        b->topic = strdup(e->text);

        if (b == ui->current && (changed || elm_object_disabled_get(ui->topic))) _topic_update(ui);
     }
   else
     {

        b = elaro_ui_buffer_get(ui, e->server, e->target);
        if (!b) return;
        if (e->type == ELARO_ENGINE_LINE && e->private_message && ui->config
            && b != ui->current)
          {
             if (ui->config->notify_private_messages)
                elaro_ui_notify_private(e->server, e->target, e->text);
             if (ui->config->enable_sounds) elaro_ui_notify_sound(ui);
          }
        now = time(NULL);
        localtime_r(&now, &local);
        strftime(stamp, sizeof(stamp), "%H:%M", &local);
        plain = elaro_string_printf("[%s] %s", stamp, e->text);
        safe = elaro_ui_text_markup(plain);
        eina_strbuf_append(b->history, safe);
        eina_strbuf_append(b->history, "<br/>");

        if (*b->target && b != ui->current && (!b->unread || (e->mention && !b->mention)))
          {
             b->unread = EINA_TRUE;
             if (e->mention) b->mention = EINA_TRUE;
             if (b->item) elm_genlist_item_update(b->item);
             if (b->parent) elm_genlist_item_update(b->parent->item);
          }
        /* Bound retained history, cutting only at a complete markup line. */

        if (eina_strbuf_length_get(b->history) > 256 * 1024)
          {
             cut = strstr(eina_strbuf_string_get(b->history)
                              + eina_strbuf_length_get(b->history) / 2,
                          "<br/>");

             if (cut)
                eina_strbuf_remove(b->history, 0, cut + 5 - eina_strbuf_string_get(b->history));

             if (b == ui->current)
                elaro_ui_history_refresh(ui);
          }
        else if (b == ui->current)
           elaro_ui_history_append(ui, safe);

        free(safe);
        free(plain);
     }
}
