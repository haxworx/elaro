#ifndef ELARO_UI_UTIL_H
#define ELARO_UI_UTIL_H

#include "elaro_ui.h"

enum { ELARO_UI_INPUT_HEIGHT = 36 };

Evas_Object *elaro_ui_entry_add(Evas_Object *parent, Eina_Bool secret);

/* Scroll overflowing text back and forth while the entry is unfocused. */
void elaro_ui_entry_auto_scroll_enable(Evas_Object *entry);

Eina_Bool
elaro_ui_icon_set(Elaro_Ui *ui, Evas_Object *icon, const char *asset);
void
elaro_ui_layout_capture(Elaro_Ui *ui);
void
elaro_ui_layout_restore(Elaro_Ui *ui);
void
elaro_ui_opacity_apply(Elaro_Ui *ui);
void
elaro_ui_expand(Evas_Object *o);
char *
elaro_ui_field_text_get(Evas_Object *field);
Evas_Object *
elaro_ui_field_add(Evas_Object *box, const char *label, const char *value, Eina_Bool secret);
Evas_Object *
elaro_ui_button_add(Evas_Object *parent, const char *text, Evas_Smart_Cb cb, Elaro_Ui *ui);
Evas_Object *
elaro_ui_icon_button_add(Evas_Object *parent, const char *asset, const char *tooltip,
                         Evas_Smart_Cb cb, Elaro_Ui *ui);
Evas_Object *
elaro_ui_media_box_add(Evas_Object *popup, const char *url, const char *fallback);
Evas_Object *
elaro_ui_read_view_add(Evas_Object *parent);

Evas_Object *elaro_ui_popup_close_button_add(Elaro_Ui *ui, Evas_Object *popup,
                                            Evas_Smart_Cb callback, void *data);
void elaro_ui_popup_show(Evas_Object *popup);
void elaro_ui_popup_close(Evas_Object *popup);
Evas_Object *elaro_ui_tabs_add(Evas_Object *parent);
Evas_Object *elaro_ui_window_content_add(Evas_Object *win);
void elaro_ui_window_control_style_set(Evas_Object *button, Eina_Bool close);
void elaro_ui_view_reveal(Evas_Object *view);

void elaro_ui_wrap_width_set(Evas_Object *obj, int width);

/* Logical dimensions, reapplied when Elementary changes the widget scale. */
void elaro_ui_fixed_height_set(Evas_Object *obj, int height);
void elaro_ui_size_min_set(Evas_Object *obj, int w, int h);
void elaro_ui_size_max_set(Evas_Object *obj, int w, int h);
void elaro_ui_padding_set(Evas_Object *obj, int l, int r, int t, int b);
void elaro_ui_box_padding_set(Evas_Object *obj, int x, int y);

#endif
