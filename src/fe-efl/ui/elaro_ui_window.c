#include "elaro_ui_util.h"
#include "../elaro_intl.h"

typedef struct {
   Evas_Object *win, *frame, *root, *icons[3], *grips[8];
} Window_Frame;

static const char *window_icons[] = { "minimize", "maximize", "close" };

void
elaro_ui_window_control_style_set(Evas_Object *button, Eina_Bool close)
{
   elm_object_style_set(button, close ? "elaro/window-close" : "elaro/window-control");
   elaro_ui_size_min_set(button, 30, 30);
   elaro_ui_size_max_set(button, 30, 30);
   Evas_Object *icon = elm_object_part_content_get(button, "icon");
   if (icon)
     {
        elaro_ui_size_min_set(icon, 19, 19);
        elaro_ui_size_max_set(icon, 19, 19);
     }
}
static const Elm_Win_Move_Resize_Mode resize_modes[] = {
   ELM_WIN_MOVE_RESIZE_TOP, ELM_WIN_MOVE_RESIZE_BOTTOM,
   ELM_WIN_MOVE_RESIZE_LEFT, ELM_WIN_MOVE_RESIZE_RIGHT,
   ELM_WIN_MOVE_RESIZE_TOP | ELM_WIN_MOVE_RESIZE_LEFT,
   ELM_WIN_MOVE_RESIZE_TOP | ELM_WIN_MOVE_RESIZE_RIGHT,
   ELM_WIN_MOVE_RESIZE_BOTTOM | ELM_WIN_MOVE_RESIZE_LEFT,
   ELM_WIN_MOVE_RESIZE_BOTTOM | ELM_WIN_MOVE_RESIZE_RIGHT
};

static void
_window_move(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   Evas_Event_Mouse_Down *event = info;
   if (event->button != 1) return;
   if (event->flags & EVAS_BUTTON_DOUBLE_CLICK)
     elm_win_maximized_set(data, !elm_win_maximized_get(data));
   else elm_win_move_resize_start(data, ELM_WIN_MOVE_RESIZE_MOVE);
}

static void
_window_resize(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   Window_Frame *frame = data;
   Evas_Event_Mouse_Down *event = info;
   if (event->button != 1 || elm_win_maximized_get(frame->win)) return;
   for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(frame->grips); i++)
     if (obj == frame->grips[i])
       {
          elm_win_move_resize_start(frame->win, resize_modes[i]);
          break;
       }
}

static void
_window_grips_update(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   Window_Frame *frame = data;
   int x, y, w, h, g = ELM_SCALE_SIZE(3);
   evas_object_geometry_get(frame->frame, &x, &y, &w, &h);
   int geometry[8][4] = {
      {x+g,y,w-2*g,g}, {x+g,y+h-g,w-2*g,g},
      {x,y+g,g,h-2*g}, {x+w-g,y+g,g,h-2*g},
      {x,y,g,g}, {x+w-g,y,g,g}, {x,y+h-g,g,g}, {x+w-g,y+h-g,g,g}
   };
   for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(frame->grips); i++)
     {
        evas_object_move(frame->grips[i], geometry[i][0], geometry[i][1]);
        evas_object_resize(frame->grips[i], geometry[i][2], geometry[i][3]);
     }
}

static void
_window_theme_changed(void *data, Evas_Object *obj, void *info)
{
   Window_Frame *frame = data;
   const char *path = elm_theme_group_path_find(NULL, "elaro/dialog/frame");
   if (!path) return;
   elm_object_part_content_unset(frame->frame, "elaro.content");
   elm_layout_file_set(frame->frame, path, "elaro/dialog/frame");
   elm_object_part_content_set(frame->frame, "elaro.content", frame->root);
   for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(frame->icons); i++)
     {
        char group[64];
        snprintf(group, sizeof(group), "elaro/icons/%s", window_icons[i]);
        elm_image_file_set(frame->icons[i], path, group);
     }
   _window_grips_update(frame, NULL, NULL, NULL);
}

static void
_window_frame_deleted(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   Window_Frame *frame = data;
   evas_object_smart_callback_del_full(frame->win, "theme,changed", _window_theme_changed, frame);
   for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(frame->grips); i++)
     evas_object_del(frame->grips[i]);
   free(frame);
}

static void _window_minimize(void *data, Evas_Object *obj, void *info)
{
   elm_win_iconified_set(data, EINA_TRUE);
}
static void _window_maximize(void *data, Evas_Object *obj, void *info)
{
   elm_win_maximized_set(data, !elm_win_maximized_get(data));
}
static void _window_close(void *data, Evas_Object *obj, void *info)
{
   evas_object_del(data);
}

Evas_Object *
elaro_ui_window_content_add(Evas_Object *win)
{
   const char *path = elm_theme_group_path_find(NULL, "elaro/dialog/frame");
   Evas_Object *body = elm_box_add(win);
   evas_object_size_hint_weight_set(body, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(body, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(body);
   /* Retain native decorations if the application theme is unavailable. */
   if (!path)
     {
        elm_win_resize_object_add(win, body);
        return body;
     }
   Window_Frame *frame = calloc(1, sizeof(*frame));
   if (!frame)
     {
        elm_win_resize_object_add(win, body);
        return body;
     }
   frame->win = win;
   elm_win_borderless_set(win, EINA_TRUE);
   frame->frame = elm_layout_add(win);
   elm_layout_file_set(frame->frame, path, "elaro/dialog/frame");
   evas_object_size_hint_weight_set(frame->frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_win_resize_object_add(win, frame->frame);
   frame->root = elm_box_add(frame->frame);
   evas_object_size_hint_weight_set(frame->root, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame->root, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_part_content_set(frame->frame, "elaro.content", frame->root);
   Evas_Object *header = elm_table_add(frame->root);
   elm_table_padding_set(header, ELM_SCALE_SIZE(5), 0);
   evas_object_size_hint_weight_set(header, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(header, EVAS_HINT_FILL, 0.5);
   elaro_ui_fixed_height_set(header, 33);
   elm_box_pack_end(frame->root, header);
   Evas_Object *drag = evas_object_rectangle_add(evas_object_evas_get(win));
   evas_object_color_set(drag, 0, 0, 0, 0);
   evas_object_size_hint_weight_set(drag, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(drag, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elaro_ui_size_min_set(drag, 60, 24);
   elm_table_pack(header, drag, 0, 0, 1, 1);
   evas_object_event_callback_add(drag, EVAS_CALLBACK_MOUSE_DOWN, _window_move, win);
   evas_object_show(drag);
   Evas_Object *title = elm_label_add(header);
   elm_object_style_set(title, "elaro/title");
   char *safe = elm_entry_utf8_to_markup(elm_win_title_get(win));
   elm_object_text_set(title, safe);
   free(safe);
   elm_label_ellipsis_set(title, EINA_TRUE);
   evas_object_size_hint_weight_set(title, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(title, EVAS_HINT_FILL, 0.5);
   evas_object_pass_events_set(title, EINA_TRUE);
   elm_table_pack(header, title, 0, 0, 1, 1);
   evas_object_show(title);
   const char *tooltips[] = { _("Minimize"), _("Maximize or restore"), _("Close") };
   Evas_Smart_Cb callbacks[] = { _window_minimize, _window_maximize, _window_close };
   for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(frame->icons); i++)
     {
        Evas_Object *button = elm_button_add(header);
        elaro_ui_window_control_style_set(button, i == 2);
        frame->icons[i] = elm_icon_add(button);
        char group[64];
        snprintf(group, sizeof(group), "elaro/icons/%s", window_icons[i]);
        elm_image_file_set(frame->icons[i], path, group);
        elaro_ui_size_min_set(frame->icons[i], 19, 19);
        elaro_ui_size_max_set(frame->icons[i], 19, 19);
        elm_object_part_content_set(button, "icon", frame->icons[i]);
        elm_object_tooltip_text_set(button, tooltips[i]);
        elm_object_access_info_set(button, tooltips[i]);
        evas_object_smart_callback_add(button, "clicked", callbacks[i], win);
        elm_table_pack(header, button, i + 1, 0, 1, 1);
        evas_object_show(frame->icons[i]);
        evas_object_show(button);
        evas_object_data_set(win, window_icons[i], button);
     }
   elm_box_pack_end(frame->root, body);
   elaro_ui_padding_set(body, 6, 6, 5, 6);
   evas_object_show(header);
   evas_object_show(frame->root);
   evas_object_show(frame->frame);
   for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(frame->grips); i++)
     {
        frame->grips[i] = evas_object_rectangle_add(evas_object_evas_get(win));
        evas_object_color_set(frame->grips[i], 0, 0, 0, 0);
        evas_object_layer_set(frame->grips[i], EVAS_LAYER_MAX);
        evas_object_event_callback_add(frame->grips[i], EVAS_CALLBACK_MOUSE_DOWN, _window_resize, frame);
        evas_object_show(frame->grips[i]);
     }
   evas_object_event_callback_add(frame->frame, EVAS_CALLBACK_RESIZE, _window_grips_update, frame);
   evas_object_event_callback_add(frame->frame, EVAS_CALLBACK_MOVE, _window_grips_update, frame);
   evas_object_event_callback_add(frame->frame, EVAS_CALLBACK_DEL, _window_frame_deleted, frame);
   evas_object_smart_callback_add(win, "theme,changed", _window_theme_changed, frame);
   evas_object_data_set(win, "elaro-window-drag", drag);
   evas_object_data_set(win, "elaro-window-frame", frame->frame);
   return body;
}
