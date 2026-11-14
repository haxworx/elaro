#ifndef ELARO_UI_AVATAR_H
#define ELARO_UI_AVATAR_H

#include "elaro_ui.h"

void
elaro_ui_avatar_placeholder_set(Evas_Object *image);
void
elaro_ui_avatar_load(Elaro_Ui *ui, const char *email, Evas_Object *image);
void
elaro_ui_avatars_shutdown(Elaro_Ui *ui);

#endif
