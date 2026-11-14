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

Evas_Object *
elaro_ui_field_add(Evas_Object *box, const char *label, const char *value, Eina_Bool secret)
{
   Evas_Object *frame, *e;
   char *safe;

   frame = elm_frame_add(box);
   elm_object_text_set(frame, label);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);

   e = elm_entry_add(frame);
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
   evas_object_size_hint_min_set(e, 320, 30);
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

Evas_Object *
elaro_ui_icon_button_add(Evas_Object *parent, const char *asset, const char *tooltip,
                         Evas_Smart_Cb cb, Elaro_Ui *ui)
{
   Evas_Object *button, *icon;
   char *group, *path;

   button = elaro_ui_button_add(parent, NULL, cb, ui);

   icon = elm_icon_add(button);
   group = elaro_string_printf("elaro/icons/%s", asset);
   path = elaro_path_join(ELARO_DATA_DIR, "elaro.edj", NULL);

   if (!elm_image_file_set(icon, path, group))
     {
        free(path);
        path = elaro_path_join(ELARO_ASSET_BUILD_DIR, "elaro.edj", NULL);
        elm_image_file_set(icon, path, group);
     }
   free(path);
   free(group);
   evas_object_size_hint_min_set(icon, ELM_SCALE_SIZE(24), ELM_SCALE_SIZE(24));
   evas_object_size_hint_max_set(icon, ELM_SCALE_SIZE(24), ELM_SCALE_SIZE(24));
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
   elm_box_padding_set(box, 0, ELM_SCALE_SIZE(8));
   evas_object_size_hint_padding_set(box, ELM_SCALE_SIZE(8), ELM_SCALE_SIZE(8),
                                    ELM_SCALE_SIZE(8), ELM_SCALE_SIZE(8));
   elm_object_content_set(popup, box);
   evas_object_show(box);

   path = strndup(url, strcspn(url, "?#"));
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
   evas_object_color_set(ui->content, alpha, alpha, alpha, alpha);
}

void
elaro_ui_font_set(Evas_Object *entry, const char *font, int size)
{
   Eina_Strbuf *style;
   const char *p;

   if (!entry || !font) return;
   style = eina_strbuf_new();
   eina_strbuf_append(style, "DEFAULT='font=\\'");
   for (p = font; *p; p++)
     {
        if (*p == '\\' || *p == '\"' || *p == '\'') eina_strbuf_append_char(style, '\\');
        eina_strbuf_append_char(style, *p);
     }
   eina_strbuf_append_printf(style, "\\' font_size=%d'", ELARO_CLAMP(size, 8, 36));
   if (evas_object_data_get(entry, "chat-font-style")) elm_entry_text_style_user_pop(entry);
   elm_entry_text_style_user_push(entry, eina_strbuf_string_get(style));
   evas_object_data_set(entry, "chat-font-style", (void *)1);
   eina_strbuf_free(style);
   evas_object_smart_callback_call(entry, "chat,font,changed", NULL);
}

void
elaro_ui_font_apply(Elaro_Ui *ui)
{
   elaro_ui_font_set(ui->history, ui->config->chat_font, ui->config->chat_font_size);
   elaro_ui_font_set(ui->input, ui->config->chat_font, ui->config->chat_font_size);
}

/* Panes use proportions so restored widths also adapt to a different display. */
void
elaro_ui_layout_capture(Elaro_Ui *ui)
{
   if (!elm_win_maximized_get(ui->win) && !elm_win_fullscreen_get(ui->win))
     {
        evas_object_geometry_get(ui->win, NULL, NULL, &ui->config->window_width,
                                 &ui->config->window_height);
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
   if (ui->buffer_panes)
      elm_panes_content_left_size_set(ui->buffer_panes, ui->config->buffer_panel_ratio);
   if (ui->user_panes)
     {
        ui->user_panel_saved_ratio = ui->config->user_panel_ratio;
        elm_panes_content_right_size_set(ui->user_panes,
                                       ui->user_panel_collapsed ? 0.0 : ui->config->user_panel_ratio);
     }
   evas_object_resize(ui->win, ui->config->window_width, ui->config->window_height);
   if (ui->config->window_position_set)
      evas_object_move(ui->win, ui->config->window_x, ui->config->window_y);
}
