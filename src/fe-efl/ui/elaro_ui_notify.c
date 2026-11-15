#include "elaro_ui_private.h"

void
elaro_ui_notify_sound(Elaro_Ui *ui)
{
   Evas_Object *sound = evas_object_data_get(ui->win, "private-message-sound");
   char *path;

   if (!sound)
     {
        /* The window owns this hidden layout for the lifetime of playback. */
        sound = elm_layout_add(ui->win);
        path = elaro_path_join(ELARO_DATA_DIR, "elaro.edj", NULL);
        if (!elm_layout_file_set(sound, path, "elaro/sounds"))
          {
             free(path);
             path = elaro_path_join(ELARO_ASSET_BUILD_DIR, "elaro.edj", NULL);
             if (!elm_layout_file_set(sound, path, "elaro/sounds"))
               {
                  free(path);
                  evas_object_del(sound);
                  return;
               }
          }
        free(path);
        evas_object_data_set(ui->win, "private-message-sound", sound);
     }
   elm_layout_signal_emit(sound, "private,message", "elaro");
}

void
elaro_ui_notify_private(const char *server, const char *nick, const char *message)
{
   char *executable, *icon = NULL, *title = NULL, *markup = NULL, *plain = NULL;
   char *quoted_executable = NULL, *quoted_icon = NULL, *quoted_title = NULL;
   char *quoted_body = NULL, *command = NULL;
   Eina_Strbuf *body = NULL;

   executable = elaro_executable_find("notify-send");
   if (!executable) return;
   icon = elaro_path_join(ELARO_DATA_DIR, "..", "icons", "hicolor", "128x128", "apps", "elaro.png",
                          NULL);
   if (!icon) goto done;
   if (!ecore_file_exists(icon))
     {
        free(icon);
        icon = elaro_path_join(ELARO_SOURCE_DIR, "data", "elaro.png", NULL);
     }
   if (!icon) goto done;
   title = elaro_string_printf(_("Private message from %s (%s)"), nick, server);
   /* Strip IRC formatting, then escape notification body markup. */
   markup = elaro_ui_text_markup(message);
   if (!title || !markup) goto done;
   plain = elm_entry_markup_to_utf8(markup);
   if (!plain) goto done;
   body = eina_strbuf_new();
   if (!body) goto done;
   eina_strbuf_append(body, plain ? plain : "");
   eina_strbuf_replace_all(body, "&", "&amp;");
   eina_strbuf_replace_all(body, "<", "&lt;");
   eina_strbuf_replace_all(body, ">", "&gt;");
   quoted_executable = elaro_command_quote(executable);
   quoted_icon = elaro_command_quote(ecore_file_exists(icon) ? icon : "elaro");
   quoted_title = elaro_command_quote(title);
   quoted_body = elaro_command_quote(eina_strbuf_string_get(body));
   if (!quoted_executable || !quoted_icon || !quoted_title || !quoted_body) goto done;
   command = elaro_string_printf("%s -a Elaro -t 10000 -i %s -u normal -- %s %s", quoted_executable,
                                 quoted_icon, quoted_title, quoted_body);
   if (!command) goto done;
   ecore_exe_pipe_run(command, ECORE_EXE_USE_SH, NULL);
done:
   free(command);
   free(quoted_body);
   free(quoted_title);
   free(quoted_icon);
   free(quoted_executable);
   if (body) eina_strbuf_free(body);
   free(plain);
   free(markup);
   free(title);
   free(icon);
   free(executable);
}
