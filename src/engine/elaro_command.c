#include "elaro_engine.h"
#include <stdlib.h>

void
elaro_engine_command_free(Elaro_Engine_Command *c)
{
   if (!c) return;
   free(c->server);
   free(c->target);
   free(c->text);
   free(c->host);
   free(c->nick);
   free(c->username);
   free(c->realname);
   free(c->email);
   free(c->password);
   free(c->channels);
   free(c->sasl_username);
   free(c->sasl_password);
   free(c);
}
