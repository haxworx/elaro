#include <glib.h>
#include "src/fe-efl/ui/elaro_ui.c"

static Elaro_Ui ui;
static Elaro_Ui_Buffer channel;
static gboolean sent, disconnected;
static const char *input;

static Eina_Bool receive_events(void *data, Ecore_Fd_Handler *handler)
{
    Elaro_Engine_Event *event;
    while ((event = elaro_engine_poll(ui.engine))) {
        if (event->type == ELARO_ENGINE_JOINED && !sent) {
            channel.server = strdup(event->server);
            channel.target = strdup(event->target);
            ui.current = &channel;
            char *markup = elm_entry_utf8_to_markup(input);
            elm_entry_entry_set(ui.input, markup);
            free(markup);
            evas_object_smart_callback_call(ui.input, "activated", NULL);
            sent = TRUE;
        }
        if (!strcmp(event->text, "Auto-reconnect cancelled")) {
            disconnected = TRUE;
            ecore_main_loop_quit();
        }
        elaro_engine_event_free(event);
    }
    return ECORE_CALLBACK_RENEW;
}

static Eina_Bool timed_out(void *data)
{
    ecore_main_loop_quit();
    return ECORE_CALLBACK_CANCEL;
}

int main(int argc, char **argv)
{
    ecore_main_loop_glib_always_integrate_disable();
    g_assert_cmpint(elm_init(argc, argv), >, 0);
    input = argv[2];
    ui.win = elm_win_util_standard_add("quit-ui-test", "Quit UI test");
    ui.input = elm_entry_add(ui.win);
    elm_entry_single_line_set(ui.input, EINA_TRUE);
    elm_entry_scrollable_set(ui.input, EINA_TRUE);
    evas_object_smart_callback_add(ui.input, "activated", _send_input_cb, &ui);
    ui.engine = elaro_engine_start(argv[0]);
    g_assert_nonnull(ui.engine);
    Ecore_Fd_Handler *events = ecore_main_fd_handler_add(
        elaro_engine_event_fd(ui.engine), ECORE_FD_READ, receive_events, NULL, NULL, NULL);
    g_assert_nonnull(events);
    Elaro_Engine_Command *command = calloc(1, sizeof(*command));
    command->type = ELARO_ENGINE_CONNECT;
    command->host = strdup("127.0.0.1");
    command->port = atoi(argv[1]);
    command->nick = strdup("tester");
    command->password = strdup("");
    command->channels = strdup("#test");
    command->auto_reconnect = TRUE;
    elaro_engine_submit(ui.engine, command);
    Ecore_Timer *timeout = ecore_timer_add(10, timed_out, NULL);
    ecore_main_loop_begin();
    if (disconnected) ecore_timer_del(timeout);
    ecore_main_fd_handler_del(events);
    elaro_engine_stop(ui.engine);
    g_assert_true(sent);
    g_assert_true(disconnected);
    g_assert_cmpstr(eina_list_data_get(channel.input_history), ==, input);
    char *history;
    EINA_LIST_FREE(channel.input_history, history) free(history);
    free(channel.server);
    free(channel.target);
    evas_object_del(ui.win);
    elm_shutdown();
    return 0;
}
