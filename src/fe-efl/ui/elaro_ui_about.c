#include "elaro_ui_private.h"

static void
_irssi_clicked_cb(void *data, Evas_Object *obj, void *info)
{
   elaro_ui_external_link_open(data, "https://irssi.org/");
}

static void
_about_deleted_cb(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui = data;
   if (ui->about == obj) ui->about = NULL;
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
        path = elaro_path_join(!strcmp(name, "ABOUT") ? ELARO_BUILD_DIR : ELARO_SOURCE_DIR,
                               name, NULL);
        contents = elaro_file_read(path);
     }
   if (contents) eina_strbuf_append(text, contents);
   else eina_strbuf_append_printf(text, _("Could not load %s."), name);
   free(contents);
   free(path);
}

static void
_about_page_set(Evas_Object *view, int page)
{
   Eina_Strbuf *text = eina_strbuf_new();
   char *safe;
   if (!text) return;
   if (page == 1)
     {
        eina_strbuf_append(text, _("Made by Alastair Poole\nnetstar@netstar.im\n\n"));
        eina_strbuf_append(text, _("With thanks to the Irssi authors and contributors, and the "
                                  "EFL and Enlightenment community.\n\n"));
        _about_append(text, "AUTHORS");
     }
   else if (page == 2) _about_append(text, "COPYING");
   else
     {
        eina_strbuf_append(text, _("A comfortable place for your channels, people and conversations.\n\n"));
        _about_append(text, "ABOUT");
     }
   safe = elm_entry_utf8_to_markup(eina_strbuf_string_get(text));
   elm_entry_entry_set(view, safe);
   elm_entry_cursor_begin_set(view);
   free(safe);
   eina_strbuf_free(text);
}

static void
_about_page_changed(void *data, Evas_Object *obj, void *info)
{
   _about_page_set(data, elm_segment_control_item_index_get(info));
   elaro_ui_view_reveal(data);
}

static void
_about_close_cb(void *data, Evas_Object *obj, void *info)
{
   Evas_Object *popup = data;
   Elaro_Ui *ui = evas_object_data_get(popup, "app");
   if (ui && ui->about == popup) ui->about = NULL;
   elaro_ui_popup_close(popup);
}

void
elaro_ui_about_show(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui = data;
   Evas_Object *box, *hero, *release, *tabs, *viewport, *reserve, *view, *footer, *powered, *close;
   Elm_Object_Item *overview;
   char *version;

   if (ui->about)
     {
        evas_object_raise(ui->about);
        elm_object_focus_set(ui->about, EINA_TRUE);
        return;
     }
   ui->about = elm_popup_add(ui->win);
   elm_object_part_text_set(ui->about, "title,text", _("About Elaro"));
   evas_object_data_set(ui->about, "app", ui);
   evas_object_smart_callback_add(ui->about, "block,clicked", _about_close_cb, ui->about);
   evas_object_event_callback_add(ui->about, EVAS_CALLBACK_DEL, _about_deleted_cb, ui);

   box = elm_box_add(ui->about);
   elaro_ui_box_padding_set(box, 0, 14);
   elaro_ui_padding_set(box, 12, 12,
                                     12, 12);
   elm_object_content_set(ui->about, box);
   evas_object_show(box);

   hero = elm_layout_add(box);
   if (ui->theme_path) elm_layout_file_set(hero, ui->theme_path, "elaro/about/hero");
   elaro_ui_size_min_set(hero, 420, 174);
   evas_object_size_hint_weight_set(hero, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(hero, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(box, hero);
   evas_object_show(hero);
   evas_object_data_set(ui->about, "hero", hero);

   release = elm_label_add(box);
   version = elaro_string_printf(_("<b>Version %s</b>  ·  © 2026 Alastair Poole"), PACKAGE_VERSION);
   elm_object_text_set(release, version);
   free(version);
   elm_box_pack_end(box, release);
   evas_object_show(release);

   tabs = elaro_ui_tabs_add(box);
   overview = elm_segment_control_item_add(tabs, NULL, _("Overview"));
   elm_segment_control_item_add(tabs, NULL, _("Credits"));
   elm_segment_control_item_add(tabs, NULL, _("License"));
   evas_object_size_hint_weight_set(tabs, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(tabs, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(box, tabs);
   evas_object_show(tabs);

   /* Entry recalculation replaces its own size hints; reserve a real viewport
    * independently so each tab remains readable after the text changes. */
   viewport = elm_table_add(box);
   evas_object_size_hint_weight_set(viewport, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(viewport, EVAS_HINT_FILL, 0.5);
   elaro_ui_size_max_set(viewport, -1, 160);
   reserve = evas_object_rectangle_add(evas_object_evas_get(viewport));
   evas_object_color_set(reserve, 0, 0, 0, 0);
   evas_object_pass_events_set(reserve, EINA_TRUE);
   elaro_ui_size_min_set(reserve, 420, 160);
   elm_table_pack(viewport, reserve, 0, 0, 1, 1);
   evas_object_show(reserve);
   elm_box_pack_end(box, viewport);
   evas_object_show(viewport);
   view = elm_entry_add(viewport);
   elm_entry_scrollable_set(view, EINA_TRUE);
   elm_entry_editable_set(view, EINA_FALSE);
   elm_entry_line_wrap_set(view, ELM_WRAP_WORD);
   elm_scroller_policy_set(view, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
   evas_object_size_hint_weight_set(view, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(view, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(viewport, view, 0, 0, 1, 1);
   evas_object_show(view);
   evas_object_smart_callback_add(tabs, "changed", _about_page_changed, view);
   elm_segment_control_item_selected_set(overview, EINA_TRUE);
   _about_page_set(view, 0);
   evas_object_data_set(ui->about, "pages", tabs);
   evas_object_data_set(ui->about, "text", view);

   footer = elm_box_add(box);
   elm_box_horizontal_set(footer, EINA_TRUE);
   elaro_ui_box_padding_set(footer, 12, 0);
   evas_object_size_hint_weight_set(footer, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(footer, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(box, footer);
   evas_object_show(footer);
   powered = elaro_ui_button_add(footer, _("Powered by Irssi ↗"), _irssi_clicked_cb, ui);
   elm_box_pack_end(footer, powered);
   close = elaro_ui_popup_close_button_add(ui, ui->about, _about_close_cb, ui->about);
   elaro_ui_popup_show(ui->about);
   elm_object_focus_set(close, EINA_TRUE);
}
