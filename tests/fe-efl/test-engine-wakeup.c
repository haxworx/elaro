#include <glib.h>
#include <poll.h>
#include <errno.h>
#include "src/engine/elaro_engine.h"

static void
wait_readable(int fd)
{
    struct pollfd watch = { .fd = fd, .events = POLLIN };
    int result;
    do result = poll(&watch, 1, 3000); while (result < 0 && errno == EINTR);
    g_assert_cmpint(result, ==, 1);
    g_assert_cmpint(watch.revents, ==, POLLIN);
}

int main(int argc, char **argv)
{
    Elaro_Engine *engine = elaro_engine_start(argv[0]);
    g_assert_nonnull(engine);
    int fd = elaro_engine_event_fd(engine);
    gboolean ready = FALSE;
    while (!ready) {
        wait_readable(fd);
        Elaro_Engine_Event *event = elaro_engine_poll(engine);
        g_assert_nonnull(event);
        ready = g_str_has_prefix(event->text, "Ready");
        elaro_engine_event_free(event);
    }

    /* Exceed both dispatch batch limits and normal pipe capacity. Repeat
     * after draining to exercise the empty-to-nonempty transition again. */
    for (unsigned int round = 0; round < 3; round++) {
        for (unsigned int i = 0; i < 10000; i++) {
            Elaro_Engine_Command *command = g_new0(Elaro_Engine_Command, 1);
            command->type = ELARO_ENGINE_INPUT;
            command->server = g_strdup_printf("request-%u", i);
            command->target = g_strdup("");
            command->text = g_strdup("/quit");
            elaro_engine_submit(engine, command);
        }
        for (unsigned int i = 0; i < 10000; i++) {
            wait_readable(fd);
            Elaro_Engine_Event *event = elaro_engine_poll(engine);
            g_assert_nonnull(event);
            char *expected = g_strdup_printf("request-%u", i);
            g_assert_cmpstr(event->server, ==, expected);
            g_assert_cmpstr(event->text, ==, "Auto-reconnect cancelled");
            g_free(expected);
            elaro_engine_event_free(event);
        }
        struct pollfd watch = { .fd = fd, .events = POLLIN };
        g_assert_cmpint(poll(&watch, 1, 0), ==, 0);
        g_assert_null(elaro_engine_poll(engine));
    }
    elaro_engine_stop(engine);
    return 0;
}
