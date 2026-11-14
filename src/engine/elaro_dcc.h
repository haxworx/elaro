#ifndef ELARO_DCC_H
#define ELARO_DCC_H
#include "elaro_engine.h"
/* Worker-thread adapter to the original DCC engine. */
void
elaro_dcc_init(void);
/* Call after irc_deinit has closed the original DCC records. */
void
elaro_dcc_shutdown(void);
void
elaro_dcc_command(Elaro_Engine_Command *command);
#endif
