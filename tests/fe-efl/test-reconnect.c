#include <glib.h>
#include "src/engine/elaro_engine.h"

static void submit(Elaro_Engine *engine, Elaro_Engine_Command_Type type,
                   const char *server, const char *text)
{
    Elaro_Engine_Command *c = g_new0(Elaro_Engine_Command, 1);
    c->type = type;
    c->server = g_strdup(server);
    c->target = g_strdup("");
    c->text = g_strdup(text);
    elaro_engine_submit(engine, c);
}

int main(int argc, char **argv)
{
    g_assert_cmpint(argc, ==, 3);
    const char *mode = argv[2];
    const char *quit_text = !strcmp(mode, "quit-message") ? "/QuIt \tGone fishing — back soon"
                          : !strcmp(mode, "quit-default") ? "/quit"
                          : !strcmp(mode, "quit-blank") ? "/quit \t " : NULL;
    Elaro_Engine *engine = elaro_engine_start(argv[0]);
    Elaro_Engine_Command *c = g_new0(Elaro_Engine_Command, 1);
    c->type = ELARO_ENGINE_CONNECT;
    c->host = g_strdup("127.0.0.1");
    c->port = atoi(argv[1]);
    c->nick = g_strdup("tester");
    c->password = g_strdup("");
    c->channels = g_strdup("#saved secret");
    c->auto_reconnect = strcmp(mode, "disabled") != 0;
    elaro_engine_submit(engine, c);
    guint retries = 0, saved = 0, later = 0;
    gboolean done = FALSE, closing = FALSE;
    char *tag = NULL;
    gint64 deadline = g_get_monotonic_time() + 75 * G_TIME_SPAN_SECOND;
    while (g_get_monotonic_time() < deadline) {
        Elaro_Engine_Event *event = elaro_engine_poll(engine);
        if (!event) { g_usleep(1000); continue; }
        if (!strcmp(event->text, "Connected")) {
            if (!tag) tag = g_strdup(event->server);
            else g_assert_cmpstr(tag, ==, event->server);
        }
        if (g_str_has_prefix(event->text, "Reconnecting (attempt")) {
            g_assert_false(done || closing);
            retries++;
        }
        if (event->type == ELARO_ENGINE_JOINED) {
            if (!strcmp(event->target, "#saved")) {
                saved++;
                if (saved == 1) submit(engine, ELARO_ENGINE_INPUT, event->server, "/join #later");
            }
            if (!strcmp(event->target, "#later")) later++;
            if (quit_text && saved == 1 && later == 1 && !closing) {
                submit(engine, ELARO_ENGINE_INPUT, event->server, quit_text);
                closing = TRUE;
            }
            if (saved == 2 && later == 2) {
                g_assert_cmpuint(retries, ==, 1);
                submit(engine, ELARO_ENGINE_CLOSE, event->server, NULL);
                closing = TRUE;
            }
        }
        if (!strcmp(mode, "quit") && !strcmp(event->text, "Reconnecting in 5 seconds…") && !closing) {
            submit(engine, ELARO_ENGINE_INPUT, event->server, "/quit");
            closing = TRUE;
        }
        if (!strcmp(mode, "cancel") && !strcmp(event->text, "Reconnecting in 5 seconds…") && !closing) {
            submit(engine, ELARO_ENGINE_CLOSE, event->server, NULL);
            closing = TRUE;
        }
        if ((!strcmp(mode, "disabled") && !strcmp(event->text, "Disconnected")) ||
            (closing && event->type == ELARO_ENGINE_BUFFER_CLOSED) ||
            ((!strcmp(mode, "quit") || quit_text) && !strcmp(event->text, "Auto-reconnect cancelled")) ||
            !strcmp(event->text, "Auto-reconnect stopped after 5 retries")) {
            if (!closing && strcmp(mode, "disabled")) g_assert_cmpuint(retries, ==, 5);
            done = TRUE;
            deadline = g_get_monotonic_time() + 6 * G_TIME_SPAN_SECOND;
        }
        elaro_engine_event_free(event);
    }
    elaro_engine_stop(engine);
    g_free(tag);
    g_assert_true(done);
    return 0;
}
