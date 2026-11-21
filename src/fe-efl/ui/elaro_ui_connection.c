#include "elaro_ui_private.h"

static void
_saved_list_fill(Elaro_Ui *ui, Elaro_Config *selected);

static void
_popup_closed_cb(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui;

   ui = data;
   Evas_Object *popup = ui->connection.popup;
   if (popup) evas_object_smart_callback_del_full(popup, "block,clicked", _popup_closed_cb, ui);
   elaro_ui_popup_close(popup);
   ui->connection.popup = NULL;
   ui->connection.selected = NULL;
}

static void
_popup_deleted_cb(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui = data;
   if (ui->connection.popup != obj) return;
   ui->connection.popup = NULL;
   ui->connection.selected = NULL;
}

static void
_view_changed_cb(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui;
   Eina_Bool options;
   Evas_Object *previous, *next;

   ui = data;
   options = elm_segment_control_item_index_get(info) == 1;

   if (options == ui->connection.options_visible) return;

   previous = ui->connection.options_visible ? ui->connection.options_view
                                             : ui->connection.connection_view;
   next = options ? ui->connection.options_view : ui->connection.connection_view;
   elm_table_unpack(ui->connection.view_box, previous);
   elm_object_tree_focus_allow_set(previous, EINA_FALSE);
   evas_object_hide(previous);
   elm_table_pack(ui->connection.view_box, next, 0, 0, 1, 1);
   elm_object_tree_focus_allow_set(next, EINA_TRUE);
   evas_object_show(next);
   elm_object_focus_set(options ? ui->connection.allow_links : ui->connection.host,
                        EINA_TRUE);
   elm_scroller_region_show(next, 0, 0, 1, 1);
   elaro_ui_view_reveal(next);
   ui->connection.options_visible = options;
   elm_object_text_set(ui->connection.apply, options ? _("Save options") : _("Save"));
}

static void
_save_clicked_cb(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui;
   Elaro_Config *c, *saved, *profile, *selected;
   char *port, *end;
   long number;
   Eina_Bool valid_port, replaced;
   Eina_List *node;
   Eina_Error error;

   ui = data;

   if (ui->connection.options_visible)
     {
        elaro_ui_options_save(ui);

        return;
     }
   if (!ui->connection.dirty)
     {
        error = 0;
        if (elaro_config_save(ui->config, &error))
           elm_object_text_set(ui->connection.error, _("Settings saved."));
        else elm_object_text_set(ui->connection.error, eina_error_msg_get(error));
        return;
     }

   c = calloc(1, sizeof(Elaro_Config));
   if (!c) goto allocation_failed;
   c->version = ELARO_CONFIG_VERSION;
   c->last_connected_port = ui->config->last_connected_port;
   if (ui->config->last_connected_host && ui->config->last_connected_nick)
     {
        c->last_connected_host = strdup(ui->config->last_connected_host);
        c->last_connected_nick = strdup(ui->config->last_connected_nick);
        if (!c->last_connected_host || !c->last_connected_nick) goto allocation_failed;
     }
   c->notify_private_messages = ui->config->notify_private_messages;
   c->enable_sounds = ui->config->enable_sounds;
   c->always_allow_external_links = ui->config->always_allow_external_links;
   c->window_transparent = ui->config->window_transparent;
   c->window_opacity = ui->config->window_opacity;
   c->window_width = ui->config->window_width;
   c->window_height = ui->config->window_height;
   c->window_x = ui->config->window_x;
   c->window_y = ui->config->window_y;
   c->window_position_set = ui->config->window_position_set;
   c->buffer_panel_ratio = ui->config->buffer_panel_ratio;
   c->user_panel_ratio = ui->config->user_panel_ratio;
   c->host = elaro_ui_field_text_get(ui->connection.host);
   c->nick = elaro_ui_field_text_get(ui->connection.nick);
   c->username = elaro_ui_field_text_get(ui->connection.username);
   c->realname = elaro_ui_field_text_get(ui->connection.realname);
   c->email = elaro_ui_field_text_get(ui->connection.email);
   c->password = elaro_ui_field_text_get(ui->connection.password);
   c->channels = elaro_ui_field_text_get(ui->connection.channels);
   c->sasl_username = elaro_ui_field_text_get(ui->connection.sasl_username);
   c->sasl_password = elaro_ui_field_text_get(ui->connection.sasl_password);
   c->auto_reconnect = elm_check_state_get(ui->connection.auto_reconnect);
   c->tls = elm_check_state_get(ui->connection.tls);
   c->sasl = elm_check_state_get(ui->connection.sasl);
   if (!c->host || !c->nick || !c->username || !c->realname || !c->email
       || !c->password || !c->channels || !c->sasl_username || !c->sasl_password)
      goto allocation_failed;
   port = elaro_ui_field_text_get(ui->connection.port);
   if (!port) goto allocation_failed;
   number = strtol(port, &end, 10);
   valid_port = *port && !*end && number >= 1 && number <= 65535;
   free(port);

   if (!*c->host || !*c->nick || !valid_port || strpbrk(c->host, " \t\r\n")
       || strpbrk(c->nick, " ,:\t\r\n") || strpbrk(c->channels, "\r\n")
       || strpbrk(c->password, "\r\n"))
     {
        elm_object_text_set(ui->connection.error, _("Enter a host, nickname and port (1–65535)."));
        elaro_config_free(c);

        return;
     }

   if (!*c->username || strpbrk(c->username, " @:\t\r\n"))
     {
        elm_object_text_set(ui->connection.error,
                            _("Enter an IRC username without spaces, @ or colon."));
        elaro_config_free(c);

        return;
     }

   if (strpbrk(c->realname, "\r\n<>") || strlen(c->realname) > 128
       || (*c->email && !elaro_email_valid(c->email)))
     {
        elm_object_text_set(
            ui->connection.error,
            _("Enter a valid optional email and a name without angle brackets or line breaks."));
        elaro_config_free(c);
        return;
     }

   if (c->sasl
       && (!*c->sasl_username || !*c->sasl_password || strpbrk(c->sasl_username, "\r\n")
           || strpbrk(c->sasl_password, "\r\n")))
     {
        elm_object_text_set(ui->connection.error,
                            _("SASL requires an account name and password."));
        elaro_config_free(c);

        return;
     }
   c->port = number;

   replaced = EINA_FALSE;
   selected = NULL;
   EINA_LIST_FOREACH (ui->config->connections, node, saved)
     {
        if (saved == ui->connection.selected
            || (!ui->connection.selected && !strcasecmp(saved->host, c->host)
                && saved->port == c->port && !strcasecmp(saved->nick, c->nick)))
          {
             profile = elaro_config_connection_copy(c);
             if (!profile) goto allocation_failed;
             c->connections = eina_list_append(c->connections, profile);
             selected = profile;
             replaced = EINA_TRUE;
          }
        else
          {
             profile = elaro_config_connection_copy(saved);
             if (!profile) goto allocation_failed;
             c->connections = eina_list_append(c->connections, profile);
          }
     }

   if (!replaced)
     {
        selected = elaro_config_connection_copy(c);
        if (!selected) goto allocation_failed;
        c->connections = eina_list_append(c->connections, selected);
     }
   error = 0;

   if (!elaro_config_save(c, &error))
     {
        elm_object_text_set(ui->connection.error, eina_error_msg_get(error));

        elaro_config_free(c);

        return;
     }
   /* Genlist items reference the old profiles. Drop them before replacing config. */
   ui->connection.selected = NULL;
   elm_genlist_clear(ui->connection.saved);
   elaro_config_free(ui->config);
   ui->config = c;
   free(ui->config_error);
   ui->config_error = NULL;
   _saved_list_fill(ui, selected);
   ui->connection.dirty = EINA_FALSE;
   elm_object_text_set(ui->connection.error, _("Connection saved."));
   return;

allocation_failed:
   elaro_config_free(c);
}

static char *
_saved_text_get(void *data, Evas_Object *obj, const char *part)
{
   Elaro_Config *c;
   char *text, *safe;

   c = data;
   text = elaro_string_printf("%s:%d — %s", c->host, c->port, c->nick);
   safe = elm_entry_utf8_to_markup(text);
   free(text);

   return safe;
}

static void
_field_set(Evas_Object *field, const char *value)
{
   char *safe;

   safe = elm_entry_utf8_to_markup(value);
   elm_entry_entry_set(field, safe);
   free(safe);
}

static void
_saved_selected_cb(void *data, Evas_Object *obj, void *info)
{
   Elaro_Config *c;
   Elaro_Ui *ui;
   char *port;

   c = data;
   ui = evas_object_data_get(obj, "ui");
   ui->connection.selected = c;
#define SET(field) _field_set(ui->connection.field, c->field)
   SET(host);
   SET(nick);
   SET(username);
   SET(realname);
   SET(email);
   SET(password);
   SET(channels);
   SET(sasl_username);
   SET(sasl_password);
#undef SET
   port = elaro_string_printf("%d", c->port);
   _field_set(ui->connection.port, port);
   free(port);
   elm_check_state_set(ui->connection.auto_reconnect, c->auto_reconnect);
   elm_check_state_set(ui->connection.tls, c->tls);
   elm_check_state_set(ui->connection.sasl, c->sasl);
   elm_object_disabled_set(ui->connection.connect, EINA_FALSE);
   elm_object_disabled_set(ui->connection.remove, EINA_FALSE);
   elm_object_text_set(ui->connection.error, "");
   ui->connection.dirty = EINA_FALSE;
}

static void
_saved_connect_cb(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui;
   const Elaro_Config *c;
   Elaro_Engine_Command *command;

   ui = data;
   c = ui->connection.selected;
   if (!c) return;

   command = calloc(1, sizeof(Elaro_Engine_Command));
   if (!command) goto allocation_failed;
   command->type = ELARO_ENGINE_CONNECT;
   command->host = strdup(c->host);
   command->nick = strdup(c->nick);
   command->username = strdup(c->username);
   command->realname = strdup(c->realname);
   command->email = strdup(c->email);
   command->port = c->port;
   command->auto_reconnect = c->auto_reconnect;
   command->tls = c->tls;
   command->password = strdup(c->password);
   command->channels = strdup(c->channels);
   command->sasl = c->sasl;
   command->sasl_username = strdup(c->sasl_username);
   command->sasl_password = strdup(c->sasl_password);
   if (!command->host || !command->nick || !command->username || !command->realname
       || !command->email || !command->password || !command->channels || !command->sasl_username
       || !command->sasl_password)
      goto allocation_failed;

   elaro_engine_submit(ui->engine, command);
   _popup_closed_cb(ui, NULL, NULL);
   return;

allocation_failed:
   elaro_engine_command_free(command);
}

static void
_saved_delete_cb(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui;
   Elaro_Config *selected;
   Eina_List *original;
   Eina_Error error;
   Elm_Object_Item *item;

   ui = data;
   selected = ui->connection.selected;

   if (!selected) return;
   /* Keep the original list intact until the atomic save succeeds. */
   original = ui->config->connections;
   ui->config->connections = eina_list_remove(eina_list_clone(original), selected);
   error = 0;

   if (!elaro_config_save(ui->config, &error))
     {
        eina_list_free(ui->config->connections);
        ui->config->connections = original;
        elm_object_text_set(ui->connection.error, eina_error_msg_get(error));

        return;
     }
   eina_list_free(original);
   item = elm_genlist_selected_item_get(ui->connection.saved);

   if (item) elm_object_item_del(item);
   ui->connection.selected = NULL;
   elaro_config_free(selected);
   ui->connection.dirty = EINA_FALSE;
   elm_object_disabled_set(ui->connection.connect, EINA_TRUE);
   elm_object_disabled_set(ui->connection.remove, EINA_TRUE);
   elm_object_text_set(ui->connection.error, _("Saved connection deleted."));
}

static void
_saved_new_cb(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui;
   Elm_Object_Item *item;

   ui = data;
   item = elm_genlist_selected_item_get(ui->connection.saved);

   if (item) elm_genlist_item_selected_set(item, EINA_FALSE);
   ui->connection.selected = NULL;
#define CLEAR(field) _field_set(ui->connection.field, "")
   CLEAR(host);
   CLEAR(nick);
   CLEAR(username);
   CLEAR(realname);
   CLEAR(email);
   CLEAR(password);
   CLEAR(channels);
   CLEAR(sasl_username);
   CLEAR(sasl_password);
#undef CLEAR
   _field_set(ui->connection.port, "6697");
   elm_check_state_set(ui->connection.tls, EINA_TRUE);
   elm_check_state_set(ui->connection.auto_reconnect, EINA_FALSE);
   elm_check_state_set(ui->connection.sasl, EINA_FALSE);
   elm_object_disabled_set(ui->connection.connect, EINA_TRUE);
   elm_object_disabled_set(ui->connection.remove, EINA_TRUE);
   ui->connection.dirty = EINA_TRUE;
   elm_object_text_set(ui->connection.error, "");
   elm_object_focus_set(ui->connection.host, EINA_TRUE);
}

static void
_saved_list_fill(Elaro_Ui *ui, Elaro_Config *selected)
{
   Elm_Genlist_Item_Class *klass;
   Elm_Object_Item *item;
   Eina_List *node;
   Elaro_Config *saved;

   klass = elm_genlist_item_class_new();
   klass->item_style = "default";
   klass->func.text_get = _saved_text_get;
   EINA_LIST_FOREACH (ui->config->connections, node, saved)
     {
        item = elm_genlist_item_append(ui->connection.saved, klass, saved, NULL,
                                       ELM_GENLIST_ITEM_NONE, _saved_selected_cb, saved);
        if (saved == selected)
          {
             elm_genlist_item_selected_set(item, EINA_TRUE);
             elm_genlist_item_show(item, ELM_GENLIST_ITEM_SCROLLTO_IN);
          }
     }
   elm_genlist_item_class_free(klass);
}

static void
_connection_edited_cb(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui;

   ui = data;
   ui->connection.dirty = EINA_TRUE;
}

static Evas_Object *
_connection_field_add(Evas_Object *box, const char *label, const char *value, Eina_Bool secret)
{
   Evas_Object *caption, *entry;
   char *safe;

   caption = elm_label_add(box);
   elm_object_text_set(caption, label);
   evas_object_size_hint_align_set(caption, 0, 0.5);
   elm_box_pack_end(box, caption);
   evas_object_show(caption);

   entry = elaro_ui_entry_add(box, secret);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_password_set(entry, secret);
   elm_object_part_text_set(entry, "guide", secret ? _("Password") : label);
   safe = elm_entry_utf8_to_markup(value);
   elm_entry_entry_set(entry, safe);
   free(safe);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, entry);
   evas_object_show(entry);

   return entry;
}

static Elaro_Config *
_initial_profile(Elaro_Ui *ui)
{
   Elaro_Ui_Buffer *server = ui->current;
   Elaro_Config *saved, *endpoint = NULL;
   Eina_List *node;

   if (server && server->parent) server = server->parent;
   if (server && server->connected && server->connection_host)
     {
        EINA_LIST_FOREACH (ui->config->connections, node, saved)
          {
             if (strcasecmp(saved->host, server->connection_host)
                 || saved->port != server->connection_port) continue;
             if (!endpoint) endpoint = saved;
             if (server->connection_nick && !strcasecmp(saved->nick, server->connection_nick))
                return saved;
          }
        if (endpoint) return endpoint;
     }
   if (ui->config->last_connected_host && ui->config->last_connected_nick)
     {
        EINA_LIST_FOREACH (ui->config->connections, node, saved)
           if (saved->port == ui->config->last_connected_port
               && !strcasecmp(saved->host, ui->config->last_connected_host)
               && !strcasecmp(saved->nick, ui->config->last_connected_nick))
              return saved;
     }
   return eina_list_data_get(ui->config->connections);
}

void
elaro_ui_connection_show(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui;
   Evas_Object *box, *frame, *saved_box, *list_slot, *list_size, *buttons, *viewport, *size,
       *scroller, *root, *tabs, *view_size, *details;
   char *port;
   int height;

   ui = data;

   if (ui->connection.popup) return;

   ui->connection.popup = elm_popup_add(ui->win);
   elm_object_part_text_set(ui->connection.popup, "title,text", _("Settings"));
   evas_object_event_callback_add(ui->connection.popup, EVAS_CALLBACK_DEL, _popup_deleted_cb, ui);
   evas_object_smart_callback_add(ui->connection.popup, "block,clicked", _popup_closed_cb, ui);
   ui->connection.options_visible = EINA_FALSE;

   box = elm_box_add(ui->connection.popup);
   elm_box_padding_set(box, 0, 2);

   frame = elm_frame_add(box);
   elm_object_text_set(frame, _("Saved connections"));
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);

   saved_box = elm_box_add(frame);
   evas_object_size_hint_weight_set(saved_box, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(saved_box, EVAS_HINT_FILL, EVAS_HINT_FILL);

   ui->connection.saved = elm_genlist_add(saved_box);
   evas_object_data_set(ui->connection.saved, "ui", ui);
   elm_genlist_mode_set(ui->connection.saved, ELM_LIST_COMPRESS);
   elm_genlist_homogeneous_set(ui->connection.saved, EINA_TRUE);
   evas_object_size_hint_weight_set(ui->connection.saved, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(ui->connection.saved, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elaro_ui_size_min_set(ui->connection.saved, 240, 72);

   list_slot = elm_table_add(saved_box);
   evas_object_size_hint_weight_set(list_slot, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(list_slot, EVAS_HINT_FILL, EVAS_HINT_FILL);

   list_size = evas_object_rectangle_add(evas_object_evas_get(list_slot));
   evas_object_size_hint_weight_set(list_size, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(list_size, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_color_set(list_size, 0, 0, 0, 0);
   evas_object_pass_events_set(list_size, EINA_TRUE);
   elaro_ui_size_min_set(list_size, 240, 72);
   elm_table_pack(list_slot, list_size, 0, 0, 1, 1);
   evas_object_show(list_size);
   elm_table_pack(list_slot, ui->connection.saved, 0, 0, 1, 1);
   elm_box_pack_end(saved_box, list_slot);
   evas_object_show(list_slot);
   evas_object_show(ui->connection.saved);

   buttons = elm_box_add(saved_box);
   elm_box_horizontal_set(buttons, EINA_TRUE);
   elm_box_homogeneous_set(buttons, EINA_TRUE);
   evas_object_size_hint_weight_set(buttons, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(buttons, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elaro_ui_box_padding_set(buttons, 5, 0);
   elaro_ui_padding_set(buttons, 0, 0, 6, 0);

   ui->connection.connect = elaro_ui_button_add(buttons, _("Connect"), _saved_connect_cb, ui);

   ui->connection.remove = elaro_ui_button_add(buttons, _("Delete"), _saved_delete_cb, ui);
   elm_object_disabled_set(ui->connection.connect, EINA_TRUE);
   elm_object_disabled_set(ui->connection.remove, EINA_TRUE);
   elm_box_pack_end(buttons, ui->connection.connect);
   elm_box_pack_end(buttons, ui->connection.remove);
   elm_box_pack_end(buttons, elaro_ui_button_add(buttons, _("New"), _saved_new_cb, ui));
   Eina_List *button_children = elm_box_children_get(buttons);
   Evas_Object *button;
   EINA_LIST_FREE(button_children, button)
     {
        evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0);
        evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
     }
   elm_box_pack_end(saved_box, buttons);
   evas_object_show(buttons);
   elm_object_content_set(frame, saved_box);
   evas_object_show(saved_box);
   elm_box_pack_end(box, frame);
   evas_object_show(frame);

   frame = elm_frame_add(box);
   elm_object_text_set(frame, _("Edit the fields below to save a new connection."));
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   details = elm_box_add(frame);
   elaro_ui_box_padding_set(details, 0, 2);
   elm_object_content_set(frame, details);
   elm_box_pack_end(box, frame);
   evas_object_show(details);
   evas_object_show(frame);

   ui->connection.host = _connection_field_add(details, _("Server"), ui->config->host, EINA_FALSE);
   port = elaro_string_printf("%d", ui->config->port);

   ui->connection.port = _connection_field_add(details, _("Port"), port, EINA_FALSE);
   free(port);

   ui->connection.nick
       = _connection_field_add(details, _("Nickname"), ui->config->nick, EINA_FALSE);

   ui->connection.username
       = _connection_field_add(details, _("IRC username"), ui->config->username, EINA_FALSE);

   ui->connection.realname = _connection_field_add(details, _("Real name (optional)"),
                                                   ui->config->realname, EINA_FALSE);
   ui->connection.email = _connection_field_add(details, _("Public email for Gravatar (optional)"),
                                                ui->config->email, EINA_FALSE);
   elm_object_tooltip_text_set(
       ui->connection.email,
       _("Published in your IRC real name as Name &lt;email&gt;. Leave blank to keep your email private. Changes apply when reconnecting."));

   ui->connection.password = _connection_field_add(details, _("Server password (optional)"),
                                                   ui->config->password, EINA_TRUE);

   ui->connection.channels = _connection_field_add(
       details, _("Channels (optional, comma separated)"), ui->config->channels, EINA_FALSE);

   ui->connection.auto_reconnect = elm_check_add(details);
   elm_object_text_set(ui->connection.auto_reconnect,
                       _("Automatically reconnect and rejoin channels"));
   elm_object_tooltip_text_set(
       ui->connection.auto_reconnect,
       _("Retry after 5 seconds, up to 5 times. Each attempt times out after 30 seconds."));
   elm_check_state_set(ui->connection.auto_reconnect, ui->config->auto_reconnect);
   evas_object_size_hint_weight_set(ui->connection.auto_reconnect, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(ui->connection.auto_reconnect, 0, 0.5);
   elm_box_pack_end(details, ui->connection.auto_reconnect);
   evas_object_show(ui->connection.auto_reconnect);

   ui->connection.tls = elm_check_add(details);
   elm_object_text_set(ui->connection.tls, _("TLS (verify certificate)"));
   elm_check_state_set(ui->connection.tls, ui->config->tls);
   evas_object_size_hint_weight_set(ui->connection.tls, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(ui->connection.tls, 0, 0.5);
   elm_box_pack_end(details, ui->connection.tls);
   evas_object_show(ui->connection.tls);

   ui->connection.sasl = elm_check_add(details);
   elm_object_text_set(ui->connection.sasl, _("SASL PLAIN authentication"));
   elm_check_state_set(ui->connection.sasl, ui->config->sasl);
   evas_object_size_hint_weight_set(ui->connection.sasl, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(ui->connection.sasl, 0, 0.5);
   elm_box_pack_end(details, ui->connection.sasl);
   evas_object_show(ui->connection.sasl);

   ui->connection.sasl_username
       = _connection_field_add(details, _("SASL account"), ui->config->sasl_username, EINA_FALSE);

   ui->connection.sasl_password
       = _connection_field_add(details, _("SASL password"), ui->config->sasl_password, EINA_TRUE);

   ui->connection.error = elm_label_add(box);
   elm_box_pack_end(box, ui->connection.error);
   evas_object_show(ui->connection.error);

   if (ui->config_error) elm_object_text_set(ui->connection.error, ui->config_error);
   /* Reserve a viewport so the fields remain usable on smaller windows. */

   evas_object_geometry_get(ui->win, NULL, NULL, NULL, &height);
   /* Size helpers take logical units. Reserve room for the title, tabs,
    * actions and surrounding window instead of scaling pixel space twice. */
   height = MIN(560, MAX(120, (int)(height / elm_config_scale_get()) - 210));

   viewport = elm_table_add(ui->connection.popup);
   evas_object_size_hint_weight_set(viewport, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(viewport, EVAS_HINT_FILL, EVAS_HINT_FILL);

   size = evas_object_rectangle_add(evas_object_evas_get(viewport));
   evas_object_color_set(size, 0, 0, 0, 0);
   evas_object_pass_events_set(size, EINA_TRUE);
   elaro_ui_size_min_set(size, 360, height);
   elm_table_pack(viewport, size, 0, 0, 1, 1);
   evas_object_show(size);

   scroller = elm_scroller_add(viewport);
   elm_scroller_policy_set(scroller, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, 0);
   elm_object_content_set(scroller, box);
   elm_table_pack(viewport, scroller, 0, 0, 1, 1);
   evas_object_show(scroller);
   ui->connection.connection_view = viewport;

   root = elm_box_add(ui->connection.popup);
   elaro_ui_box_padding_set(root, 0, 11);
   elaro_ui_padding_set(root, 9, 9,
                                     9, 9);

   tabs = elaro_ui_tabs_add(root);
   elm_segment_control_item_add(tabs, NULL, _("Connection"));
   elm_segment_control_item_add(tabs, NULL, _("Options"));
   elm_segment_control_item_selected_set(elm_segment_control_item_get(tabs, 0), EINA_TRUE);
   evas_object_size_hint_align_set(tabs, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(tabs, EVAS_HINT_EXPAND, 0);
   elm_box_pack_end(root, tabs);
   evas_object_show(tabs);
   evas_object_smart_callback_add(tabs, "changed", _view_changed_cb, ui);
   evas_object_data_set(ui->connection.popup, "pages", tabs);

   ui->connection.view_box = elm_table_add(root);
   /* Keep one size reservation outside the switched views: box widgets
    * recalculate their own minimum size from their children. */

   view_size = evas_object_rectangle_add(evas_object_evas_get(root));
   evas_object_color_set(view_size, 0, 0, 0, 0);
   evas_object_pass_events_set(view_size, EINA_TRUE);
   elaro_ui_size_min_set(view_size, 360, height);
   elm_table_pack(ui->connection.view_box, view_size, 0, 0, 1, 1);
   evas_object_show(view_size);
   evas_object_size_hint_align_set(ui->connection.view_box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(ui->connection.view_box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_box_pack_end(root, ui->connection.view_box);
   evas_object_show(ui->connection.view_box);
   elm_table_pack(ui->connection.view_box, viewport, 0, 0, 1, 1);
   evas_object_show(viewport);

   elaro_ui_options_add(ui, root);
   elm_object_content_set(ui->connection.popup, root);
   evas_object_show(root);
   evas_object_show(box);

   ui->connection.apply
       = elaro_ui_button_add(ui->connection.popup, _("Save"), _save_clicked_cb, ui);
   elm_object_part_content_set(ui->connection.popup, "button1", ui->connection.apply);
   elaro_ui_popup_close_button_add(ui, ui->connection.popup, _popup_closed_cb, ui);
   ui->connection.dirty = EINA_FALSE;
   _saved_list_fill(ui, _initial_profile(ui));
#define WATCH(field)                                                                               \
   evas_object_smart_callback_add(ui->connection.field, "changed,user", _connection_edited_cb, ui)
   WATCH(host);
   WATCH(port);
   WATCH(nick);
   WATCH(username);
   WATCH(realname);
   WATCH(email);
   WATCH(password);
   WATCH(channels);
   WATCH(sasl_username);
   WATCH(sasl_password);
#undef WATCH
   evas_object_smart_callback_add(ui->connection.auto_reconnect, "changed", _connection_edited_cb,
                                  ui);
   evas_object_smart_callback_add(ui->connection.tls, "changed", _connection_edited_cb, ui);
   evas_object_smart_callback_add(ui->connection.sasl, "changed", _connection_edited_cb, ui);
   elaro_ui_popup_show(ui->connection.popup);
}
