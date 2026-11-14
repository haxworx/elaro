#include <glib.h>
#include "src/engine/elaro_engine.h"
static void request(Elaro_Engine *engine, const char *server, guint id, const char *mask)
{
    Elaro_Engine_Command *c = g_new0(Elaro_Engine_Command, 1);
    c->type = mask ? ELARO_ENGINE_UNBAN : ELARO_ENGINE_BANS;
    c->server = g_strdup(server); c->target = g_strdup("#test");
    c->text = g_strdup(mask); c->request_id = id;
    elaro_engine_submit(engine, c);
}
int main(int argc, char **argv)
{
    Elaro_Engine *engine = elaro_engine_start(argv[0]);
    Elaro_Engine_Command *c = g_new0(Elaro_Engine_Command, 1);
    c->type = ELARO_ENGINE_CONNECT; c->host = g_strdup("127.0.0.1");
    c->port = atoi(argv[1]); c->nick = g_strdup("tester");
    c->username = g_strdup("tester"); c->realname = g_strdup("Tester");
    c->channels = g_strdup("#test"); c->password = g_strdup("");
    elaro_engine_submit(engine, c);
    guint entries = 0, stage = 0;
    gboolean done = FALSE;
    gint64 deadline = g_get_monotonic_time() + 35 * G_TIME_SPAN_SECOND;
    while (!done && g_get_monotonic_time() < deadline) {
        Elaro_Engine_Event *e = elaro_engine_poll(engine);
        if (!e) { g_usleep(1000); continue; }
        if (e->type == ELARO_ENGINE_JOINED) { stage = 1; request(engine, e->server, stage, NULL); }
        if (e->type == ELARO_ENGINE_BAN_ENTRY || e->type == ELARO_ENGINE_BAN_END || e->type == ELARO_ENGINE_BAN_ERROR) {
            g_assert_cmpstr(e->target, ==, "#test");
            g_assert_cmpuint(e->request_id, ==, stage);
            if (e->type == ELARO_ENGINE_BAN_ENTRY) {
                g_assert_cmpuint(stage, ==, 1);
                g_assert_cmpstr(e->text, ==, "*!*@bad.example"); entries++;
            } else if (stage == 1) {
                g_assert_cmpint(e->type, ==, ELARO_ENGINE_BAN_END);
                g_assert_cmpuint(entries, ==, 1);
                stage = 2; request(engine, e->server, stage, "*!*@bad.example");
            } else if (stage == 2) {
                g_assert_cmpint(e->type, ==, ELARO_ENGINE_BAN_END);
                stage = 3; request(engine, e->server, stage, NULL);
            } else {
                g_assert_cmpint(e->type, ==, ELARO_ENGINE_BAN_ERROR);
                g_assert_cmpstr(e->text, ==, "Channel operator privileges needed");
                done = TRUE;
            }
        }
        elaro_engine_event_free(e);
    }
    elaro_engine_stop(engine);
    g_assert_true(done);
    return 0;
}
