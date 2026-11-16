#ifndef ELARO_UI_THEME_H
#define ELARO_UI_THEME_H
#include "elaro_ui_palette.h"

static void _frame_bind(Elaro_Ui *ui);

/* These helpers belong to the main window; dialogs inherit its theme. */
static const char *theme_names[] = { "midnight", "porcelain", "verdigris", "ember" };
static const char *theme_labels[] = { "Midnight", "Porcelain", "Verdigris", "Ember" };

static void
_theme_palette_apply(Elaro_Ui *ui)
{
   for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(palette_classes); i++)
     {
        char *value = edje_file_data_get(ui->theme_path, palette_classes[i]);
        int r, g, b, a;
        if (value && sscanf(value, "%d %d %d %d", &r, &g, &b, &a) == 4)
          edje_color_class_set(palette_classes[i], r, g, b, a, r, g, b, a, r, g, b, a);
        free(value);
     }
}

static Eina_Bool
_theme_config_changed(void *data, int type, void *event)
{
   /* Elementary clears global palette classes when desktop settings reload.
    * Its completion event arrives after that reset, including for open popups. */
   _theme_palette_apply(data);
   return ECORE_CALLBACK_RENEW;
}

static void
_theme_shutdown(Elaro_Ui *ui)
{
   if (ui->theme_config_handler) ecore_event_handler_del(ui->theme_config_handler);
   ui->theme_config_handler = NULL;
   if (ui->theme_path) elm_theme_overlay_del(NULL, ui->theme_path);
   free(ui->theme_path);
   ui->theme_path = NULL;
}

static void
_theme_apply(Elaro_Ui *ui)
{
   char *name, *path, *build_dir;
   Eina_Bool development;
   Eina_List *l;
   Evas_Object *icon, *content = NULL;
   int index = ELARO_CLAMP(ui->config->theme, 0, 3);
   name = elaro_string_printf("%s.edj", theme_names[index]);
   /* A checkout must not silently use older, installed theme archives. */
   build_dir = ecore_file_dir_get(ELARO_ASSET_BUILD_DIR);
   development = !ui->binary || (build_dir && !strncmp(ui->binary, build_dir, strlen(build_dir))
                                  && ui->binary[strlen(build_dir)] == '/');
   free(build_dir);
   path = elaro_path_join(development ? ELARO_ASSET_BUILD_DIR : ELARO_DATA_DIR, name, NULL);
   if (!ecore_file_exists(path))
     {
        free(path);
        path = elaro_path_join(development ? ELARO_DATA_DIR : ELARO_ASSET_BUILD_DIR, name, NULL);
     }
   free(name);
   /* Install the replacement before removing the old overlay, so widgets
    * never briefly re-theme through the desktop fallback. */
   if (!ui->theme_path || strcmp(ui->theme_path, path))
     {
        elm_theme_overlay_add(NULL, path);
        if (ui->theme_path) elm_theme_overlay_del(NULL, ui->theme_path);
     }
   free(ui->theme_path);
   ui->theme_path = path;
   _theme_palette_apply(ui);
   if (!ui->theme_config_handler)
      ui->theme_config_handler = ecore_event_handler_add(ELM_EVENT_CONFIG_ALL_CHANGED,
                                                         _theme_config_changed, ui);
   if (ui->frame)
     {
        content = elm_object_part_content_unset(ui->frame, "elaro.content");
        elm_layout_file_set(ui->frame, path, "elaro/frame");
        if (content) elm_object_part_content_set(ui->frame, "elaro.content", content);
        _frame_bind(ui);
     }
   if (ui->about)
     {
        Evas_Object *hero = evas_object_data_get(ui->about, "hero");
        if (hero) elm_layout_file_set(hero, path, "elaro/about/hero");
     }
   EINA_LIST_FOREACH(ui->theme_icons, l, icon)
     elm_image_file_set(icon, path, evas_object_data_get(icon, "elaro-icon-group"));
}

static void
_theme_selected(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui = data;
   Eina_Error error = 0;
   int index = (int)(intptr_t)evas_object_data_get(obj, "theme-index");
   if (index < 0 || index >= 4) return;
   ui->config->theme = index;
   _theme_apply(ui);
   elaro_ui_buffers_recreate(ui);
   if (ui->nicks) elm_genlist_realized_items_update(ui->nicks);
   for (int i = 0; i < 4; i++)
     {
        Evas_Object *button = ui->theme_buttons[i];
        if (button) elm_object_signal_emit(button, i == index ? "elaro,selected,on" : "elaro,selected,off", "elaro");
     }
   if (!elaro_config_save(ui->config, &error))
     {
        elm_object_text_set(ui->theme_status, _("Theme applied. Could not save your preference."));
        elm_box_unpack(evas_object_data_get(ui->theme_status, "box"), ui->theme_status);
        elm_box_pack_after(evas_object_data_get(ui->theme_status, "box"), ui->theme_status,
                           evas_object_data_get(ui->theme_status, "separator"));
        evas_object_show(ui->theme_status);
     }
   else
     {
        elm_box_unpack(evas_object_data_get(ui->theme_status, "box"), ui->theme_status);
        evas_object_hide(ui->theme_status);
     }
}

static void
_frame_resize(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui = data;
   Evas_Event_Mouse_Down *ev = info;
   Elm_Win_Move_Resize_Mode mode = (intptr_t)evas_object_data_get(obj, "elaro-resize-mode");
   if (ev->button == 1 && !elm_win_maximized_get(ui->win))
     elm_win_move_resize_start(ui->win, mode);
}

static void
_frame_move(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui = data;
   Evas_Event_Mouse_Down *ev = info;
   if (ev->button != 1) return;
   if (ev->flags & EVAS_BUTTON_DOUBLE_CLICK)
     elm_win_maximized_set(ui->win, !elm_win_maximized_get(ui->win));
   else elm_win_move_resize_start(ui->win, ELM_WIN_MOVE_RESIZE_MOVE);
}

/* Keep resize hit surfaces above popup backdrops as well as window content. */
static void
_frame_grips_update(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui = data;
   int x, y, w, h, g = ELM_SCALE_SIZE(5);
   evas_object_geometry_get(ui->frame, &x, &y, &w, &h);
   int geometry[8][4] = {
      {x+g,y,w-2*g,g}, {x+g,y+h-g,w-2*g,g},
      {x,y+g,g,h-2*g}, {x+w-g,y+g,g,h-2*g},
      {x,y,g,g}, {x+w-g,y,g,g}, {x,y+h-g,g,g}, {x+w-g,y+h-g,g,g}
   };
   for (unsigned int i = 0; i < 8; i++)
     {
        evas_object_move(ui->resize_grips[i], geometry[i][0], geometry[i][1]);
        evas_object_resize(ui->resize_grips[i], geometry[i][2], geometry[i][3]);
     }
}

static void _frame_theme_changed(void *data, Evas_Object *obj, void *info);

static void
_frame_grips_free(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui = data;
   evas_object_smart_callback_del_full(ui->win, "theme,changed", _frame_theme_changed, ui);
   for (unsigned int i = 0; i < 8; i++)
     {
        evas_object_del(ui->resize_grips[i]);
        ui->resize_grips[i] = NULL;
     }
}

static void
_frame_theme_changed(void *data, Evas_Object *obj, void *info)
{
   _frame_bind(data);
}

static void
_frame_bind(Elaro_Ui *ui)
{
   static const struct { const char *part; Elm_Win_Move_Resize_Mode mode; } edges[] = {
      {"top", ELM_WIN_MOVE_RESIZE_TOP}, {"bottom", ELM_WIN_MOVE_RESIZE_BOTTOM},
      {"left", ELM_WIN_MOVE_RESIZE_LEFT}, {"right", ELM_WIN_MOVE_RESIZE_RIGHT},
      {"tl", ELM_WIN_MOVE_RESIZE_TOP | ELM_WIN_MOVE_RESIZE_LEFT},
      {"tr", ELM_WIN_MOVE_RESIZE_TOP | ELM_WIN_MOVE_RESIZE_RIGHT},
      {"bl", ELM_WIN_MOVE_RESIZE_BOTTOM | ELM_WIN_MOVE_RESIZE_LEFT},
      {"br", ELM_WIN_MOVE_RESIZE_BOTTOM | ELM_WIN_MOVE_RESIZE_RIGHT}
   };
   for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(edges); i++)
     {
        Evas_Object *edge = ui->resize_grips[i];
        if (!edge)
          {
             edge = ui->resize_grips[i] = evas_object_rectangle_add(evas_object_evas_get(ui->win));
             evas_object_color_set(edge, 0, 0, 0, 0);
             evas_object_layer_set(edge, EVAS_LAYER_MAX);
             evas_object_data_set(edge, "elaro-resize-mode", (void *)(intptr_t)edges[i].mode);
             evas_object_event_callback_add(edge, EVAS_CALLBACK_MOUSE_DOWN, _frame_resize, ui);
             evas_object_show(edge);
          }
     }
   evas_object_event_callback_del_full(ui->frame, EVAS_CALLBACK_RESIZE, _frame_grips_update, ui);
   evas_object_event_callback_del_full(ui->frame, EVAS_CALLBACK_MOVE, _frame_grips_update, ui);
   evas_object_event_callback_add(ui->frame, EVAS_CALLBACK_RESIZE, _frame_grips_update, ui);
   evas_object_event_callback_add(ui->frame, EVAS_CALLBACK_MOVE, _frame_grips_update, ui);
   evas_object_event_callback_del_full(ui->frame, EVAS_CALLBACK_DEL, _frame_grips_free, ui);
   evas_object_event_callback_add(ui->frame, EVAS_CALLBACK_DEL, _frame_grips_free, ui);
   evas_object_smart_callback_del_full(ui->win, "theme,changed", _frame_theme_changed, ui);
   evas_object_smart_callback_add(ui->win, "theme,changed", _frame_theme_changed, ui);
   _frame_grips_update(ui, NULL, NULL, NULL);
   Evas_Object *surface = (Evas_Object *)edje_object_part_object_get(
       elm_layout_edje_get(ui->frame), "move");
   if (!surface) return;
   evas_object_pass_events_set(surface, EINA_FALSE);
   evas_object_event_callback_del_full(surface, EVAS_CALLBACK_MOUSE_DOWN, _frame_move, ui);
   evas_object_event_callback_add(surface, EVAS_CALLBACK_MOUSE_DOWN, _frame_move, ui);
}

static void
_window_minimize(void *data, Evas_Object *obj, void *info)
{
   elm_win_iconified_set(((Elaro_Ui *)data)->win, EINA_TRUE);
}
static void
_window_maximize(void *data, Evas_Object *obj, void *info)
{
   Evas_Object *win = ((Elaro_Ui *)data)->win;
   elm_win_maximized_set(win, !elm_win_maximized_get(win));
}

static void
_menu_close(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui = data;
   Evas_Object *menu = ui->app_menu;
   ui->app_menu = NULL;
   ui->theme_status = NULL;
   memset(ui->theme_buttons, 0, sizeof(ui->theme_buttons));
   /* Menus are transient. Reusing a hiding notify can let its delayed
    * dismissal hide the next opening or keep its input blocker alive. */
   if (menu)
     {
        evas_object_smart_callback_del_full(menu, "block,clicked", _menu_close, ui);
        elaro_ui_popup_close(menu);
     }
}
static void
_menu_settings(void *data, Evas_Object *obj, void *info)
{
   _menu_close(data, obj, info);
   elaro_ui_connection_show(data, obj, info);
}
static void
_menu_about(void *data, Evas_Object *obj, void *info)
{
   _menu_close(data, obj, info);
   elaro_ui_about_show(data, obj, info);
}
static Evas_Object *
_menu_button(Elaro_Ui *ui, Evas_Object *parent, const char *asset,
             const char *text, Evas_Smart_Cb callback)
{
   Evas_Object *button = elaro_ui_button_add(parent, NULL, callback, ui);
   Evas_Object *row = elm_box_add(button);
   Evas_Object *icon = elm_icon_add(row);
   Evas_Object *label = elm_label_add(row);
   elm_box_horizontal_set(row, EINA_TRUE);
   elaro_ui_box_padding_set(row, 9, 0);
   elaro_ui_icon_set(ui, icon, asset);
   elaro_ui_size_min_set(icon, 15, 15);
   elaro_ui_size_max_set(icon, 15, 15);
   elm_object_text_set(label, text);
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(label, 0, 0.5);
   elm_box_pack_end(row, icon);
   elm_box_pack_end(row, label);
   evas_object_show(icon);
   evas_object_show(label);
   evas_object_show(row);
   evas_object_pass_events_set(row, EINA_TRUE);
   elm_object_content_set(button, row);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, 0.5);
   elaro_ui_size_min_set(button, 240, 32);
   return button;
}

static void
_menu_show(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui = data;
   Evas_Object *box, *heading, *button;
   int i;
   if (ui->app_menu) { evas_object_show(ui->app_menu); return; }
   ui->app_menu = elm_popup_add(ui->win);
   elm_object_part_text_set(ui->app_menu, "title,text", _("Elaro"));
   evas_object_smart_callback_add(ui->app_menu, "block,clicked", _menu_close, ui);
   box = elm_box_add(ui->app_menu);
   elaro_ui_box_padding_set(box, 0, 3);
   heading = elm_label_add(box);
   elm_object_text_set(heading, _("<b>APPEARANCE</b>"));
   evas_object_size_hint_align_set(heading, 0, 0.5);
   elm_box_pack_end(box, heading);
   evas_object_show(heading);
   for (i = 0; i < 4; i++)
     {
        button = ui->theme_buttons[i] = _menu_button(ui, box, "theme", theme_labels[i], _theme_selected);
        evas_object_data_set(button, "theme-index", (void *)(intptr_t)i);
        if (i == ui->config->theme) elm_object_signal_emit(button, "elaro,selected,on", "elaro");
        elm_box_pack_end(box, button);
     }
   Evas_Object *separator = elm_separator_add(box);
   elm_separator_horizontal_set(separator, EINA_TRUE);
   evas_object_size_hint_weight_set(separator, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(separator, EVAS_HINT_FILL, 0.5);
   elaro_ui_fixed_height_set(separator, 1);
   elaro_ui_padding_set(separator, 0, 0, 6, 6);
   evas_object_data_set(ui->app_menu, "separator", separator);
   elm_box_pack_end(box, separator);
   evas_object_show(separator);
   ui->theme_status = elm_label_add(box);
   evas_object_data_set(ui->theme_status, "box", box);
   evas_object_data_set(ui->theme_status, "separator", separator);
   button = _menu_button(ui, box, "settings", _("Settings"), _menu_settings);
   elm_box_pack_end(box, button);
   evas_object_data_set(ui->app_menu, "settings", button);
   button = _menu_button(ui, box, "application", _("About Elaro"), _menu_about);
   elm_box_pack_end(box, button);
   evas_object_data_set(ui->app_menu, "about", button);
   button = _menu_button(ui, box, "quit", _("Quit"), _close_window_cb);
   elm_box_pack_end(box, button);
   elm_object_content_set(ui->app_menu, box);
   evas_object_show(box);
   elaro_ui_popup_close_button_add(ui, ui->app_menu, _menu_close, ui);
   elaro_ui_popup_show(ui->app_menu);
}
#endif
