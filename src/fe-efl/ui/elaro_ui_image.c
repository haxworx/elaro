#include "elaro_ui_private.h"

static void
_preview_deleted_cb(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   ((Elaro_Ui *)data)->preview = NULL;
}

static void
_preview_close_cb(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui;

   ui = data;

   if (ui->preview) evas_object_del(ui->preview);
}

static void
_image_progress_show(Evas_Object *progress)
{
   Evas_Object *pulse, *box;

   pulse = evas_object_data_get(progress, "pulse");

   if (!pulse) return;

   box = elm_object_parent_widget_get(progress);
   elm_box_pack_before(box, progress, pulse);
   elm_box_unpack(box, pulse);
   evas_object_del(pulse);
   evas_object_data_del(progress, "pulse");
   evas_object_show(progress);
}

static void
_image_done_cb(void *data, Evas_Object *obj, void *info)
{
   _image_progress_show(data);
   elm_progressbar_value_set(data, 1.0);
   elm_object_text_set(data, _("Image ready"));
}

static void
_image_error_cb(void *data, Evas_Object *obj, void *info)
{
   _image_progress_show(data);
   elm_progressbar_value_set(data, 0.0);
   elm_progressbar_unit_format_set(data, "");
   elm_object_text_set(data, _("Image unavailable"));
   elm_object_tooltip_text_set(data, _("Could not load this image."));
}

static void
_image_progress_cb(void *data, Evas_Object *obj, void *info)
{
   Elm_Image_Progress *progress;

   progress = info;

   if (!progress || progress->total <= 0) return;

   _image_progress_show(data);
   elm_progressbar_value_set(data, ELARO_CLAMP(progress->now / progress->total, 0.0, 0.99));
}

void
elaro_ui_image_preview(Elaro_Ui *ui, const char *url)
{
   Evas_Object *box, *viewport, *size, *image, *status, *pulse;
   int width, height;

   if (ui->preview) evas_object_del(ui->preview);

   ui->preview = elm_popup_add(ui->win);
   elm_object_part_text_set(ui->preview, "title,text", _("Image preview"));
   evas_object_smart_callback_add(ui->preview, "dismissed", _preview_close_cb, ui);
   evas_object_event_callback_add(ui->preview, EVAS_CALLBACK_DEL, _preview_deleted_cb, ui);

   box = elaro_ui_media_box_add(ui->preview, url, _("Image"));

   viewport = elm_table_add(box);
   evas_object_size_hint_weight_set(viewport, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(viewport, EVAS_HINT_FILL, EVAS_HINT_FILL);

   evas_object_geometry_get(ui->win, NULL, NULL, &width, &height);

   size = evas_object_rectangle_add(evas_object_evas_get(viewport));
   evas_object_color_set(size, 0, 0, 0, 0);
   evas_object_pass_events_set(size, EINA_TRUE);
   elaro_ui_size_min_set(size,
                                 MIN(390, MAX(75, width - 102)),
                                 MIN(270, MAX(75, height - 225)));
   elm_table_pack(viewport, size, 0, 0, 1, 1);
   evas_object_show(size);

   image = elm_image_add(viewport);
   elm_image_resizable_set(image, EINA_FALSE, EINA_TRUE);
   elm_image_aspect_fixed_set(image, EINA_TRUE);
   evas_object_size_hint_weight_set(image, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(image, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(viewport, image, 0, 0, 1, 1);
   evas_object_show(image);
   elm_box_pack_end(box, viewport);
   evas_object_show(viewport);

   status = elm_progressbar_add(box);
   evas_object_size_hint_weight_set(status, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(status, EVAS_HINT_FILL, 0.5);
   /* Span sizes are logical pixels; Elementary applies the UI scale itself. */
   elm_progressbar_span_size_set(status, 135);
   elm_progressbar_unit_format_set(status, "%.0f%%");
   elm_object_text_set(status, _("Loading image…"));
   /* Keep pulse and fraction widgets separate so theme animation state cannot leak. */

   pulse = elm_progressbar_add(box);
   evas_object_size_hint_weight_set(pulse, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(pulse, EVAS_HINT_FILL, 0.5);
   elm_progressbar_span_size_set(pulse, elm_progressbar_span_size_get(status));
   elm_object_text_set(pulse, _("Loading image…"));
   elm_progressbar_pulse_set(pulse, EINA_TRUE);
   elm_progressbar_pulse(pulse, EINA_TRUE);
   evas_object_data_set(status, "pulse", pulse);
   elm_box_pack_end(box, pulse);
   evas_object_show(pulse);
   /* Elementary downloads HTTP(S) images asynchronously and cancels on deletion. */
   evas_object_smart_callback_add(image, "download,progress", _image_progress_cb, status);
   evas_object_smart_callback_add(image, "download,done", _image_done_cb, status);
   evas_object_smart_callback_add(image, "download,error", _image_error_cb, status);
   evas_object_smart_callback_add(image, "load,error", _image_error_cb, status);
   evas_object_smart_callback_add(image, "load,ready", _image_done_cb, status);
   elaro_ui_popup_close_button_add(ui, ui->preview, _preview_close_cb, ui);

   if (!elm_image_file_set(image, url, NULL)) _image_error_cb(status, image, NULL);
   evas_object_show(ui->preview);
}
