#include <glib.h>
#include <poll.h>
#include "src/engine/elaro_engine.h"

int main(int argc, char **argv)
{
    Elaro_Engine *engine = elaro_engine_start(argv[0]);
    g_assert_nonnull(engine);
    Elaro_Engine_Command *command = g_new0(Elaro_Engine_Command, 1);
    command->type = ELARO_ENGINE_CONNECT;
    command->host = g_strdup("127.0.0.1");
    command->port = atoi(argv[1]);
    command->nick = g_strdup(argv[2]);
    command->username = g_strdup("receiver");
    command->channels = g_strdup("#test");
    command->password = g_strdup("");
    command->tls = TRUE;
    elaro_engine_submit(engine, command);
    unsigned int received = 0;
    gint64 deadline = g_get_monotonic_time() + 8 * G_TIME_SPAN_SECOND;
    while (received < 100 && g_get_monotonic_time() < deadline) {
        struct pollfd watch = { .fd = elaro_engine_event_fd(engine), .events = POLLIN };
        poll(&watch, 1, 100);
        Elaro_Engine_Event *event;
        while ((event = elaro_engine_poll(engine))) {
            if (event->type == ELARO_ENGINE_LINE && !strcmp(event->target, "#test")
                && g_str_has_prefix(event->text, "<sender> message-")) {
                char *prefix = g_strdup_printf("<sender> message-%03u ", received);
                g_assert_true(g_str_has_prefix(event->text, prefix));
                g_free(prefix);
                received++;
            }
            elaro_engine_event_free(event);
        }
    }
    g_printerr("%s received %u/100 messages\n", argv[2], received);
    elaro_engine_stop(engine);
    return received == 100 ? 0 : 1;
}
