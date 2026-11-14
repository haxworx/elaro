#include "elaro_ui_private.h"

void
elaro_ui_notify_private(const char *server, const char *nick, const char *message)
{
   char *executable, *icon, *title, *markup, *plain;
   char *quoted_executable, *quoted_icon, *quoted_title, *quoted_body, *command;
   Eina_Strbuf *body;

   executable = elaro_executable_find("notify-send");
   if (!executable) return;
   icon = elaro_path_join(ELARO_DATA_DIR, "..", "icons", "hicolor", "128x128", "apps", "elaro.png",
                          NULL);
   if (!ecore_file_exists(icon))
     {
        free(icon);
        icon = elaro_path_join(ELARO_SOURCE_DIR, "data", "elaro.png", NULL);
     }
   title = elaro_string_printf(_("Private message from %s (%s)"), nick, server);
   /* Strip IRC formatting, then escape notification body markup. */
   markup = elaro_ui_text_markup(message);
   plain = elm_entry_markup_to_utf8(markup);
   body = eina_strbuf_new();
   eina_strbuf_append(body, plain ? plain : "");
   eina_strbuf_replace_all(body, "&", "&amp;");
   eina_strbuf_replace_all(body, "<", "&lt;");
   eina_strbuf_replace_all(body, ">", "&gt;");
   quoted_executable = elaro_command_quote(executable);
   quoted_icon = elaro_command_quote(ecore_file_exists(icon) ? icon : "elaro");
   quoted_title = elaro_command_quote(title);
   quoted_body = elaro_command_quote(eina_strbuf_string_get(body));
   command = elaro_string_printf("%s -a Elaro -t 10000 -i %s -u normal -- %s %s", quoted_executable,
                                 quoted_icon, quoted_title, quoted_body);
   ecore_exe_pipe_run(command, ECORE_EXE_USE_SH, NULL);
   free(command);
   free(quoted_body);
   free(quoted_title);
   free(quoted_icon);
   free(quoted_executable);
   eina_strbuf_free(body);
   free(plain);
   free(markup);
   free(title);
   free(icon);
   free(executable);
}
