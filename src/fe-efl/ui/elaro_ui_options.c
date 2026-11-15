#include "elaro_ui_private.h"

static void
_transparency_changed_cb(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui;

   ui = data;
   elm_object_disabled_set(ui->connection.opacity, !elm_check_state_get(obj));
}

static Evas_Object *
_options_group_add(Evas_Object *parent, const char *title)
{
   Evas_Object *frame, *box;

   frame = elm_frame_add(parent);
   elm_object_text_set(frame, title);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, 0);
   box = elm_box_add(frame);
   elm_box_align_set(box, 0, 0);
   elaro_ui_box_padding_set(box, 0, 6);
   elm_object_content_set(frame, box);
   elm_box_pack_end(parent, frame);
   evas_object_show(box);
   evas_object_show(frame);
   return box;
}

void
elaro_ui_options_save(Elaro_Ui *ui)
{
   Elaro_Config candidate;
   Eina_Error error;
   char *safe;

   candidate = *ui->config;
   candidate.notify_private_messages = elm_check_state_get(ui->connection.notify_private);
   candidate.enable_sounds = elm_check_state_get(ui->connection.enable_sounds);
   candidate.window_transparent = elm_check_state_get(ui->connection.transparent);
   candidate.window_opacity = (int)(elm_slider_value_get(ui->connection.opacity) + 0.5);
   candidate.always_allow_external_links = elm_check_state_get(ui->connection.allow_links);
   error = 0;

   if (!elaro_config_save(&candidate, &error))
     {
        safe = elm_entry_utf8_to_markup(eina_error_msg_get(error));
        elm_object_text_set(ui->connection.options_error, safe);
        free(safe);

        return;
     }
   ui->config->notify_private_messages = candidate.notify_private_messages;
   ui->config->enable_sounds = candidate.enable_sounds;
   ui->config->always_allow_external_links = candidate.always_allow_external_links;
   ui->config->window_transparent = candidate.window_transparent;
   ui->config->window_opacity = candidate.window_opacity;
   elaro_ui_opacity_apply(ui);
   elm_object_text_set(ui->connection.options_error, _("Options saved."));
}

void
elaro_ui_options_add(Elaro_Ui *ui, Evas_Object *parent)
{
   Evas_Object *options_box, *options;

   ui->connection.options_view = elm_scroller_add(parent);
   elm_scroller_policy_set(ui->connection.options_view, ELM_SCROLLER_POLICY_OFF,
                           ELM_SCROLLER_POLICY_AUTO);
   evas_object_size_hint_align_set(ui->connection.options_view, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(ui->connection.options_view, EVAS_HINT_EXPAND,
                                    EVAS_HINT_EXPAND);
   options_box = elm_box_add(ui->connection.options_view);
   elm_box_align_set(options_box, 0, 0);
   evas_object_size_hint_weight_set(options_box, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(options_box, EVAS_HINT_FILL, 0);
   elm_object_content_set(ui->connection.options_view, options_box);
   evas_object_show(options_box);

   options = _options_group_add(options_box, _("Links"));
   ui->connection.allow_links = elm_check_add(options);
   elm_object_text_set(ui->connection.allow_links, _("Always allow external links"));
   elm_object_tooltip_text_set(ui->connection.allow_links,
                               _("Open web links in the default browser without asking first."));
   elm_check_state_set(ui->connection.allow_links, ui->config->always_allow_external_links);
   evas_object_size_hint_weight_set(ui->connection.allow_links, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(ui->connection.allow_links, 0, 0.5);
   elm_box_pack_end(options, ui->connection.allow_links);
   evas_object_show(ui->connection.allow_links);

   options = _options_group_add(options_box, _("Notifications"));
   ui->connection.notify_private = elm_check_add(options);
   elm_object_text_set(ui->connection.notify_private, _("Notify about private messages"));
   elm_object_tooltip_text_set(
       ui->connection.notify_private,
       _("Show desktop notifications only for private conversations that are not selected. Requires notify-send."));
   elm_check_state_set(ui->connection.notify_private, ui->config->notify_private_messages);
   evas_object_size_hint_weight_set(ui->connection.notify_private, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(ui->connection.notify_private, 0, 0.5);
   elm_box_pack_end(options, ui->connection.notify_private);
   evas_object_show(ui->connection.notify_private);

   ui->connection.enable_sounds = elm_check_add(options);
   elm_object_text_set(ui->connection.enable_sounds, _("Enable sounds"));
   elm_object_tooltip_text_set(ui->connection.enable_sounds,
       _("Enable sounds"));
   elm_check_state_set(ui->connection.enable_sounds, ui->config->enable_sounds);
   evas_object_size_hint_weight_set(ui->connection.enable_sounds, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(ui->connection.enable_sounds, 0, 0.5);
   elm_box_pack_end(options, ui->connection.enable_sounds);
   evas_object_show(ui->connection.enable_sounds);

   options = _options_group_add(options_box, _("Appearance"));
   ui->connection.transparent = elm_check_add(options);
   elm_object_text_set(ui->connection.transparent, _("Enable window transparency"));
   elm_check_state_set(ui->connection.transparent, ui->config->window_transparent);
   evas_object_size_hint_weight_set(ui->connection.transparent, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(ui->connection.transparent, 0, 0.5);
   elm_box_pack_end(options, ui->connection.transparent);
   evas_object_show(ui->connection.transparent);
   evas_object_smart_callback_add(ui->connection.transparent, "changed", _transparency_changed_cb,
                                  ui);

   ui->connection.opacity = elm_slider_add(options);
   elm_object_text_set(ui->connection.opacity, _("Window opacity"));
   elm_slider_min_max_set(ui->connection.opacity, ELARO_OPACITY_MIN, ELARO_OPACITY_MAX);
   elm_slider_step_set(ui->connection.opacity, 1.0 / (ELARO_OPACITY_MAX - ELARO_OPACITY_MIN));
   elm_slider_unit_format_set(ui->connection.opacity, "%1.0f %%");
   elm_slider_indicator_visible_mode_set(ui->connection.opacity,
                                         ELM_SLIDER_INDICATOR_VISIBLE_MODE_NONE);
   elm_slider_value_set(ui->connection.opacity, ui->config->window_opacity);
   elm_object_disabled_set(ui->connection.opacity, !ui->config->window_transparent);
   evas_object_size_hint_align_set(ui->connection.opacity, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(ui->connection.opacity, EVAS_HINT_EXPAND, 0);
   elm_box_pack_end(options, ui->connection.opacity);
   evas_object_show(ui->connection.opacity);

   ui->connection.options_error = elm_label_add(options_box);
   elm_label_line_wrap_set(ui->connection.options_error, ELM_WRAP_WORD);
   elaro_ui_wrap_width_set(ui->connection.options_error, 340);
   elm_box_pack_end(options_box, ui->connection.options_error);
   evas_object_show(ui->connection.options_error);
}
