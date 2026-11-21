#ifndef ELARO_UI_TEXT_H
#define ELARO_UI_TEXT_H
#include "../elaro_util.h"
#include "../../engine/elaro_nick_match.h"
Eina_Bool
elaro_ui_text_audio_url_check(const char *url);
Eina_Bool
elaro_ui_text_image_url_check(const char *url);
/* Convert IRC formatting into escaped, balanced Elementary text markup. */
char *
elaro_ui_text_markup(const char *text);
char *
elaro_ui_text_message_markup(const char *text, size_t body_offset, const char *nick,
                             Elaro_Nick_Compare compare);
#endif
