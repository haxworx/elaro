#ifndef ELARO_UI_NOTIFY_H
#define ELARO_UI_NOTIFY_H

#include "elaro_ui.h"

void
elaro_ui_notify_sound(Elaro_Ui *ui);

void
elaro_ui_notify_private(const char *server, const char *nick, const char *message);

#endif
