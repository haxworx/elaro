#ifndef ELARO_CTCP_H
#define ELARO_CTCP_H

#include <glib.h>

/* Returned plain text is owned by the caller. now is Unix time in microseconds. */
char *
elaro_ctcp_reply_format(const char *nick, const char *reply, gint64 now);
void
elaro_ctcp_init(void);
void
elaro_ctcp_deinit(void);

#endif
