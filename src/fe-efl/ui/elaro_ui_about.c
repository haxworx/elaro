#include "elaro_ui_private.h"
#include <Elementary_Cursor.h>

static void
_irssi_clicked_cb(void *data, Evas_Object *obj, void *info)
{
   elaro_ui_external_link_open(data, "https://irssi.org/");
}

static void
_about_deleted_cb(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   ((Elaro_Ui *)data)->about = NULL;
}

static void
_about_append(Eina_Strbuf *text, const char *name)
{
   char *path, *contents;

   path = elaro_path_join(ELARO_DATA_DIR, name, NULL);
   contents = elaro_file_read(path);

   if (!contents)
     {
        free(path);
        path = elaro_path_join(!strcmp(name, "ABOUT") ? ELARO_BUILD_DIR : ELARO_SOURCE_DIR, name,
                               NULL);
        contents = elaro_file_read(path);
     }

   if (contents)
     {
        eina_strbuf_append(text, contents);
        eina_strbuf_append(text, "\n\n");
     }
   else eina_strbuf_append_printf(text, _("Could not load %s.\n\n"), name);
   free(contents);
   free(path);
}

static void
_about_close_cb(void *data, Evas_Object *obj, void *info)
{
   /* Unregister descendants while their focus parents are still alive. */
   elm_object_tree_focus_allow_set(data, EINA_FALSE);
   evas_object_hide(data);
   evas_object_del(data);
}

void
elaro_ui_about_show(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui;
   Evas_Object *box, *header, *details, *icon, *name, *copyright, *powered, *link_label, *frame;
   Evas_Object *viewport, *size, *view, *close;
   char *path, *safe;
   Eina_Strbuf *text;

   ui = data;

   if (ui->about)
     {
        evas_object_raise(ui->about);
        elm_object_focus_set(ui->about, EINA_TRUE);

        return;
     }

   ui->about = elm_popup_add(ui->win);
   elm_object_part_text_set(ui->about, "title,text", _("About Elaro"));
   evas_object_size_hint_weight_set(ui->about, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_smart_callback_add(ui->about, "dismissed", _about_close_cb, ui->about);
   evas_object_event_callback_add(ui->about, EVAS_CALLBACK_DEL, _about_deleted_cb, ui);

   box = elm_box_add(ui->about);
   elm_box_padding_set(box, 0, ELM_SCALE_SIZE(16));
   evas_object_size_hint_padding_set(box, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16),
                                     ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_content_set(ui->about, box);
   evas_object_show(box);

   header = elm_box_add(box);
   elm_box_horizontal_set(header, EINA_TRUE);
   elm_box_padding_set(header, ELM_SCALE_SIZE(20), 0);
   evas_object_size_hint_align_set(header, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(box, header);
   evas_object_show(header);

   icon = elm_icon_add(header);
   path = elaro_path_join(ELARO_DATA_DIR, "..", "icons", "hicolor", "128x128", "apps", "elaro.png",
                          NULL);

   if (!elm_image_file_set(icon, path, NULL))
     {
        free(path);
        path = elaro_path_join(ELARO_SOURCE_DIR, "data", "elaro.png", NULL);
        elm_image_file_set(icon, path, NULL);
     }
   free(path);
   evas_object_size_hint_min_set(icon, ELM_SCALE_SIZE(96), ELM_SCALE_SIZE(96));
   evas_object_size_hint_max_set(icon, ELM_SCALE_SIZE(96), ELM_SCALE_SIZE(96));
   elm_box_pack_end(header, icon);
   evas_object_show(icon);

   details = elm_box_add(header);
   elm_box_padding_set(details, 0, ELM_SCALE_SIZE(8));
   evas_object_size_hint_weight_set(details, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(details, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(header, details);
   evas_object_show(details);

   name = elm_label_add(details);
   elm_object_text_set(name, "<font_size=24><b>Elaro</b></font_size>  " PACKAGE_VERSION);
   evas_object_size_hint_align_set(name, 0, 0.5);
   elm_box_pack_end(details, name);
   evas_object_show(name);

   powered = elm_button_add(details);
   elm_object_style_set(powered, "blank");
   link_label = elm_label_add(powered);
   elm_object_text_set(link_label,
                       _("<color=#80b8e8><u>Powered by Irssi ↗</u></color>"));
   evas_object_pass_events_set(link_label, EINA_TRUE);
   elm_object_content_set(powered, link_label);
   evas_object_show(link_label);
   elm_object_cursor_set(powered, ELM_CURSOR_HAND2);
   evas_object_size_hint_align_set(powered, 0, 0.5);
   elm_object_tooltip_text_set(powered, "https://irssi.org/");
   evas_object_smart_callback_add(powered, "clicked", _irssi_clicked_cb, ui);
   elm_box_pack_end(details, powered);
   evas_object_show(powered);

   copyright = elm_label_add(details);
   elm_object_text_set(copyright,
                       "© 2026 <b>Alastair Poole</b><br/>netstar@netstar.im");
   evas_object_size_hint_align_set(copyright, 0, 0.5);
   elm_box_pack_end(details, copyright);
   evas_object_show(copyright);

   frame = elm_frame_add(box);
   elm_object_text_set(frame, _("Credits and license"));
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, frame);
   evas_object_show(frame);

   viewport = elm_table_add(frame);
   evas_object_size_hint_weight_set(viewport, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(viewport, EVAS_HINT_FILL, EVAS_HINT_FILL);
   /* Reserve space independently of the scrollable entry's content. */

   size = evas_object_rectangle_add(evas_object_evas_get(viewport));
   evas_object_color_set(size, 0, 0, 0, 0);
   evas_object_size_hint_weight_set(size, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(size, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_pass_events_set(size, EINA_TRUE);
   evas_object_size_hint_min_set(size, ELM_SCALE_SIZE(440), ELM_SCALE_SIZE(240));
   elm_table_pack(viewport, size, 0, 0, 1, 1);
   evas_object_show(size);

   view = elm_entry_add(viewport);
   elm_entry_scrollable_set(view, EINA_TRUE);
   elm_scroller_policy_set(view, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
   elm_entry_editable_set(view, EINA_FALSE);
   elm_entry_line_wrap_set(view, ELM_WRAP_WORD);
   evas_object_size_hint_weight_set(view, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(view, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_min_set(view, ELM_SCALE_SIZE(440), ELM_SCALE_SIZE(240));
   evas_object_size_hint_max_set(view, -1, ELM_SCALE_SIZE(240));
   text = eina_strbuf_new();
   _about_append(text, "ABOUT");
   eina_strbuf_append(text, _("AUTHORS\n\n"));
   _about_append(text, "AUTHORS");
   eina_strbuf_append(text, _("COPYRIGHT AND LICENSE\n\n"));
   _about_append(text, "COPYING");
   safe = elm_entry_utf8_to_markup(eina_strbuf_string_get(text));
   elm_entry_entry_set(view, safe);
   free(safe);
   eina_strbuf_free(text);
   elm_table_pack(viewport, view, 0, 0, 1, 1);
   evas_object_show(view);
   elm_object_content_set(frame, viewport);
   evas_object_show(viewport);

   close = elm_button_add(box);
   evas_object_size_hint_weight_set(close, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(close, 1.0, 0.5);
   elm_object_text_set(close, _("Close"));
   evas_object_smart_callback_add(close, "clicked", _about_close_cb, ui->about);
   elm_box_pack_end(box, close);
   evas_object_show(close);
   evas_object_show(ui->about);
   elm_object_focus_set(close, EINA_TRUE);
}
