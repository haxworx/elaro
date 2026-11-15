#include "elaro_ui_private.h"
#include <locale.h>

static Eina_Bool
_receive_events_cb(void *data, Ecore_Fd_Handler *handler)
{
   Elaro_Ui *ui;
   Elaro_Engine_Event *event;
   int i;
   double until = ecore_time_get() + 0.004;

   ui = data;

   for (i = 0; i < 128 && ecore_time_get() < until && (event = elaro_engine_poll(ui->engine)); i++)
     {
        switch (event->type)
          {
          case ELARO_ENGINE_LIST_OPEN:
          case ELARO_ENGINE_LIST_BATCH:
          case ELARO_ENGINE_LIST_END:
          case ELARO_ENGINE_LIST_ERROR:
             elaro_ui_list_event(ui, event);
             break;
          case ELARO_ENGINE_BAN_ENTRY:
          case ELARO_ENGINE_BAN_END:
          case ELARO_ENGINE_BAN_ERROR:
             elaro_ui_bans_event(ui, event);
             break;
          case ELARO_ENGINE_TRANSFER_OFFER:
          case ELARO_ENGINE_TRANSFER_UPDATE:
          case ELARO_ENGINE_TRANSFER_END:
          case ELARO_ENGINE_TRANSFER_ERROR:
             elaro_ui_dcc_event(ui, event);
             break;
          case ELARO_ENGINE_USER_DETAILS:
             elaro_ui_users_details(ui, event);
             break;
          case ELARO_ENGINE_USER_PRESENCE:
          case ELARO_ENGINE_PRESENCE_RESET:
             elaro_ui_users_presence(ui, event);
             elaro_ui_buffer_event(ui, event);
             break;
          case ELARO_ENGINE_NICKS:

             if (ui->current && !strcmp(event->server, ui->current->server)
                 && !strcmp(event->target, ui->current->target))
                elaro_ui_users_update(ui, event->users, event->user_count);
             break;
          default:
             if (event->type == ELARO_ENGINE_BUFFER_CLOSED)
               {
                  elaro_ui_bans_event(ui, event);
                  elaro_ui_list_event(ui, event);
               }
             elaro_ui_buffer_event(ui, event);
             break;
          }
        elaro_engine_event_free(event);
     }

   return ECORE_CALLBACK_RENEW;
}

static void
_send_input_cb(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui;
   char *text;
   Elaro_Engine_Command *c;

   ui = data;
   text = elm_entry_markup_to_utf8(elm_entry_entry_get(ui->input));

   if (text && *text && ui->current)
     {
        c = calloc(1, sizeof(Elaro_Engine_Command));
        if (!c) goto allocation_failed;
        c->type = ELARO_ENGINE_INPUT;
        c->server = strdup(ui->current->server);
        c->target = strdup(ui->current->target);
        c->text = strdup(text);
        if (!c->server || !c->target || !c->text) goto allocation_failed;

        elaro_ui_input_record(ui, text);
        elaro_engine_submit(ui->engine, c);
        elm_entry_entry_set(ui->input, "");
     }
   free(text);
   return;

allocation_failed:
   elaro_engine_command_free(c);
   free(text);
}

static void
_window_resized_cb(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui;

   ui = data;

   elaro_ui_layout_capture(ui);
}

static void
_layout_changed_cb(void *data, Evas_Object *obj, void *info)
{
   elaro_ui_layout_capture(data);
}

static void
_close_window_cb(void *data, Evas_Object *obj, void *info)
{
   elm_exit();
}

#include "elaro_ui_theme.h"

static Eina_Bool
_smoke_done_cb(void *data)
{
   elm_exit();

   return ECORE_CALLBACK_CANCEL;
}

static Evas_Object *
_shadow_add(Evas_Object *parent, const char *group)
{
   Evas_Object *shadow;
   char *path;

   shadow = elm_layout_add(parent);
   path = elaro_path_join(ELARO_DATA_DIR, "elaro.edj", NULL);
   if (!elm_layout_file_set(shadow, path, group))
     {
        free(path);
        path = elaro_path_join(ELARO_ASSET_BUILD_DIR, "elaro.edj", NULL);
        elm_layout_file_set(shadow, path, group);
     }
   free(path);
   elm_object_focus_allow_set(shadow, EINA_FALSE);
   evas_object_pass_events_set(shadow, EINA_TRUE);
   elaro_ui_size_min_set(shadow, 0, 6);
   evas_object_size_hint_weight_set(shadow, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(shadow, EVAS_HINT_FILL, 0);
   evas_object_show(shadow);

   return shadow;
}

int
elaro_ui_run(int argc, char **argv)
{
   Elaro_Ui ui;
   Eina_Error config_error, save_error;
   Evas_Object *icon, *box, *bar, *shadow, *connection, *about, *split, *right, *conversation;
   char *path;
   Ecore_Fd_Handler *events;

   setlocale(LC_ALL, "");
#if ENABLE_NLS
   bindtextdomain(PACKAGE_TARNAME, LOCALEDIR);
   bind_textdomain_codeset(PACKAGE_TARNAME, "UTF-8");
   textdomain(PACKAGE_TARNAME);
#endif

   /* Ecore must never dispatch the engine's default GLib context. */
   ecore_main_loop_glib_always_integrate_disable();

   if (!elm_init(argc, argv)) return 1;
   /* This is a desktop client: the default Elementary profile can otherwise
    * disable drag selection and right-click text menus. Do not persist it. */
   elm_config_desktop_entry_set(EINA_TRUE);

   ui = (Elaro_Ui){ 0 };
   ui.binary = elaro_executable_find(argv[0]);
   if (!ui.binary) ui.binary = strdup(argv[0]);
   config_error = 0;
   ui.config = elaro_config_load(&config_error);

   if (config_error)
     {
        ui.config_error = strdup(eina_error_msg_get(config_error));
     }
   if (!ui.config || !ui.binary)
     {
        elaro_config_free(ui.config);
        free(ui.config_error);
        free(ui.binary);
        elm_shutdown();
        return 1;
     }
   ui.engine = elaro_engine_start(argv[0]);
   if (!ui.engine)
     {
        EINA_LOG_ERR("Cannot create engine wakeup descriptors");
        elaro_config_free(ui.config);
        free(ui.config_error);
        free(ui.binary);
        elm_shutdown();
        return 1;
     }
   elaro_ui_dcc_init(&ui);

   _theme_apply(&ui);
   ui.win = elm_win_add(NULL, "elaro", ELM_WIN_BASIC);
   elm_win_borderless_set(ui.win, EINA_TRUE);
   elm_win_title_set(ui.win, _("Elaro — IRC"));

   ui.background = elm_bg_add(ui.win);
   evas_object_size_hint_weight_set(ui.background, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_win_resize_object_add(ui.win, ui.background);
   evas_object_show(ui.background);

   icon = elm_icon_add(ui.win);

   if (!elm_icon_standard_set(icon, "elaro"))
     {
        path = elaro_path_join(ELARO_DATA_DIR, "..", "icons", "hicolor", "128x128", "apps",
                               "elaro.png", NULL);

        if (!elm_image_file_set(icon, path, NULL))
          {
             free(path);
             path = elaro_path_join(ELARO_SOURCE_DIR, "data", "elaro.png", NULL);
             elm_image_file_set(icon, path, NULL);
          }
        free(path);
     }
   elm_win_icon_object_set(ui.win, icon);
   evas_object_smart_callback_add(ui.win, "delete,request", _close_window_cb, &ui);
   box = ui.content = elm_box_add(ui.win);
   elaro_ui_opacity_apply(&ui);
   elm_box_padding_set(box, 0, 0);
   elaro_ui_expand(box);
   ui.frame = elm_layout_add(ui.win);
   elm_layout_file_set(ui.frame, ui.theme_path, "elaro/frame");
   elaro_ui_expand(ui.frame);
   elm_win_resize_object_add(ui.win, ui.frame);
   elm_object_part_content_set(ui.frame, "elaro.content", box);
   elaro_ui_opacity_apply(&ui);
   _frame_bind(&ui);

   bar = elm_box_add(ui.win);
   elm_box_horizontal_set(bar, EINA_TRUE);
   elm_box_align_set(bar, 0, 0.5);
   elaro_ui_box_padding_set(bar, 6, 0);
   elaro_ui_padding_set(bar, 6, 6, 4,
                                     4);
   evas_object_size_hint_weight_set(bar, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(bar, EVAS_HINT_FILL, 0);
   elm_box_pack_end(box, bar);
   evas_object_show(bar);

   connection
       = elaro_ui_icon_button_add(ui.win, "menu", _("Application menu"), _menu_show, &ui);
   evas_object_size_hint_align_set(connection, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(bar, connection);

   /* A dedicated title region leaves the channel heading clickable. */
   icon = elm_label_add(bar);
   elm_object_style_set(icon, "elaro/title");
   elm_object_text_set(icon, "<b>ELARO</b>");
   evas_object_size_hint_weight_set(icon, 0, 0);
   evas_object_size_hint_align_set(icon, 0, 0.5);
   elaro_ui_size_min_set(icon, 0, 38);
   elm_box_pack_end(bar, icon);
   evas_object_show(icon);
   elm_box_pack_end(bar, elaro_ui_bans_heading_add(&ui, bar));

   /* Give the space after the heading to window dragging, not the button. */
   icon = evas_object_rectangle_add(evas_object_evas_get(ui.win));
   evas_object_color_set(icon, 0, 0, 0, 0);
   evas_object_size_hint_weight_set(icon, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(icon, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elaro_ui_size_min_set(icon, 40, 38);
   evas_object_event_callback_add(icon, EVAS_CALLBACK_MOUSE_DOWN, _frame_move, &ui);
   elm_box_pack_end(bar, icon);
   evas_object_show(icon);

   about = elaro_ui_icon_button_add(ui.win, "minimize", _("Minimize"), _window_minimize, &ui);
   elm_box_pack_end(bar, about);
   about = elaro_ui_icon_button_add(ui.win, "maximize", _("Maximize or restore"), _window_maximize, &ui);
   elm_box_pack_end(bar, about);
   about = elaro_ui_icon_button_add(ui.win, "close", _("Close Elaro"), _close_window_cb, &ui);
   elm_box_pack_end(bar, about);

   shadow = _shadow_add(ui.win, "elaro/header/shadow");
   elm_box_pack_end(box, shadow);

   split = ui.buffer_panes = elm_panes_add(ui.win);
   elaro_ui_expand(split);
   elm_box_pack_end(box, split);

   ui.buffers = elm_genlist_add(ui.win);
   elaro_ui_buffers_init(&ui);
   elm_genlist_homogeneous_set(ui.buffers, EINA_TRUE);
   elaro_ui_expand(ui.buffers);
   evas_object_data_set(ui.buffers, "app", &ui);
   elm_object_part_content_set(split, "left", ui.buffers);

   right = ui.user_panes = elm_panes_add(ui.win);
   elaro_ui_expand(right);
   elm_object_part_content_set(split, "right", right);

   conversation = elm_box_add(ui.win);
   elaro_ui_expand(conversation);
   elm_object_part_content_set(right, "left", conversation);

   ui.topic = elaro_ui_entry_add(conversation, EINA_FALSE);
   elm_entry_single_line_set(ui.topic, EINA_TRUE);
   elm_entry_scrollable_set(ui.topic, EINA_TRUE);
   elm_object_part_text_set(ui.topic, "guide", _("Topic — Enter to set"));
   evas_object_size_hint_weight_set(ui.topic, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(ui.topic, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(conversation, ui.topic);
   evas_object_show(ui.topic);
   elm_object_tooltip_text_set(
       ui.topic,
       _("Edit and press Enter to set the topic; empty clears it. Channel permissions apply."));
   evas_object_smart_callback_add(ui.topic, "activated", elaro_ui_topic_apply, &ui);

   ui.history = elaro_ui_history_add(ui.win);
   elaro_ui_links_init(&ui);
   elm_box_pack_end(conversation, ui.history);

   ui.nicks = elm_genlist_add(ui.win);
   elaro_ui_expand(ui.nicks);
   elaro_ui_users_init(&ui);
   elm_object_part_content_set(right, "right", ui.nicks);

   shadow = _shadow_add(ui.win, "elaro/input/shadow");
   elm_box_pack_end(box, shadow);

   ui.input = elaro_ui_entry_add(ui.win, EINA_FALSE);
   elm_entry_single_line_set(ui.input, EINA_TRUE);
   elm_entry_key_down_intercept_set(ui.input, elaro_ui_input_key_down, &ui);
   elm_entry_scrollable_set(ui.input, EINA_TRUE);
   elm_object_part_text_set(ui.input, "guide", _("Message or /join #channel — Enter to send"));
   evas_object_size_hint_weight_set(ui.input, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(ui.input, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, ui.input);
   evas_object_show(ui.input);
   evas_object_smart_callback_add(ui.input, "activated", _send_input_cb, &ui);
   elaro_ui_buffer_get(&ui, "Status", "");
   events = ecore_main_fd_handler_add(elaro_engine_event_fd(ui.engine),
                                      ECORE_FD_READ, _receive_events_cb, &ui,
                                      NULL, NULL);
   if (!events) EINA_LOG_CRIT("Cannot watch engine events");
   elaro_ui_layout_restore(&ui);
   evas_object_event_callback_add(ui.win, EVAS_CALLBACK_RESIZE, _window_resized_cb, &ui);
   evas_object_smart_callback_add(ui.win, "moved", _layout_changed_cb, &ui);
   evas_object_smart_callback_add(split, "unpress", _layout_changed_cb, &ui);
   evas_object_smart_callback_add(right, "unpress", _layout_changed_cb, &ui);
   evas_object_show(ui.win);
   elaro_ui_connection_show(&ui, NULL, NULL);

   if (argc == 2 && !strcmp(argv[1], "--smoke-test")) ecore_timer_add(2.0, _smoke_done_cb, NULL);
   if (events) elm_run();
   if (events) ecore_main_fd_handler_del(events);

   elaro_ui_layout_capture(&ui);
   save_error = 0;

   if (!elaro_config_save(ui.config, &save_error))
     {
        EINA_LOG_WARN("%s", eina_error_msg_get(save_error));
     }
   elaro_ui_list_shutdown(&ui);
   elaro_ui_bans_shutdown(&ui);
   elaro_ui_dcc_shutdown(&ui);
   elaro_engine_stop(ui.engine);
   elaro_ui_users_shutdown(&ui);
   elaro_ui_avatars_shutdown(&ui);
   elaro_ui_buffers_shutdown(&ui);

   if (ui.about) evas_object_del(ui.about);

   if (ui.preview) evas_object_del(ui.preview);

   if (ui.music) evas_object_del(ui.music);
   evas_object_del(ui.win);
   elaro_config_free(ui.config);
   free(ui.binary);
   if (ui.theme_path) elm_theme_overlay_del(NULL, ui.theme_path);
   free(ui.theme_path);
   free(ui.config_error);
   elm_shutdown();

   return events ? 0 : 1;
}
