#ifndef ELARO_UI_BUFFERS_H
#define ELARO_UI_BUFFERS_H

#include "elaro_ui.h"
#include "../../engine/elaro_engine.h"

typedef struct _Elaro_Ui_Buffer Elaro_Ui_Buffer;

Elaro_Ui_Buffer *
elaro_ui_buffer_get(Elaro_Ui *ui, const char *server, const char *target);
/* Reveal a hidden child before selecting it. */
void
elaro_ui_buffer_select(Elaro_Ui_Buffer *buffer);

void
elaro_ui_buffer_title_update(Elaro_Ui *ui);
void
elaro_ui_input_record(Elaro_Ui *ui, const char *text);
Eina_Bool
elaro_ui_input_key_down(void *data, Evas_Object *obj, Evas_Event_Key_Down *ev);
void
elaro_ui_topic_apply(void *data, Evas_Object *obj, void *info);
/* Register item classes and callbacks after creating the buffers genlist. */
void
elaro_ui_buffers_init(Elaro_Ui *ui);
void
elaro_ui_buffers_shutdown(Elaro_Ui *ui);

void
elaro_ui_buffer_event(Elaro_Ui *ui, const Elaro_Engine_Event *event);

#endif
