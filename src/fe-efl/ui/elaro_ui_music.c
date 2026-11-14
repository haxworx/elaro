#include "elaro_ui_private.h"
#include <math.h>

typedef struct
{
   Elaro_Ui *ui;
   Evas_Object *box, *play, *seek, *time, *status, *pulse, *controls, *stop;
   Ecore_Exe *backend;
   Ecore_Event_Handler *data_handler, *exit_handler;
   Ecore_Timer *load_timer;
   char *url;
   double volume;
   Eina_Bool seeking, playing, opened, failed, closing;
} Music;

static void
_button_icon_set(Evas_Object *button, const char *name, const char *label)
{
   Evas_Object *icon;
   const char *current;

   icon = elm_object_content_get(button);

   if (!icon)
     {

        icon = elm_icon_add(button);
        elm_object_content_set(button, icon);
     }
   current = elm_icon_standard_get(icon);

   if (current && !strcmp(current, name)) return;

   elm_icon_standard_set(icon, name);
   evas_object_size_hint_min_set(icon, ELM_SCALE_SIZE(20), ELM_SCALE_SIZE(20));
   evas_object_size_hint_max_set(icon, ELM_SCALE_SIZE(20), ELM_SCALE_SIZE(20));
   elm_object_tooltip_text_set(button, label);
   elm_object_access_info_set(button, label);
}

static void
_backend_stop(Music *music)
{
   if (music->load_timer)
     {
        ecore_timer_del(music->load_timer);
        music->load_timer = NULL;
     }

   if (!music->backend) return;
   /* Codec teardown can also fail; keep it outside the UI process. Ecore reaps it. */
   ecore_exe_kill(music->backend);
   music->backend = NULL;
}

static void
_progress_show(Music *music)
{
   if (!music->pulse) return;

   elm_box_pack_before(music->box, music->status, music->pulse);
   elm_box_unpack(music->box, music->pulse);
   evas_object_del(music->pulse);
   music->pulse = NULL;
   evas_object_show(music->status);
}

static void
_music_failed(Music *music)
{
   _backend_stop(music);
   music->failed = EINA_TRUE;
   music->playing = EINA_FALSE;
   _progress_show(music);
   elm_progressbar_value_set(music->status, 0);
   elm_object_text_set(music->status, _("Audio unavailable"));
   elm_object_tooltip_text_set(music->status, _("Audio unavailable. Use Retry to try again."));
   elm_object_disabled_set(music->controls, EINA_FALSE);
   elm_object_disabled_set(music->stop, EINA_TRUE);
   elm_object_disabled_set(music->seek, EINA_TRUE);
   _button_icon_set(music->play, "view-refresh", _("Retry"));
}

static void
_music_send(Music *music, const char *command)
{
   if (music->closing) return;

   if (!music->backend || !ecore_exe_send(music->backend, command, strlen(command)))
      _music_failed(music);
}

static void
_music_number_send(Music *music, const char *command, double value)
{
   char number[64];
   char *line;

   line = elaro_string_printf("%s %s\n", command,
                              elaro_double_format(number, sizeof(number), value));
   _music_send(music, line);
   free(line);
}

static Eina_Bool
_music_data_cb(void *data, int type, void *event)
{
   Music *music;
   Ecore_Exe_Event_Data *output;
   int i, elapsed, total;
   const char *line;
   char **fields;
   char *end_pos, *end_length, *time;
   double position, length;
   Eina_Bool seekable;
   unsigned int count;

   music = data;
   output = event;

   if (output->exe != music->backend || !output->lines) return ECORE_CALLBACK_PASS_ON;

   for (i = 0; output->lines[i].line; i++)
     {
        line = output->lines[i].line;

        if (!strcmp(line, "ERROR"))
          {
             _music_failed(music);
             break;
          }

        if (!eina_str_has_prefix(line, "STATE ")) continue;

        fields = eina_str_split_full(line, " ", 0, &count);

        if (!fields || count != 6)
          {
             if (fields) free(fields[0]);
             free(fields);
             continue;
          }

        position = eina_convert_strtod_c(fields[1], &end_pos);
        length = eina_convert_strtod_c(fields[2], &end_length);

        if (*end_pos || *end_length || !isfinite(position) || !isfinite(length) || position < 0
            || length < 0 || position > INT_MAX || length > INT_MAX)
          {
             if (fields) free(fields[0]);
             free(fields);
             continue;
          }
        music->playing = !strcmp(fields[3], "1");
        seekable = !strcmp(fields[4], "1");

        if (!strcmp(fields[5], "1")) music->opened = EINA_TRUE;
        if (fields) free(fields[0]);
        free(fields);

        if (music->opened)
          {
             if (music->load_timer)
               {
                  ecore_timer_del(music->load_timer);
                  music->load_timer = NULL;
               }
             _progress_show(music);
             elm_progressbar_value_set(music->status, 1);
             elm_object_text_set(music->status, music->playing ? _("Playing") : _("Paused"));
             elm_object_disabled_set(music->controls, EINA_FALSE);
          }

        if (!music->seeking)
          {
             elm_slider_min_max_set(music->seek, 0, MAX(1.0, length));
             elm_slider_value_set(music->seek, position);
          }
        elm_object_disabled_set(music->seek, !music->opened || !seekable);
        _button_icon_set(music->play,
                         music->playing ? "media-playback-pause" : "media-playback-start",
                         music->playing ? _("Pause") : _("Play"));
        elapsed = (int)position;
        total = (int)length;
        time = length > 0 ? elaro_string_printf("%d:%02d / %d:%02d", elapsed / 60, elapsed % 60,
                                                total / 60, total % 60)
                          : elaro_string_printf("%d:%02d / --:--", elapsed / 60, elapsed % 60);
        elm_object_text_set(music->time, time);
        free(time);
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_music_exit_cb(void *data, int type, void *event)
{
   Music *music;
   Ecore_Exe_Event_Del *exit;

   music = data;
   exit = event;

   if (music->backend && exit->exe == music->backend)
     {
        music->backend = NULL;
        _music_failed(music);
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_music_timeout_cb(void *data)
{
   Music *music;

   music = data;
   music->load_timer = NULL;
   _music_failed(music);

   return ECORE_CALLBACK_CANCEL;
}

static void
_music_start(Music *music)
{
   char *binary, *command, *url;

   if (music->closing) return;

   _backend_stop(music);
   music->opened = music->failed = EINA_FALSE;
   elm_object_disabled_set(music->status, EINA_FALSE);
   elm_object_tooltip_unset(music->status);
   elm_progressbar_value_set(music->status, 0);
   elm_object_disabled_set(music->controls, EINA_TRUE);
   elm_object_disabled_set(music->stop, EINA_FALSE);

   if (!music->pulse)
     {
        elm_box_unpack(music->box, music->status);
        evas_object_hide(music->status);

        music->pulse = elm_progressbar_add(music->box);
        evas_object_size_hint_weight_set(music->pulse, EVAS_HINT_EXPAND, 0);
        evas_object_size_hint_align_set(music->pulse, EVAS_HINT_FILL, 0.5);
        elm_progressbar_span_size_set(music->pulse, elm_progressbar_span_size_get(music->status));
        elm_object_text_set(music->pulse, _("Loading audio…"));
        elm_progressbar_pulse_set(music->pulse, EINA_TRUE);
        elm_progressbar_pulse(music->pulse, EINA_TRUE);
        elm_box_pack_end(music->box, music->pulse);
        evas_object_show(music->pulse);
     }

   if (!music->ui->binary)
     {
        _music_failed(music);

        return;
     }
   binary = elaro_command_quote(music->ui->binary);
   command = elaro_string_printf("%s --music-backend", binary);
   music->backend
       = ecore_exe_pipe_run(command,
                            ECORE_EXE_PIPE_READ | ECORE_EXE_PIPE_WRITE
                                | ECORE_EXE_PIPE_READ_LINE_BUFFERED | ECORE_EXE_TERM_WITH_PARENT,
                            NULL);
   free(command);
   free(binary);

   if (!music->backend)
     {
        _music_failed(music);

        return;
     }
   music->load_timer = ecore_timer_add(30.0, _music_timeout_cb, music);
   url = elaro_uri_escape(music->url);
   command = elaro_string_printf("OPEN %s\n", url);
   _music_send(music, command);
   free(url);
   free(command);

   if (!music->failed) _music_number_send(music, "VOLUME", music->volume);
}

static void
_music_deleted_cb(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   Music *music;

   music = data;
   music->closing = EINA_TRUE;
   ecore_event_handler_del(music->data_handler);
   ecore_event_handler_del(music->exit_handler);
   _backend_stop(music);
   music->ui->music = NULL;
}

static void
_music_free_cb(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   Music *music;

   music = data;
   free(music->url);
   free(music);
}

static void
_music_close_cb(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui;

   ui = data;

   if (ui->music) evas_object_del(ui->music);
}

static void
_music_play_cb(void *data, Evas_Object *obj, void *info)
{
   Music *music;

   music = data;

   if (music->failed) _music_start(music);
   else _music_send(music, music->playing ? "PAUSE\n" : "PLAY\n");
}

static void
_music_stop_cb(void *data, Evas_Object *obj, void *info)
{
   _music_send(data, "STOP\n");
}

static void
_music_seek_start_cb(void *data, Evas_Object *obj, void *info)
{
   ((Music *)data)->seeking = EINA_TRUE;
}

static void
_music_seek_cb(void *data, Evas_Object *obj, void *info)
{
   Music *music;

   music = data;

   if (music->opened && !music->failed)
      _music_number_send(music, "SEEK", elm_slider_value_get(obj));
}

static void
_music_seek_stop_cb(void *data, Evas_Object *obj, void *info)
{
   _music_seek_cb(data, obj, info);
   ((Music *)data)->seeking = EINA_FALSE;
}

static void
_music_volume_cb(void *data, Evas_Object *obj, void *info)
{
   Music *music;

   music = data;
   music->volume = elm_slider_value_get(obj) / 100.0;
   _music_number_send(music, "VOLUME", music->volume);
}

void
elaro_ui_music_show(Elaro_Ui *ui, const char *url)
{
   Music *music;
   Evas_Object *box, *controls, *stop, *volume;

   if (ui->music) evas_object_del(ui->music);
   music = calloc(1, sizeof(*music));
   music->ui = ui;
   music->volume = 1.0;
   music->url = strdup(url);

   ui->music = elm_popup_add(ui->win);
   elm_object_part_text_set(ui->music, "title,text", _("Audio preview"));
   evas_object_smart_callback_add(ui->music, "dismissed", _music_close_cb, ui);
   box = music->box = elaro_ui_media_box_add(ui->music, url, _("Audio"));

   music->seek = elm_slider_add(box);
   elm_slider_unit_format_set(music->seek, "");
   elm_slider_indicator_show_set(music->seek, EINA_FALSE);

   /* Span sizes are scaled by Elementary, unlike Evas size hints. */
   elm_slider_span_size_set(music->seek, 240);
   evas_object_size_hint_weight_set(music->seek, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(music->seek, EVAS_HINT_FILL, 0.5);
   elm_object_disabled_set(music->seek, EINA_TRUE);
   evas_object_smart_callback_add(music->seek, "slider,drag,start", _music_seek_start_cb, music);
   evas_object_smart_callback_add(music->seek, "slider,drag,stop", _music_seek_stop_cb, music);
   evas_object_smart_callback_add(music->seek, "delay,changed", _music_seek_cb, music);
   elm_box_pack_end(box, music->seek);
   evas_object_show(music->seek);

   music->time = elm_label_add(box);
   elm_object_text_set(music->time, "0:00 / --:--");
   elm_box_pack_end(box, music->time);
   evas_object_show(music->time);
   controls = music->controls = elm_box_add(box);
   elm_box_horizontal_set(controls, EINA_TRUE);
   elm_box_padding_set(controls, ELM_SCALE_SIZE(12), 0);

   music->play = elm_button_add(controls);
   _button_icon_set(music->play, "media-playback-pause", _("Pause"));
   evas_object_smart_callback_add(music->play, "clicked", _music_play_cb, music);
   elm_box_pack_end(controls, music->play);
   evas_object_show(music->play);
   stop = music->stop = elm_button_add(controls);
   _button_icon_set(stop, "media-playback-stop", _("Stop"));
   evas_object_smart_callback_add(stop, "clicked", _music_stop_cb, music);
   elm_box_pack_end(controls, stop);
   evas_object_show(stop);

   volume = elm_slider_add(controls);
   elm_object_text_set(volume, _("Volume"));
   elm_slider_min_max_set(volume, 0, 100);
   elm_slider_value_set(volume, 100.0);
   elm_slider_unit_format_set(volume, "%.0f%%");
   elm_slider_span_size_set(volume, 100);
   evas_object_smart_callback_add(volume, "changed", _music_volume_cb, music);
   elm_box_pack_end(controls, volume);
   evas_object_show(volume);
   elm_box_pack_end(box, controls);
   evas_object_show(controls);

   music->status = elm_progressbar_add(box);
   evas_object_size_hint_weight_set(music->status, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(music->status, EVAS_HINT_FILL, 0.5);
   elm_progressbar_span_size_set(music->status, 180);
   elm_progressbar_unit_format_set(music->status, "");
   music->data_handler = ecore_event_handler_add(ECORE_EXE_EVENT_DATA, _music_data_cb, music);
   music->exit_handler = ecore_event_handler_add(ECORE_EXE_EVENT_DEL, _music_exit_cb, music);
   evas_object_event_callback_add(ui->music, EVAS_CALLBACK_DEL, _music_deleted_cb, music);
   evas_object_event_callback_add(ui->music, EVAS_CALLBACK_FREE, _music_free_cb, music);
   elm_object_part_content_set(ui->music, "button1",
                               elaro_ui_button_add(ui->music, _("Close"), _music_close_cb, ui));
   _music_start(music);
   evas_object_show(ui->music);
}
