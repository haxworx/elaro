#ifndef ELARO_UI_USERS_H
#define ELARO_UI_USERS_H

#include "elaro_ui.h"
#include "../../engine/elaro_engine.h"

Eina_Bool
elaro_ui_users_complete(void *data, Evas_Object *obj, Evas_Event_Key_Down *ev);
void
elaro_ui_users_details(Elaro_Ui *ui, const Elaro_Engine_Event *event);
void
elaro_ui_users_save(Elaro_Ui *ui);
void
elaro_ui_users_clear(Elaro_Ui *ui);
void
elaro_ui_users_update(Elaro_Ui *ui, const Elaro_Engine_User *users, unsigned int count);
void
elaro_ui_users_presence(Elaro_Ui *ui, const Elaro_Engine_Event *event);
void
elaro_ui_users_init(Elaro_Ui *ui);
void
elaro_ui_users_shutdown(Elaro_Ui *ui);

#endif
