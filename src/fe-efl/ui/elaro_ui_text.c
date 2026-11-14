#include <Elementary.h>
#include "../elaro_util.h"
#include <string.h>
#include "elaro_ui_text.h"

Eina_Bool
elaro_ui_text_image_url_check(const char *url)
{
   const char *end;
   static const char *extensions[]
       = { ".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp", ".svg", ".tif", ".tiff", ".avif" };
   unsigned int i;
   size_t length;

   if (!url || (strncasecmp(url, "http://", 7) && strncasecmp(url, "https://", 8)))
      return EINA_FALSE;
   end = url + strcspn(url, "?#");

   for (i = 0; i < EINA_C_ARRAY_LENGTH(extensions); i++)
     {
        length = strlen(extensions[i]);

        if ((size_t)(end - url) > length && !strncasecmp(end - length, extensions[i], length))
           return EINA_TRUE;
     }

   return EINA_FALSE;
}

Eina_Bool
elaro_ui_text_audio_url_check(const char *url)
{
   const char *end;
   static const char *extensions[]
       = { ".mp3", ".ogg", ".oga", ".opus", ".wav", ".flac", ".m4a", ".aac", ".aif", ".aiff" };
   unsigned int i;
   size_t length;

   if (!url || (strncasecmp(url, "http://", 7) && strncasecmp(url, "https://", 8)))
      return EINA_FALSE;
   end = url + strcspn(url, "?#");

   for (i = 0; i < EINA_C_ARRAY_LENGTH(extensions); i++)
     {
        length = strlen(extensions[i]);

        if ((size_t)(end - url) > length && !strncasecmp(end - length, extensions[i], length))
           return EINA_TRUE;
     }

   return EINA_FALSE;
}

static void
_append_escaped(Eina_Strbuf *out, const char *text, ssize_t length)
{
   char *part, *safe;

   part = length < 0 ? strdup(text) : strndup(text, length);
   safe = elm_entry_utf8_to_markup(part);
   eina_strbuf_append(out, safe);
   free(safe);
   free(part);
}

static void
_append_text(Eina_Strbuf *out, Eina_Strbuf *text)
{
   const char *cursor, *start, *end, *closing, *p;
   char opening;
   int balance;
   char *url, *attribute;

   cursor = eina_strbuf_string_get(text);

   while (*cursor)
     {
        start = cursor;

        while (*start && strncasecmp(start, "http://", 7) && strncasecmp(start, "https://", 8))
           start++;
        _append_escaped(out, cursor, start - cursor);

        if (!*start) break;

        end = start;

        while (*end && !strchr(" \t\n\r\f\v", *end) && !strchr("<>\"'", *end))
           end++;

        while (end > start)
          {
             if (strchr(".,;!", end[-1]))
               {
                  end--;
                  continue;
               }
             closing = strchr(")]}", end[-1]);

             if (!closing) break;

             opening = "([{"[closing - ")]}"];
             balance = 0;

             for (p = start; p < end; p++)
                balance += (*p == opening) - (*p == end[-1]);

             if (balance >= 0) break;

             end--;
          }
        url = strndup(start, end - start);
        /* Anchor names are opaque tokens; encode URLs before placing them in markup. */
        attribute = elaro_uri_escape(url);
        eina_strbuf_append_printf(out, "<a href=%s:%s>",
                                  elaro_ui_text_image_url_check(url)   ? "image"
                                  : elaro_ui_text_audio_url_check(url) ? "audio"
                                                                       : "url",
                                  attribute);
        _append_escaped(out, url, -1);
        eina_strbuf_append(out, "</a>");
        free(attribute);
        free(url);
        cursor = end;
     }
   eina_strbuf_reset(text);
}

static void
_styles(Eina_Strbuf *out, unsigned int flags, Eina_Bool open)
{
   static const char *tags[] = { "b", "i", "u" };
   int n;
   int i;

   for (n = 0; n < 3; n++)
     {
        i = open ? n : 2 - n;

        if (flags & (1u << i)) eina_strbuf_append_printf(out, "<%s%s>", open ? "" : "/", tags[i]);
     }
}

static const unsigned char *
_color(const unsigned char *p, int digits, Eina_Bool hex)
{
   const unsigned char *end;

   end = p;

   while (end - p < digits
          && (hex ? ((*end >= '0' && *end <= '9') || (*end >= 'a' && *end <= 'f')
                     || (*end >= 'A' && *end <= 'F'))
                  : (*end >= '0' && *end <= '9')))
      end++;

   return hex && end - p != digits ? p : end;
}

char *
elaro_ui_text_markup(const char *text)
{
   Eina_Strbuf *out, *run;
   unsigned int flags;
   const unsigned char *p, *end, *background;
   unsigned char c;

   out = eina_strbuf_new();
   run = eina_strbuf_new();
   flags = 0;

   for (p = (const unsigned char *)text; *p;)
     {
        c = *p++;

        if (c == 2 || c == 15 || c == 29 || c == 31)
          {
             _append_text(out, run);
             _styles(out, flags, EINA_FALSE);
             flags = c == 15 ? 0 : flags ^ (c == 2 ? 1u : c == 29 ? 2u : 4u);
             _styles(out, flags, EINA_TRUE);
          }
        else if (c == 3 || c == 4)
          {
             /* Consume unsupported colour parameters, keeping the message text. */
             end = _color(p, c == 3 ? 2 : 6, c == 4);

             if (end != p && *end == ',')
               {
                  background = _color(end + 1, c == 3 ? 2 : 6, c == 4);

                  if (background != end + 1) end = background;
               }
             p = end;
          }
        else if ((c >= 32 && c != 127) || c == '\n' || c == '\t')
          {
             eina_strbuf_append_char(run, c);
          }
     }
   _append_text(out, run);
   _styles(out, flags, EINA_FALSE);
   eina_strbuf_free(run);

   return eina_strbuf_release(out);
}
