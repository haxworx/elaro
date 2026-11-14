#ifndef ELARO_UI_HISTORY_H
#define ELARO_UI_HISTORY_H

#include "elaro_ui.h"

/* Read-only chat view with aligned metadata, wrapping and clipboard support. */
Evas_Object *
elaro_ui_history_add(Evas_Object *parent);

/* Reload the current conversation's retained history, starting a fresh group. */
void
elaro_ui_history_refresh(Elaro_Ui *ui);

/* Append an IRC-formatted, escaped markup line to the current view. */
void
elaro_ui_history_append(Elaro_Ui *ui, const char *markup);

#endif
