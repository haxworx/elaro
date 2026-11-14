#ifndef ELARO_UI_TEXT_H
#define ELARO_UI_TEXT_H
#include "../elaro_util.h"
Eina_Bool
elaro_ui_text_audio_url_check(const char *url);
Eina_Bool
elaro_ui_text_image_url_check(const char *url);
/* Convert IRC formatting into escaped, balanced Elementary text markup. */
char *
elaro_ui_text_markup(const char *text);
#endif
