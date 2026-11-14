#ifndef ELARO_ENGINE_PRIVATE_H
#define ELARO_ENGINE_PRIVATE_H
#include "elaro_engine.h"
/* Worker only; transfers ownership of a copied event to the UI queue. */
void
elaro_engine_event_push(Elaro_Engine_Event *event);
#endif
