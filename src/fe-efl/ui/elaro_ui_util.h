#ifndef ELARO_UI_UTIL_H
#define ELARO_UI_UTIL_H

#include "elaro_ui.h"

void
elaro_ui_layout_capture(Elaro_Ui *ui);
void
elaro_ui_layout_restore(Elaro_Ui *ui);
void
elaro_ui_font_apply(Elaro_Ui *ui);
void
elaro_ui_font_set(Evas_Object *entry, const char *font, int size);
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

#endif
