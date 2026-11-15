#include <openssl/sha.h>
#include "elaro_util.h"
#include <stdarg.h>
#include <unistd.h>
#include <pwd.h>
#include <locale.h>

char *
elaro_config_dir_get(void)
{
   const char *base;

   base = getenv("XDG_CONFIG_HOME");
   if (base && base[0] == '/') return elaro_path_join(base, "elaro", NULL);
   return elaro_path_join(eina_environment_home_get(), ".config", "elaro", NULL);
}

char *
elaro_string_printf(const char *format, ...)
{
   Eina_Strbuf *buffer;
   va_list args;

   buffer = eina_strbuf_new();
   if (!buffer) return NULL;
   va_start(args, format);
   eina_strbuf_append_vprintf(buffer, format, args);
   va_end(args);

   return eina_strbuf_release(buffer);
}

char *
elaro_path_join(const char *first, ...)
{
   Eina_Strbuf *buffer;
   const char *part;
   va_list args;
   size_t length;

   if (!first) return NULL;
   buffer = eina_strbuf_new();
   if (!buffer) return NULL;
   eina_strbuf_append(buffer, first);
   va_start(args, first);
   while ((part = va_arg(args, const char *)))
     {
        length = eina_strbuf_length_get(buffer);
        if (length && eina_strbuf_string_get(buffer)[length - 1] != '/')
           eina_strbuf_append_char(buffer, '/');
        while (*part == '/')
           part++;
        eina_strbuf_append(buffer, part);
     }
   va_end(args);

   return eina_strbuf_release(buffer);
}

char *
elaro_file_read(const char *path)
{
   Eina_File *file;
   const char *map;
   char *text;
   size_t size;

   file = eina_file_open(path, EINA_FALSE);
   if (!file) return NULL;
   size = eina_file_size_get(file);
   map = size ? eina_file_map_all(file, EINA_FILE_SEQUENTIAL) : NULL;
   text = size ? (map ? strndup(map, size) : NULL) : strdup("");
   if (map) eina_file_map_free(file, (void *)map);
   eina_file_close(file);

   if (!text) return NULL;
   return text;
}

char *
elaro_user_name_get(void)
{
   struct passwd record, *result;
   char *buffer, *name;
   long size;

   size = sysconf(_SC_GETPW_R_SIZE_MAX);
   if (size < 1024) size = 16384;
   buffer = malloc(size);
   if (!buffer) return NULL;
   result = NULL;
   if (buffer && !getpwuid_r(getuid(), &record, buffer, size, &result) && result)
      name = strdup(record.pw_name);
   else name = strdup("elaro");
   free(buffer);

   if (!name) return NULL;
   return name;
}

char *
elaro_executable_find(const char *name)
{
   char **paths, **p, *candidate;
   const char *search;

   if (strchr(name, '/')) return realpath(name, NULL);
   search = getenv("PATH");
   paths = eina_str_split(search ? search : "/usr/local/bin:/usr/bin:/bin", ":", 0);
   candidate = NULL;
   for (p = paths; p && *p; p++)
     {
        candidate = elaro_path_join(**p ? *p : ".", name, NULL);
        if (!candidate) break;
        if (ecore_file_can_exec(candidate) && !ecore_file_is_dir(candidate)) break;
        free(candidate);
        candidate = NULL;
     }
   if (paths) free(paths[0]);
   free(paths);

   return candidate;
}

char *
elaro_command_quote(const char *argument)
{
   Eina_Strbuf *buffer;
   const char *p;

   buffer = eina_strbuf_new();
   if (!buffer) return NULL;
   eina_strbuf_append_char(buffer, '\'');
   for (p = argument; *p; p++)
     {
        if (*p == '\'') eina_strbuf_append(buffer, "'\\''");
        else eina_strbuf_append_char(buffer, *p);
     }
   eina_strbuf_append_char(buffer, '\'');

   return eina_strbuf_release(buffer);
}

char *
elaro_uri_escape(const char *text)
{
   Eina_Strbuf *buffer;
   const unsigned char *p;

   buffer = eina_strbuf_new();
   if (!buffer) return NULL;
   for (p = (const unsigned char *)text; *p; p++)
     {
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9')
            || strchr("-._~", *p))
           eina_strbuf_append_char(buffer, *p);
        else eina_strbuf_append_printf(buffer, "%%%02X", *p);
     }

   return eina_strbuf_release(buffer);
}

static int
_hex_value(unsigned char c)
{
   if (c >= '0' && c <= '9') return c - '0';
   if (c >= 'a' && c <= 'f') return c - 'a' + 10;
   if (c >= 'A' && c <= 'F') return c - 'A' + 10;
   return -1;
}

char *
elaro_uri_unescape(const char *text)
{
   Eina_Strbuf *buffer;
   const unsigned char *p;
   int high, low;

   buffer = eina_strbuf_new();
   if (!buffer) return NULL;
   for (p = (const unsigned char *)text; *p; p++)
     {
        if (*p != '%') eina_strbuf_append_char(buffer, *p);
        else
          {
             if (!p[1] || !p[2]) goto invalid;
             high = _hex_value(p[1]);
             low = _hex_value(p[2]);
             if (high < 0 || low < 0 || !(high || low)) goto invalid;
             eina_strbuf_append_char(buffer, high * 16 + low);
             p += 2;
          }
     }

   return eina_strbuf_release(buffer);
invalid:
   eina_strbuf_free(buffer);
   return NULL;
}

const char *
elaro_double_format(char *buffer, size_t size, double value)
{
   locale_t numeric, previous;

   numeric = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
   if (!numeric) return NULL;
   previous = uselocale(numeric);
   snprintf(buffer, size, "%.17g", value);
   uselocale(previous);
   freelocale(numeric);

   return buffer;
}

/* Email normalization and SHA-256 lookup follow EDI's edi_scm_avatar_url_get
 * (GPL-2.0). Use the existing OpenSSL dependency for the digest. */
Eina_Bool
elaro_email_valid(const char *email)
{
   const char *at, *p;

   if (!email || !*email || strlen(email) > 254) return EINA_FALSE;
   at = strchr(email, '@');
   if (!at || at == email || !at[1] || strchr(at + 1, '@')) return EINA_FALSE;
   for (p = email; *p; p++)
      if ((unsigned char)*p <= 32 || (unsigned char)*p >= 127 || strchr("<>\"\\", *p))
         return EINA_FALSE;
   return EINA_TRUE;
}

char *
elaro_email_from_realname(const char *realname)
{
   const char *start, *end;
   char *email;

   if (!realname) return NULL;
   start = strrchr(realname, '<');
   if (!start) return NULL;
   end = strchr(++start, '>');
   if (!end || end[1]) return NULL;
   email = strndup(start, end - start);
   if (!email) return NULL;
   if (elaro_email_valid(email)) return email;
   free(email);
   return NULL;
}

char *
elaro_avatar_hash_get(const char *email)
{
   const char *start, *end;
   char *normalized, *result;
   unsigned char digest[SHA256_DIGEST_LENGTH];
   unsigned int i;
   static const char hex[] = "0123456789abcdef";

   if (!email) return NULL;
   start = email;
   while (isspace((unsigned char)*start))
      start++;
   end = email + strlen(email);
   while (end > start && isspace((unsigned char)end[-1]))
      end--;
   normalized = strndup(start, end - start);
   if (!normalized) return NULL;
   if (!elaro_email_valid(normalized))
     {
        free(normalized);
        return NULL;
     }
   for (i = 0; normalized[i]; i++)
      normalized[i] = tolower((unsigned char)normalized[i]);
   SHA256((unsigned char *)normalized, strlen(normalized), digest);
   free(normalized);
   result = malloc(SHA256_DIGEST_LENGTH * 2 + 1);
   if (!result) return NULL;
   for (i = 0; i < SHA256_DIGEST_LENGTH; i++)
     {
        result[2 * i] = hex[digest[i] >> 4];
        result[2 * i + 1] = hex[digest[i] & 15];
     }
   result[2 * SHA256_DIGEST_LENGTH] = 0;
   return result;
}
