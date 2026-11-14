#ifndef ELARO_UI_LIST_H
#define ELARO_UI_LIST_H
#include "elaro_ui.h"
#include "../../engine/elaro_engine.h"
typedef struct _Elaro_Ui_List Elaro_Ui_List;
/* Batch payload ownership moves from the event to the browser. */
void
elaro_ui_list_event(Elaro_Ui *ui, Elaro_Engine_Event *event);
void
elaro_ui_list_shutdown(Elaro_Ui *ui);
#endif
