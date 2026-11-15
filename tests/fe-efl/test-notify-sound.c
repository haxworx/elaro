#include "src/fe-efl/ui/elaro_ui_private.h"
#include <Eet.h>
#include <assert.h>

int main(int argc, char **argv)
{
    Elaro_Ui ui = {0};
    const char *path, *group;
    Evas_Object *sound;
    Eet_File *archive;
    char **samples;
    int count;

    ecore_main_loop_glib_always_integrate_disable();
    assert(elm_init(argc, argv) > 0);
    /* Check playback wiring without making the test suite produce sound. */
    edje_audio_channel_mute_set(EDJE_CHANNEL_ALERT, EINA_TRUE);
    ui.win = elm_win_util_standard_add("sound-test", "Sound test");
    elaro_ui_notify_sound(&ui);
    sound = evas_object_data_get(ui.win, "private-message-sound");
    assert(sound);
    elm_layout_file_get(sound, &path, &group);
    assert(!strcmp(group, "elaro/sounds"));
    archive = eet_open(path, EET_FILE_MODE_READ);
    assert(archive);
    samples = eet_list(archive, "edje/sounds/*", &count);
    assert(samples && count > 0);
    free(samples);
    eet_close(archive);
    edje_object_message_signal_process(elm_layout_edje_get(sound));
    elaro_ui_notify_sound(&ui);
    assert(evas_object_data_get(ui.win, "private-message-sound") == sound);
    edje_object_message_signal_process(elm_layout_edje_get(sound));
    evas_object_del(ui.win);
    elm_shutdown();
    return 0;
}
