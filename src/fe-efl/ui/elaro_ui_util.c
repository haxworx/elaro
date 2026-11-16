#include "elaro_ui_private.h"

void
elaro_ui_expand(Evas_Object *o)
{
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(o);
}

char *
elaro_ui_field_text_get(Evas_Object *field)
{
   char *s, *copy;

   s = elm_entry_markup_to_utf8(elm_entry_entry_get(field));
   copy = strdup(s ? s : "");
   free(s);

   return copy;
}

/* Single-line inputs share a logical height, including password fields.
 * Capping it prevents spare box space or text contents changing one row. */
Evas_Object *
elaro_ui_entry_add(Evas_Object *parent, Eina_Bool secret)
{
   Evas_Object *entry = elm_entry_add(parent);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_TRUE);
   elm_entry_password_set(entry, secret);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_scroller_policy_set(entry, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_OFF);
   elm_scroller_bounce_set(entry, EINA_FALSE, EINA_FALSE);
   elm_scroller_movement_block_set(entry, ELM_SCROLLER_MOVEMENT_BLOCK_VERTICAL);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, 0.5);
   elaro_ui_fixed_height_set(entry, ELARO_UI_INPUT_HEIGHT);
   evas_object_data_set(entry, "elaro-single-line", (void *)1);
   return entry;
}

typedef struct
{
   Evas_Object *entry;
   Ecore_Timer *timer;
   double position, last_tick, resume_at;
   int direction;
   Eina_Bool reset;
} Entry_Scroll;

static void
_entry_scroll_reset(void *data, Evas_Object *obj, void *info)
{
   Entry_Scroll *scroll = data;
   scroll->reset = EINA_TRUE;
}

static void
_entry_scroll_resize(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   _entry_scroll_reset(data, obj, info);
}

static Eina_Bool
_entry_scroll_tick(void *data)
{
   Entry_Scroll *scroll = data;
   Evas_Object *entry = scroll->entry;
   double now = ecore_loop_time_get();
   double elapsed = MIN(now - scroll->last_tick, 0.1);
   int width, height, content_width;
   scroll->last_tick = now;
   if (!evas_object_visible_get(entry) || elm_object_focus_get(entry)
       || elm_object_disabled_get(entry))
     {
        scroll->reset = EINA_TRUE;
        return ECORE_CALLBACK_RENEW;
     }
   elm_scroller_region_get(entry, NULL, NULL, &width, &height);
   elm_scroller_child_size_get(entry, &content_width, NULL);
   int limit = MAX(0, content_width - width);
   if (scroll->reset || !limit)
     {
        scroll->position = 0;
        scroll->direction = 1;
        scroll->resume_at = now + 1.5;
        scroll->reset = EINA_FALSE;
     }
   else if (now >= scroll->resume_at)
     {
        scroll->position += scroll->direction * ELM_SCALE_SIZE(30) * elapsed;
        if (scroll->position >= limit || scroll->position <= 0)
          {
             scroll->position = ELARO_CLAMP(scroll->position, 0, limit);
             scroll->direction = -scroll->direction;
             scroll->resume_at = now + 1.5;
          }
     }
   if (width > 0 && height > 0)
      elm_scroller_region_show(entry, (int)scroll->position, 0, width, height);
   return ECORE_CALLBACK_RENEW;
}

static void
_entry_scroll_free(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   Entry_Scroll *scroll = data;
   ecore_timer_del(scroll->timer);
   free(scroll);
}

/* The entry retains normal cursor scrolling whenever the user edits it. */
void
elaro_ui_entry_auto_scroll_enable(Evas_Object *entry)
{
   if (evas_object_data_get(entry, "elaro-auto-scroll")) return;
   Entry_Scroll *scroll = calloc(1, sizeof(*scroll));
   if (!scroll) return;
   scroll->entry = entry;
   scroll->last_tick = ecore_loop_time_get();
   scroll->reset = EINA_TRUE;
   scroll->timer = ecore_timer_add(1.0 / 30.0, _entry_scroll_tick, scroll);
   if (!scroll->timer) { free(scroll); return; }
   evas_object_data_set(entry, "elaro-auto-scroll", scroll);
   evas_object_smart_callback_add(entry, "changed", _entry_scroll_reset, scroll);
   evas_object_event_callback_add(entry, EVAS_CALLBACK_RESIZE, _entry_scroll_resize, scroll);
   evas_object_event_callback_add(entry, EVAS_CALLBACK_DEL, _entry_scroll_free, scroll);
}

Evas_Object *
elaro_ui_field_add(Evas_Object *box, const char *label, const char *value, Eina_Bool secret)
{
   Evas_Object *frame, *e;
   char *safe;

   frame = elm_frame_add(box);
   elm_object_text_set(frame, label);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);

   e = elaro_ui_entry_add(frame, secret);
   elm_object_style_set(e, "default");
   elm_entry_single_line_set(e, EINA_TRUE);
   elm_entry_scrollable_set(e, EINA_TRUE);
   elm_entry_editable_set(e, EINA_TRUE);
   elm_entry_password_set(e, secret);
   elm_object_part_text_set(e, "guide", secret ? _("Password") : label);
   safe = elm_entry_utf8_to_markup(value);
   elm_entry_entry_set(e, safe);
   free(safe);
   evas_object_size_hint_weight_set(e, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(e, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elaro_ui_size_min_set(e, 240, ELARO_UI_INPUT_HEIGHT);
   elm_object_content_set(frame, e);
   elm_box_pack_end(box, frame);
   evas_object_show(e);
   evas_object_show(frame);

   return e;
}

Evas_Object *
elaro_ui_button_add(Evas_Object *parent, const char *text, Evas_Smart_Cb cb, Elaro_Ui *ui)
{
   Evas_Object *b;

   b = elm_button_add(parent);
   elm_object_text_set(b, text);
   evas_object_smart_callback_add(b, "clicked", cb, ui);
   evas_object_show(b);

   return b;
}

Evas_Object *
elaro_ui_read_view_add(Evas_Object *parent)
{
   Evas_Object *e;

   e = elm_entry_add(parent);
   elm_entry_editable_set(e, EINA_FALSE);
   elm_entry_scrollable_set(e, EINA_TRUE);
   elm_entry_line_wrap_set(e, ELM_WRAP_WORD);
   elaro_ui_expand(e);

   return e;
}

static void
_theme_icon_deleted(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui = data;
   ui->theme_icons = eina_list_remove(ui->theme_icons, obj);
   free(evas_object_data_get(obj, "elaro-icon-group"));
}

Eina_Bool
elaro_ui_icon_set(Elaro_Ui *ui, Evas_Object *icon, const char *asset)
{
   char *group;
   Eina_Bool ok;
   if (!ui->theme_path) return elm_icon_standard_set(icon, asset);
   group = elaro_string_printf("elaro/icons/%s", asset);
   ok = elm_image_file_set(icon, ui->theme_path, group);
   if (ok)
     {
        evas_object_data_set(icon, "elaro-icon-group", group);
        ui->theme_icons = eina_list_append(ui->theme_icons, icon);
        evas_object_event_callback_add(icon, EVAS_CALLBACK_DEL, _theme_icon_deleted, ui);
     }
   else free(group);
   return ok;
}

Evas_Object *
elaro_ui_icon_button_add(Evas_Object *parent, const char *asset, const char *tooltip,
                         Evas_Smart_Cb cb, Elaro_Ui *ui)
{
   Evas_Object *button, *icon;
   button = elaro_ui_button_add(parent, NULL, cb, ui);
   icon = elm_icon_add(button);
   elaro_ui_icon_set(ui, icon, asset);
   elaro_ui_size_min_set(icon, 17, 17);
   elaro_ui_size_max_set(icon, 17, 17);
   elm_object_part_content_set(button, "icon", icon);
   evas_object_show(icon);
   elm_object_tooltip_text_set(button, tooltip);
   elm_object_access_info_set(button, tooltip);
   return button;
}

/* Shared presentation for image and audio previews; callers own the media state. */
Evas_Object *
elaro_ui_media_box_add(Evas_Object *popup, const char *url, const char *fallback)
{
   Evas_Object *box, *title, *source;
   char *path, *filename, *safe, *markup, *host;
   const char *slash, *authority, *at;

   box = elm_box_add(popup);
   elaro_ui_box_padding_set(box, 0, 6);
   elaro_ui_padding_set(box, 6, 6,
                                    6, 6);
   elm_object_content_set(popup, box);
   evas_object_show(box);

   path = strndup(url, strcspn(url, "?#"));
   if (!path) return box;
   slash = strrchr(path, '/');
   filename = elaro_uri_unescape(slash ? slash + 1 : path);
   safe = elm_entry_utf8_to_markup(filename && *filename ? filename : fallback);
   markup = elaro_string_printf("<align=left><b>%s</b></align>", safe);
   title = elm_label_add(box);
   elm_label_ellipsis_set(title, EINA_TRUE);
   elm_object_text_set(title, markup);
   evas_object_size_hint_weight_set(title, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(title, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(box, title);
   evas_object_show(title);
   free(markup);
   free(safe);
   free(filename);
   free(path);

   authority = strstr(url, "://");
   if (authority)
     {
        authority += 3;
        host = strndup(authority, strcspn(authority, "/?#"));
        if (!host) return box;
        at = strrchr(host, '@');
        safe = elm_entry_utf8_to_markup(at ? at + 1 : host);
        source = elm_label_add(box);
        elm_label_ellipsis_set(source, EINA_TRUE);
        markup = elaro_string_printf("<align=left>%s</align>", safe);
        elm_object_text_set(source, markup);
        free(markup);
        evas_object_size_hint_weight_set(source, EVAS_HINT_EXPAND, 0);
        evas_object_size_hint_align_set(source, EVAS_HINT_FILL, 0.5);
        elm_box_pack_end(box, source);
        evas_object_show(source);
        free(safe);
        free(host);
     }
   return box;
}

void
elaro_ui_opacity_apply(Elaro_Ui *ui)
{
   int opacity, alpha;

   opacity = ELARO_CLAMP(ui->config->window_opacity, ELARO_OPACITY_MIN, ELARO_OPACITY_MAX);
   alpha = ui->config->window_transparent ? (255 * opacity + 50) / 100 : 255;
   elm_win_alpha_set(ui->win, ui->config->window_transparent);
   evas_object_color_set(ui->background, alpha, alpha, alpha, alpha);
   /* Setting the background object's colour also colours its rectangle.
    * Reset that rectangle afterwards so the theme supplies the background. */
   elm_bg_color_set(ui->background, -1, -1, -1);
   if (ui->frame)
     {
        evas_object_color_set(ui->content, 255, 255, 255, 255);
        evas_object_color_set(ui->frame, alpha, alpha, alpha, alpha);
     }
   else evas_object_color_set(ui->content, alpha, alpha, alpha, alpha);
}

/* Panes use proportions so restored widths also adapt to a different display. */
void
elaro_ui_layout_capture(Elaro_Ui *ui)
{
   if (!elm_win_maximized_get(ui->win) && !elm_win_fullscreen_get(ui->win))
     {
        evas_object_geometry_get(ui->win, NULL, NULL, &ui->config->window_width,
                                 &ui->config->window_height);
        ui->config->window_size_default = EINA_FALSE;
        elm_win_screen_position_get(ui->win, &ui->config->window_x, &ui->config->window_y);
        ui->config->window_position_set = EINA_TRUE;
     }
   if (ui->buffer_panes)
      ui->config->buffer_panel_ratio = elm_panes_content_left_size_get(ui->buffer_panes);
   if (ui->user_panes && !ui->user_panel_collapsed && !ui->user_panel_animator)
      ui->config->user_panel_ratio = elm_panes_content_right_size_get(ui->user_panes);
}

void
elaro_ui_layout_restore(Elaro_Ui *ui)
{
   int width = ui->config->window_width, height = ui->config->window_height;
   if (ui->config->window_size_default)
     {
        int screen_width = 0, screen_height = 0;
        width = ELM_SCALE_SIZE(width);
        height = ELM_SCALE_SIZE(height);
        elm_win_screen_size_get(ui->win, NULL, NULL, &screen_width, &screen_height);
        if (screen_width > 0) width = MIN(width, screen_width * 9 / 10);
        if (screen_height > 0) height = MIN(height, screen_height * 9 / 10);
     }
   if (ui->buffer_panes)
      elm_panes_content_left_size_set(ui->buffer_panes, ui->config->buffer_panel_ratio);
   if (ui->user_panes)
     {
        ui->user_panel_saved_ratio = ui->config->user_panel_ratio;
        elm_panes_content_right_size_set(ui->user_panes,
                                       ui->user_panel_collapsed ? 0.0 : ui->config->user_panel_ratio);
     }
   evas_object_resize(ui->win, width, height);
   ui->config->window_width = width;
   ui->config->window_height = height;
   ui->config->window_size_default = EINA_FALSE;
   if (ui->config->window_position_set)
      evas_object_move(ui->win, ui->config->window_x, ui->config->window_y);
}

Evas_Object *
elaro_ui_popup_close_button_add(Elaro_Ui *ui, Evas_Object *popup,
                                Evas_Smart_Cb callback, void *data)
{
   Evas_Object *button = elm_button_add(popup);
   Evas_Object *icon = elm_icon_add(button);
   elaro_ui_window_control_style_set(button, EINA_TRUE);
   elaro_ui_icon_set(ui, icon, "close");
   elaro_ui_size_min_set(icon, 19, 19);
   elaro_ui_size_max_set(icon, 19, 19);
   elm_object_part_content_set(button, "icon", icon);
   elm_object_tooltip_text_set(button, _("Close"));
   elm_object_access_info_set(button, _("Close"));
   evas_object_smart_callback_add(button, "clicked", callback, data);
   elm_object_part_content_set(popup, "title,icon", button);
   evas_object_data_set(popup, "close", button);
   evas_object_show(icon);
   evas_object_show(button);
   return button;
}

static void
_popup_dismissed(void *data, Evas_Object *obj, void *info)
{
   evas_object_del(obj);
}

void
elaro_ui_popup_show(Evas_Object *popup)
{
   if (!evas_object_data_get(popup, "elaro-popup-lifecycle"))
     {
        evas_object_data_set(popup, "elaro-popup-lifecycle", (void *)1);
        evas_object_smart_callback_add(popup, "dismissed", _popup_dismissed, NULL);
     }
   evas_object_show(popup);
}

void
elaro_ui_popup_close(Evas_Object *popup)
{
   if (!popup || evas_object_data_get(popup, "elaro-popup-closing")) return;
   evas_object_data_set(popup, "elaro-popup-closing", (void *)1);
   /* Let widget destruction unregister focus descendants. Changing the tree's
    * focus policy first can make EFL re-register a closing modal subtree. */
   evas_object_del(popup);
}

Evas_Object *
elaro_ui_tabs_add(Evas_Object *parent)
{
   Evas_Object *tabs = elm_segment_control_add(parent);
   elm_object_style_set(tabs, "default");
   elaro_ui_size_min_set(tabs, 0, 27);
   evas_object_size_hint_weight_set(tabs, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(tabs, EVAS_HINT_FILL, 0.5);
   evas_object_show(tabs);
   return tabs;
}

typedef struct
{
   Evas_Object *view;
   Ecore_Animator *animator;
} View_Reveal;

static void
_view_reveal_deleted(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   View_Reveal *reveal = data;
   ecore_animator_del(reveal->animator);
   free(reveal);
}

static Eina_Bool
_view_reveal_frame(void *data, double position)
{
   View_Reveal *reveal = data;
   double progress = 1.0 - (1.0 - position) * (1.0 - position);
   int alpha = 255 * progress;
   evas_object_color_set(reveal->view, alpha, alpha, alpha, alpha);
   if (position < 1.0) return ECORE_CALLBACK_RENEW;
   evas_object_data_del(reveal->view, "elaro-view-reveal");
   evas_object_event_callback_del_full(reveal->view, EVAS_CALLBACK_DEL, _view_reveal_deleted, reveal);
   free(reveal);
   return ECORE_CALLBACK_CANCEL;
}

void
elaro_ui_view_reveal(Evas_Object *view)
{
   View_Reveal *reveal = evas_object_data_get(view, "elaro-view-reveal");
   if (reveal)
     {
        evas_object_event_callback_del_full(view, EVAS_CALLBACK_DEL, _view_reveal_deleted, reveal);
        _view_reveal_deleted(reveal, NULL, view, NULL);
        evas_object_data_del(view, "elaro-view-reveal");
     }
   evas_object_color_set(view, 255, 255, 255, 255);
   reveal = calloc(1, sizeof(*reveal));
   if (!reveal) return;
   reveal->view = view;
   reveal->animator = ecore_animator_timeline_add(0.14, _view_reveal_frame, reveal);
   if (!reveal->animator) { free(reveal); return; }
   evas_object_color_set(view, 0, 0, 0, 0);
   evas_object_data_set(view, "elaro-view-reveal", reveal);
   evas_object_event_callback_add(view, EVAS_CALLBACK_DEL, _view_reveal_deleted, reveal);
}
