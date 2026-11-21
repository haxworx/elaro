#ifndef ELARO_NICK_MATCH_H
#define ELARO_NICK_MATCH_H

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef int (*Elaro_Nick_Compare)(const char *, const char *);

/* RFC1459 punctuation can be part of a nickname. */
static inline bool
elaro_nick_character(unsigned char c)
{
   return c >= 128 || (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z')
          || (c >= 'a' && c <= 'z') || (c && strchr("-[]\\`^{}_|", c));
}

static inline bool
elaro_nick_matches(const char *start, const char *p, const char *nick, size_t length,
                   Elaro_Nick_Compare compare)
{
   if ((p != start && elaro_nick_character(p[-1])) || strnlen(p, length) < length
       || elaro_nick_character(p[length])) return false;
   char *candidate = strndup(p, length);
   bool match = candidate && !compare(candidate, nick);
   free(candidate);
   return match;
}

#endif
