#ifndef ELARO_UTIL_H
#define ELARO_UTIL_H

#include <Eina.h>
#include <Ecore_File.h>
#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <time.h>

#define ELARO_CLAMP(value, minimum, maximum) MIN(MAX((value), (minimum)), (maximum))

/* Returned strings are owned by the caller and released with free(). */
char *
elaro_string_printf(const char *format, ...) EINA_PRINTF(1, 2);
char *
elaro_path_join(const char *first, ...) EINA_SENTINEL;
char *
elaro_config_dir_get(void);
char *
elaro_file_read(const char *path);
char *
elaro_user_name_get(void);
char *
elaro_executable_find(const char *name);
char *
elaro_command_quote(const char *argument);
Eina_Bool
elaro_email_valid(const char *email);
char *
elaro_email_from_realname(const char *realname);
char *
elaro_avatar_hash_get(const char *email);
char *
elaro_uri_escape(const char *text);
char *
elaro_uri_unescape(const char *text);
const char *
elaro_double_format(char *buffer, size_t size, double value);

#endif
