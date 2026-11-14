#include <src/common.h>
#define MODULE_NAME "fe-efl"
#include <src/core/servers.h>
#include <src/core/signals.h>
#include <src/irc/core/irc.h>
#include <src/irc/core/irc-servers.h>
#include "elaro_ctcp.h"
#include "elaro_engine_private.h"
#include <errno.h>
#include <math.h>

static gboolean
_ping_elapsed(const char *data, gint64 now, double *elapsed)
{
   char *end;
   gint64 seconds, micros = 0, sent;

   if (!g_ascii_isdigit(*data) || now < 0) return FALSE;
   errno = 0;
   seconds = g_ascii_strtoll(data, &end, 10);
   if (errno || seconds < 0 || seconds > now / G_TIME_SPAN_SECOND) return FALSE;
   if (*end == '.')
     {
        double timestamp = g_ascii_strtod(data, &end);
        while (g_ascii_isspace(*end))
           end++;
        if (*end || !isfinite(timestamp) || timestamp > (double)now / G_TIME_SPAN_SECOND)
           return FALSE;
        *elapsed = (double)now / G_TIME_SPAN_SECOND - timestamp;
        return TRUE;
     }
   if (*end && !g_ascii_isspace(*end)) return FALSE;
   while (g_ascii_isspace(*end))
      end++;
   if (*end)
     {
        if (!g_ascii_isdigit(*end)) return FALSE;
        errno = 0;
        micros = g_ascii_strtoll(end, &end, 10);
        if (errno || micros < 0 || micros >= G_TIME_SPAN_SECOND) return FALSE;
        while (g_ascii_isspace(*end))
           end++;
        if (*end) return FALSE;
     }
   if (seconds == now / G_TIME_SPAN_SECOND && micros > now % G_TIME_SPAN_SECOND) return FALSE;
   sent = seconds * G_TIME_SPAN_SECOND + micros;
   if (sent > now) return FALSE;
   *elapsed = (double)(now - sent) / G_TIME_SPAN_SECOND;
   return TRUE;
}

char *
elaro_ctcp_reply_format(const char *nick, const char *reply, gint64 now)
{
   char *copy, *data, *command, *out, *p, *write;
   double elapsed;
   char seconds[G_ASCII_DTOSTR_BUF_SIZE];

   copy = g_strdup(reply);
   /* Keep one CTCP frame, removing controls from the visible payload. */
   p = strchr(copy, '\001');
   if (p) *p = 0;
   for (p = write = copy; *p; p++)
      if ((unsigned char)*p >= 32 && *p != 127) *write++ = *p;
   *write = 0;
   data = strchr(copy, ' ');
   if (data) *data++ = 0;
   else data = copy + strlen(copy);
   while (*data == ' ')
      data++;
   command = g_ascii_strup(copy, -1);
   if (!strcmp(command, "PING"))
     {
        if (_ping_elapsed(data, now, &elapsed))
          {
             g_ascii_formatd(seconds, sizeof(seconds), "%.3f", elapsed);
             out = g_strdup_printf("CTCP PING reply from %s: %s seconds", nick, seconds);
          }
        else out = g_strdup_printf("CTCP PING reply from %s: invalid timestamp", nick);
     }
   else
      out = g_strdup_printf("CTCP %s reply from %s:%s%s", *command ? command : "unknown", nick,
                            *data ? " " : "", data);
   g_free(command);
   g_free(copy);
   return out;
}

static void
_reply(IRC_SERVER_REC *server, const char *data, const char *nick, const char *address,
       const char *target)
{
   Elaro_Engine_Event *event = g_new0(Elaro_Engine_Event, 1);
   char *text = elaro_ctcp_reply_format(nick ? nick : server->tag, data, g_get_real_time());

   event->type = ELARO_ENGINE_LINE;
   event->server = g_strdup(server->tag);
   event->target = g_strdup(server_ischannel(SERVER(server), target) ? target : nick ? nick : "");
   event->text = g_utf8_make_valid(text, -1);
   g_free(text);
   elaro_engine_event_push(event);
}

static void
_ping_reply(IRC_SERVER_REC *server, const char *data, const char *nick, const char *address,
            const char *target)
{
   char *reply = g_strconcat("PING ", data, NULL);
   _reply(server, reply, nick, address, target);
   g_free(reply);
}

void
elaro_ctcp_init(void)
{
   signal_add("ctcp reply ping", (SIGNAL_FUNC)_ping_reply);
   signal_add("default ctcp reply", (SIGNAL_FUNC)_reply);
}

void
elaro_ctcp_deinit(void)
{
   signal_remove("ctcp reply ping", (SIGNAL_FUNC)_ping_reply);
   signal_remove("default ctcp reply", (SIGNAL_FUNC)_reply);
}
