/* Native EFL playback runs separately so codec failures cannot end an IRC session. */
#include <Elementary.h>
#include "elaro_util.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "elaro_music_backend.h"

typedef struct
{
   Evas_Object *video;
   Eina_Strbuf *input;
   Eina_Bool opened, requested, failed;
   double started;
} Backend;

static void
_failure(Backend *backend)
{
   if (backend->failed) return;

   backend->failed = EINA_TRUE;
   puts("ERROR");
   fflush(stdout);
}

static void
_ready_cb(void *data, Evas_Object *obj, void *info)
{
   ((Backend *)data)->opened = EINA_TRUE;
}

static void
_decode_stopped_cb(void *data, Evas_Object *obj, void *info)
{
   Backend *backend;

   backend = data;

   if (!backend->opened && backend->requested) _failure(backend);
}

static Eina_Bool
_report_cb(void *data)
{
   Backend *backend;
   double position, length;
   Eina_Bool playing;
   char pos[64], len[64];

   backend = data;

   if (backend->failed) return ECORE_CALLBACK_RENEW;

   position = elm_video_play_position_get(backend->video);
   length = elm_video_play_length_get(backend->video);

   if (!isfinite(position) || position < 0 || position > INT_MAX) position = 0;

   if (!isfinite(length) || length < 0 || length > INT_MAX) length = 0;
   playing = elm_video_is_playing_get(backend->video);

   if (length > 0 || position > 0) backend->opened = EINA_TRUE;

   if (!backend->opened && backend->requested && ecore_time_get() - backend->started > 30.0)
     {
        _failure(backend);

        return ECORE_CALLBACK_RENEW;
     }

   printf("STATE %s %s %d %d %d\n", elaro_double_format(pos, sizeof(pos), position),
          elaro_double_format(len, sizeof(len), length), !!playing,
          !!elm_video_is_seekable_get(backend->video), !!backend->opened);
   fflush(stdout);

   return ECORE_CALLBACK_RENEW;
}

static void
_command(Backend *backend, const char *line)
{
   char *url, *p, *end;
   double length, value;
   Eina_Bool volume;

   if (eina_str_has_prefix(line, "OPEN "))
     {
        url = elaro_uri_unescape(line + 5);

        if (!url || (strncasecmp(url, "http://", 7) && strncasecmp(url, "https://", 8)))
           _failure(backend);
        else
          {
             for (p = url; *p && *p != ':'; p++)
                *p = tolower(*p);

             if (!elm_video_file_set(backend->video, url))
               {
                  _failure(backend);
                  free(url);

                  return;
               }
             backend->started = ecore_time_get();
             backend->requested = EINA_TRUE;
             elm_video_play(backend->video);
          }
        free(url);
     }
   else if (!strcmp(line, "PLAY"))
     {
        length = elm_video_play_length_get(backend->video);

        if (length > 0 && elm_video_play_position_get(backend->video) >= length - 0.1)
           elm_video_play_position_set(backend->video, 0);
        backend->requested = EINA_TRUE;
        elm_video_play(backend->video);
     }
   else if (!strcmp(line, "PAUSE") || !strcmp(line, "STOP"))
     {
        backend->requested = EINA_FALSE;
        elm_video_pause(backend->video);

        if (!strcmp(line, "STOP")) elm_video_play_position_set(backend->video, 0);
     }
   else if (eina_str_has_prefix(line, "SEEK ") || eina_str_has_prefix(line, "VOLUME "))
     {
        volume = eina_str_has_prefix(line, "VOLUME ");

        value = eina_convert_strtod_c(line + (volume ? 7 : 5), &end);

        if (*end || !isfinite(value) || value < 0) return;

        if (volume) elm_video_audio_level_set(backend->video, MIN(value, 1.0));
        else if (elm_video_is_seekable_get(backend->video))
           elm_video_play_position_set(backend->video,
                                       MIN(value, elm_video_play_length_get(backend->video)));
     }
}

static Eina_Bool
_input_cb(void *data, Ecore_Fd_Handler *handler)
{
   Backend *backend;
   char bytes[4096];
   ssize_t length;
   const char *newline;
   char *command;
   size_t consumed;

   backend = data;

   length = read(STDIN_FILENO, bytes, sizeof(bytes));

   if (length <= 0)
     {
        elm_exit();

        return ECORE_CALLBACK_RENEW;
     }
   eina_strbuf_append_length(backend->input, bytes, length);

   if (eina_strbuf_length_get(backend->input) > 65536)
     {
        elm_exit();

        return ECORE_CALLBACK_RENEW;
     }

   while ((newline = strchr(eina_strbuf_string_get(backend->input), '\n')))
     {
        consumed = newline - eina_strbuf_string_get(backend->input) + 1;
        command = strndup(eina_strbuf_string_get(backend->input), consumed - 1);
        if (!command) return ECORE_CALLBACK_RENEW;
        _command(backend, command);
        free(command);
        eina_strbuf_remove(backend->input, 0, consumed);
     }

   return ECORE_CALLBACK_RENEW;
}

int
elaro_music_backend_run(int argc, char **argv)
{
   Evas_Object *win, *emotion;
   Backend backend;
   Ecore_Fd_Handler *input;
   Ecore_Timer *timer;

   /* No visible window; the helper does not initialise IRC or read configuration. */
   setenv("ELM_ENGINE", "buffer", 1);
   ecore_main_loop_glib_always_integrate_disable();

   if (!elm_init(1, argv)) return 1;

   win = elm_win_add(NULL, "elaro-music-backend", ELM_WIN_BASIC);
   backend = (Backend){ 0 };

   backend.video = elm_video_add(win);
   elm_video_remember_position_set(backend.video, EINA_FALSE);
   emotion = elm_video_emotion_get(backend.video);
   evas_object_smart_callback_add(emotion, "open_done", _ready_cb, &backend);
   evas_object_smart_callback_add(emotion, "playback_started", _ready_cb, &backend);
   evas_object_smart_callback_add(emotion, "decode_stop", _decode_stopped_cb, &backend);
   backend.input = eina_strbuf_new();
   if (!backend.input) goto done;
   input = ecore_main_fd_handler_add(STDIN_FILENO, ECORE_FD_READ, _input_cb, &backend, NULL, NULL);
   timer = ecore_timer_add(0.25, _report_cb, &backend);
   elm_run();
   /* EOF is normally followed by process termination by the owning popup. */
   ecore_timer_del(timer);

   if (input) ecore_main_fd_handler_del(input);
done:
   if (backend.input) eina_strbuf_free(backend.input);
   /* Emotion shutdown may iterate Ecore; keep its canvas alive until decoding is gone. */
   evas_object_smart_callback_del(emotion, "open_done", _ready_cb);
   evas_object_smart_callback_del(emotion, "playback_started", _ready_cb);
   evas_object_smart_callback_del(emotion, "decode_stop", _decode_stopped_cb);
   evas_object_del(backend.video);
   evas_object_del(win);
   elm_shutdown();

   return 0;
}
