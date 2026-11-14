#ifndef ELARO_CORE_SESSION_H
#define ELARO_CORE_SESSION_H

extern char *elaro_binary;

void session_set_binary(const char *path);
void session_upgrade(void);

void session_register_options(void);
void session_init(void);
void session_deinit(void);

#endif
