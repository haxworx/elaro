#include <glib.h>
#include "src/fe-efl/ui/elaro_ui_private.h"

static Elaro_Ui ui;
static unsigned int opened, killed;

static Eina_Bool backend_added(void *data, int type, void *event)
{
    Ecore_Exe_Event_Add *added = event;
    const char *command = ecore_exe_cmd_get(added->exe);
    if (command && strstr(command, "--music-backend")) {
        /* Simulate a decoder failure without creating a real core dump. */
        ecore_exe_kill(added->exe);
        killed++;
    }
    return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool next_popup(void *data)
{
    if (opened < 8) {
        elaro_ui_music_show(&ui, "http://127.0.0.1:9/test.wav");
        opened++;
        return ECORE_CALLBACK_RENEW;
    }
    g_assert_cmpuint(killed, ==, opened);
    g_assert_nonnull(ui.music);
    /* Close with pending events, then destroy the owner with a popup open. */
    evas_object_del(ui.music);
    g_assert_null(ui.music);
    elaro_ui_music_show(&ui, "http://127.0.0.1:9/test.wav");
    evas_object_del(ui.win);
    g_assert_null(ui.music);
    elm_exit();
    return ECORE_CALLBACK_CANCEL;
}

int main(int argc, char **argv)
{
    g_assert_cmpint(argc, ==, 2);
    ecore_main_loop_glib_always_integrate_disable();
    g_assert_cmpint(elm_init(1, argv), >, 0);
    ui.binary = g_strdup(argv[1]);
    ui.win = elm_win_util_standard_add("music-test", "Music test");
    evas_object_resize(ui.win, 800, 600);
    evas_object_show(ui.win);
    Ecore_Event_Handler *added = ecore_event_handler_add(ECORE_EXE_EVENT_ADD, backend_added, NULL);
    ecore_timer_add(0.2, next_popup, NULL);
    elm_run();
    ecore_event_handler_del(added);
    g_free(ui.binary);
    elm_shutdown();
    return 0;
}
