#include <glib.h>
#include "src/engine/elaro_engine.h"

int main(int argc, char **argv)
{
    const char *commands[] = {
        "/mode +nt", "/MODE #test -m", "/mode * +l 50",
        "/mode #test +k secret", "/mode #test +ov alice bob",
        "/mode #test", "/mode *", "/mode #elsewhere +i"
    };
    Elaro_Engine *engine = elaro_engine_start(argv[0]);
    Elaro_Engine_Command *c = g_new0(Elaro_Engine_Command, 1);
    c->type = ELARO_ENGINE_CONNECT; c->host = g_strdup("127.0.0.1");
    c->port = atoi(argv[1]); c->nick = g_strdup("tester");
    c->username = g_strdup("tester"); c->realname = g_strdup("Tester");
    c->channels = g_strdup("#test"); c->password = g_strdup("");
    elaro_engine_submit(engine, c);
    guint sent = 0, acknowledged = 0;
    gint64 deadline = g_get_monotonic_time() + 40 * G_TIME_SPAN_SECOND;
    while (acknowledged < G_N_ELEMENTS(commands) && g_get_monotonic_time() < deadline) {
        Elaro_Engine_Event *e = elaro_engine_poll(engine);
        if (!e) { g_usleep(1000); continue; }
        if (e->type == ELARO_ENGINE_LINE && !strcmp(e->text, "<mock> mode received")) acknowledged++;
        if ((e->type == ELARO_ENGINE_JOINED ||
             (e->type == ELARO_ENGINE_LINE && !strcmp(e->text, "<mock> mode received") && acknowledged == sent))
            && sent < G_N_ELEMENTS(commands)) {
            c = g_new0(Elaro_Engine_Command, 1);
            c->type = ELARO_ENGINE_INPUT; c->server = g_strdup(e->server);
            /* Explicit channel targets also work from the server buffer. */
            c->target = g_strdup(sent == G_N_ELEMENTS(commands) - 1 ? "" : "#test");
            c->text = g_strdup(commands[sent++]);
            elaro_engine_submit(engine, c);
        }
        elaro_engine_event_free(e);
    }
    elaro_engine_stop(engine);
    g_assert_cmpuint(acknowledged, ==, G_N_ELEMENTS(commands));
    return 0;
}
