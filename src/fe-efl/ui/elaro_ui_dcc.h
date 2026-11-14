#ifndef ELARO_UI_DCC_H
#define ELARO_UI_DCC_H

#include "elaro_ui.h"
#include "../../engine/elaro_engine.h"

void
elaro_ui_dcc_event(Elaro_Ui *ui, const Elaro_Engine_Event *event);
void
elaro_ui_dcc_send_show(Elaro_Ui *ui, const char *server, const char *nick);
void
elaro_ui_dcc_init(Elaro_Ui *ui);
void
elaro_ui_dcc_shutdown(Elaro_Ui *ui);

#endif
