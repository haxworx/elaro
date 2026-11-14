#include "elaro_ui_private.h"

/* Fontconfig names contain aliases and properties. Show the first family and
 * style, retaining the original identifier as item data. */
static const char *
_font_part_append(Eina_Strbuf *out, const char *text)
{
   while (*text && *text != ',' && *text != ':')
     {
        if (*text == '\\' && text[1]) text++;
        eina_strbuf_append_char(out, *text++);
     }
   return text;
}

static char *
_font_label_get(const char *name)
{
   Eina_Strbuf *label = eina_strbuf_new();
   const char *p;

   p = _font_part_append(label, name);
   while (*p)
     {
        if (!strncmp(p, ":style=", 7))
          {
             p += 7;
             if (*p && *p != ',' && *p != ':')
               {
                  eina_strbuf_append(label, " — ");
                  _font_part_append(label, p);
               }
             break;
          }
        if (*p == '\\' && p[1]) p++;
        p++;
     }
   return eina_strbuf_release(label);
}

static char *
_font_text_get(void *data, Evas_Object *obj, const char *part)
{
   char *label = _font_label_get(data);
   char *safe = elm_entry_utf8_to_markup(label);
   free(label);
   return safe;
}

static void
_font_del(void *data, Evas_Object *obj)
{
   free(data);
}

static int
_font_compare(const void *a, const void *b)
{
   return strcasecmp(a, b);
}

static void
_font_preview_update(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui = data;
   Elm_Object_Item *item = elm_genlist_selected_item_get(ui->connection.font_list);
   char *safe;

   if (!item) return;
   safe = _font_text_get(elm_object_item_data_get(item), NULL, NULL);
   elm_object_text_set(ui->connection.font_combo, safe);
   free(safe);
   elaro_ui_font_set(ui->connection.font_preview, elm_object_item_data_get(item),
                     (int)(elm_slider_value_get(ui->connection.font_size) + 0.5));
}

static void
_font_combo_pressed(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui = data;
   const char *name = elm_object_item_data_get(info);
   Elm_Object_Item *item;

   if (!name) return;
   for (item = elm_genlist_first_item_get(ui->connection.font_list); item;
        item = elm_genlist_item_next_get(item))
     {
        if (strcmp(name, elm_object_item_data_get(item))) continue;
        elm_genlist_item_selected_set(item, EINA_TRUE);
        elm_genlist_item_bring_in(item, ELM_GENLIST_ITEM_SCROLLTO_IN);
        _font_preview_update(ui, NULL, NULL);
        break;
     }
   elm_combobox_hover_end(obj);
}

void
elaro_ui_font_options_add(Elaro_Ui *ui, Evas_Object *box)
{
   Eina_List *fonts, *node;
   const char *name, *previous = NULL;
   Elm_Genlist_Item_Class *klass;
   Elm_Object_Item *selected;

   ui->connection.font_combo = elm_combobox_add(box);
   elm_object_part_text_set(ui->connection.font_combo, "guide", _("Choose a chat font"));
   elm_entry_editable_set(ui->connection.font_combo, EINA_FALSE);
   evas_object_size_hint_weight_set(ui->connection.font_combo, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(ui->connection.font_combo, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(box, ui->connection.font_combo);
   evas_object_show(ui->connection.font_combo);
   evas_object_smart_callback_add(ui->connection.font_combo, "item,pressed", _font_combo_pressed,
                                  ui);

   ui->connection.font_size = elm_slider_add(box);
   elm_object_text_set(ui->connection.font_size, _("Text size"));
   elm_slider_min_max_set(ui->connection.font_size, 8, 36);
   elm_slider_step_set(ui->connection.font_size, 1.0 / 28);
   elm_slider_unit_format_set(ui->connection.font_size, "%1.0f");
   elm_slider_value_set(ui->connection.font_size, ui->config->chat_font_size);
   evas_object_size_hint_align_set(ui->connection.font_size, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(box, ui->connection.font_size);
   evas_object_show(ui->connection.font_size);

   ui->connection.font_preview = elm_entry_add(box);
   elm_entry_editable_set(ui->connection.font_preview, EINA_FALSE);
   elm_entry_entry_set(ui->connection.font_preview,
                       _("Alice: The quick brown fox jumps over the lazy dog. 0123456789"));
   elm_entry_line_wrap_set(ui->connection.font_preview, ELM_WRAP_WORD);
   evas_object_size_hint_align_set(ui->connection.font_preview, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(box, ui->connection.font_preview);
   evas_object_show(ui->connection.font_preview);

   ui->connection.font_list = elm_genlist_add(box);
   elm_genlist_mode_set(ui->connection.font_list, ELM_LIST_COMPRESS);
   elm_genlist_homogeneous_set(ui->connection.font_list, EINA_TRUE);
   evas_object_size_hint_align_set(ui->connection.font_list, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_min_set(ui->connection.font_list, 0, ELM_SCALE_SIZE(150));
   elm_box_pack_end(box, ui->connection.font_list);
   evas_object_show(ui->connection.font_list);
   klass = elm_genlist_item_class_new();
   klass->item_style = "default";
   klass->func.text_get = _font_text_get;
   klass->func.del = _font_del;
   /* Keep a saved font selectable even if it is unavailable on this machine. */
   selected
       = elm_genlist_item_append(ui->connection.font_list, klass, strdup(ui->config->chat_font),
                                 NULL, ELM_GENLIST_ITEM_NONE, _font_preview_update, ui);
   elm_genlist_item_append(ui->connection.font_combo, klass, strdup(ui->config->chat_font), NULL,
                           ELM_GENLIST_ITEM_NONE, NULL, NULL);
   fonts = evas_font_available_list(evas_object_evas_get(box));
   fonts = eina_list_sort(fonts, 0, _font_compare);
   EINA_LIST_FOREACH (fonts, node, name)
     {
        if ((previous && !strcmp(previous, name)) || !strcmp(name, ui->config->chat_font))
           continue;
        previous = name;
        elm_genlist_item_append(ui->connection.font_combo, klass, strdup(name), NULL,
                                ELM_GENLIST_ITEM_NONE, NULL, NULL);
        elm_genlist_item_append(ui->connection.font_list, klass, strdup(name), NULL,
                                ELM_GENLIST_ITEM_NONE, _font_preview_update, ui);
     }
   evas_font_available_list_free(evas_object_evas_get(box), fonts);
   elm_genlist_item_class_free(klass);
   elm_genlist_item_selected_set(selected, EINA_TRUE);
   _font_preview_update(ui, NULL, NULL);
   evas_object_smart_callback_add(ui->connection.font_size, "changed", _font_preview_update, ui);
}
