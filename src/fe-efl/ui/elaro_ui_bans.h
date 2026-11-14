#ifndef ELARO_UI_BANS_H
#define ELARO_UI_BANS_H
#include "elaro_ui.h"
#include "../../engine/elaro_engine.h"
typedef struct _Elaro_Ui_Bans Elaro_Ui_Bans;
Evas_Object *
elaro_ui_bans_heading_add(Elaro_Ui *ui, Evas_Object *parent);
void
elaro_ui_bans_show(void *data, Evas_Object *obj, void *info);
void
elaro_ui_bans_event(Elaro_Ui *ui, const Elaro_Engine_Event *event);
void
elaro_ui_bans_shutdown(Elaro_Ui *ui);
#endif
