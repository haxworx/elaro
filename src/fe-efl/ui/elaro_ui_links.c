#include "elaro_ui_private.h"
#include <Elementary_Cursor.h>

static void
_link_error_close_cb(void *data, Evas_Object *obj, void *info)
{
   evas_object_del(data);
}

typedef struct
{
   Elaro_Ui *ui;
   Evas_Object *popup, *always, *error;
   char *url;
} External_Link;

static void
_external_open(Elaro_Ui *ui, char *url)
{
   Evas_Object *popup;
   Ecore_Exe *process;
   char *quoted, *command, *launcher, *executable;

   /* Quote the entire argument, including shell metacharacters from chat. */
   launcher = elaro_executable_find("xdg-open");
   process = NULL;
   if (launcher)
     {
        executable = elaro_command_quote(launcher);
        quoted = elaro_command_quote(url);
        command = elaro_string_printf("%s %s", executable, quoted);
        process = ecore_exe_pipe_run(command, ECORE_EXE_USE_SH, NULL);
        free(command);
        free(quoted);
        free(executable);
        free(launcher);
     }

   if (!process)
     {
        popup = elm_popup_add(ui->win);
        elm_object_part_text_set(popup, "title,text", _("Could not open link"));
        elm_object_text_set(popup, _("Could not launch the default browser."));
        elaro_ui_popup_close_button_add(ui, popup, _link_error_close_cb, popup);
        evas_object_smart_callback_add(popup, "dismissed", _link_error_close_cb, popup);
        evas_object_show(popup);
     }
}

static void
_external_deleted_cb(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   External_Link *link;

   link = data;
   free(link->url);
   free(link);
}

static void
_external_accept_cb(void *data, Evas_Object *obj, void *info)
{
   External_Link *link;
   Eina_Error error;
   char *safe;

   link = data;

   if (elm_check_state_get(link->always))
     {
        error = 0;
        link->ui->config->always_allow_external_links = EINA_TRUE;

        if (!elaro_config_save(link->ui->config, &error))
          {
             link->ui->config->always_allow_external_links = EINA_FALSE;
             safe = elm_entry_utf8_to_markup(eina_error_msg_get(error));
             elm_object_text_set(link->error, safe);
             free(safe);
             evas_object_show(link->error);

             return;
          }
     }
   _external_open(link->ui, link->url);
   evas_object_del(link->popup);
}

static void
_external_confirm(Elaro_Ui *ui, const char *url)
{
   External_Link *link;
   Evas_Object *box, *description, *address, *open, *cancel;
   char *safe;

   if (ui->config->always_allow_external_links)
     {
        _external_open(ui, (char *)url);

        return;
     }
   link = calloc(1, sizeof(External_Link));
   if (!link) return;
   link->ui = ui;
   link->url = strdup(url);

   if (!link->url) goto allocation_failed;
   link->popup = elm_popup_add(ui->win);
   elm_object_part_text_set(link->popup, "title,text", _("Open link in browser?"));

   box = elm_box_add(link->popup);
   elaro_ui_box_padding_set(box, 0, 12);

   description = elm_label_add(box);
   elm_object_text_set(description, _("This link will open in your default browser."));
   elm_label_line_wrap_set(description, ELM_WRAP_WORD);
   elaro_ui_wrap_width_set(description, 360);
   evas_object_size_hint_weight_set(description, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(description, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(box, description);
   evas_object_show(description);

   address = elm_label_add(box);
   safe = elm_entry_utf8_to_markup(url);
   elm_object_text_set(address, safe);
   free(safe);
   /* Long paths and query strings must fit without hiding the destination. */
   elm_label_line_wrap_set(address, ELM_WRAP_CHAR);
   elaro_ui_wrap_width_set(address, 360);
   evas_object_size_hint_weight_set(address, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(address, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(box, address);
   evas_object_show(address);

   link->always = elm_check_add(box);
   elm_object_text_set(link->always, _("Always open links without asking"));
   evas_object_size_hint_weight_set(link->always, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(link->always, 0, 0.5);
   elm_box_pack_end(box, link->always);
   evas_object_show(link->always);

   link->error = elm_label_add(box);
   elm_label_line_wrap_set(link->error, ELM_WRAP_WORD);
   elaro_ui_wrap_width_set(link->error, 360);
   evas_object_size_hint_weight_set(link->error, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(link->error, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(box, link->error);
   elm_object_content_set(link->popup, box);
   evas_object_show(box);

   open = elm_button_add(link->popup);
   elm_object_text_set(open, _("Open in browser"));
   evas_object_smart_callback_add(open, "clicked", _external_accept_cb, link);
   elm_object_part_content_set(link->popup, "button1", open);

   cancel = elm_button_add(link->popup);
   elm_object_text_set(cancel, _("Cancel"));
   evas_object_smart_callback_add(cancel, "clicked", _link_error_close_cb, link->popup);
   elm_object_part_content_set(link->popup, "button2", cancel);
   elaro_ui_popup_close_button_add(ui, link->popup, _link_error_close_cb, link->popup);
   evas_object_smart_callback_add(link->popup, "dismissed", _link_error_close_cb, link->popup);
   evas_object_event_callback_add(link->popup, EVAS_CALLBACK_DEL, _external_deleted_cb, link);
   evas_object_show(link->popup);
   return;

allocation_failed:
   free(link);
}

void
elaro_ui_external_link_open(Elaro_Ui *ui, const char *url)
{
   _external_confirm(ui, url);
}

static void
_link_clicked_cb(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui;
   Elm_Entry_Anchor_Info *anchor;
   Eina_Bool image, audio;
   char *url;

   ui = data;
   anchor = info;

   if (!anchor || anchor->button != 1 || !anchor->name) return;

   image = eina_str_has_prefix(anchor->name, "image:");
   audio = eina_str_has_prefix(anchor->name, "audio:");

   if (!image && !audio && !eina_str_has_prefix(anchor->name, "url:")) return;

   url = elaro_uri_unescape(strchr(anchor->name, ':') + 1);

   if (!url || (strncasecmp(url, "http://", 7) && strncasecmp(url, "https://", 8)))
     {
        free(url);

        return;
     }

   if (image && elaro_ui_text_image_url_check(url)) elaro_ui_image_preview(ui, url);
   else if (audio && elaro_ui_text_audio_url_check(url)) elaro_ui_music_show(ui, url);
   else
     {
        _external_confirm(ui, url);
     }
   free(url);
}

static void
_link_in_cb(void *data, Evas_Object *obj, void *info)
{
   elm_object_cursor_set(obj, ELM_CURSOR_HAND2);
}

static void
_link_out_cb(void *data, Evas_Object *obj, void *info)
{
   elm_object_cursor_set(obj, ELM_CURSOR_XTERM);
}

void
elaro_ui_links_reset(Elaro_Ui *ui)
{
   elm_object_cursor_set(ui->history, ELM_CURSOR_XTERM);
}

void
elaro_ui_links_init(Elaro_Ui *ui)
{
   evas_object_smart_callback_add(ui->history, "anchor,clicked", _link_clicked_cb, ui);
   evas_object_smart_callback_add(ui->history, "anchor,in", _link_in_cb, ui);
   evas_object_smart_callback_add(ui->history, "anchor,out", _link_out_cb, ui);
}
