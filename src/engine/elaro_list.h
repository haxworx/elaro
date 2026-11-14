#ifndef ELARO_LIST_H
#define ELARO_LIST_H
#include "elaro_engine.h"
void
elaro_list_init(void);
void
elaro_list_deinit(void);
void
elaro_list_command(const Elaro_Engine_Command *command);
#endif
