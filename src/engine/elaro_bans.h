#ifndef ELARO_BANS_H
#define ELARO_BANS_H
#include "elaro_engine.h"
void
elaro_bans_init(void);
void
elaro_bans_deinit(void);
void
elaro_bans_command(const Elaro_Engine_Command *command);
#endif
