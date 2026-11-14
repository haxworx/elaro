#include <glib.h>
#include "src/engine/elaro_engine.h"

static void
command(Elaro_Engine *engine, Elaro_Engine_Command_Type type,
        const char *server, const char *target, const char *text)
{
    Elaro_Engine_Command *c = g_new0(Elaro_Engine_Command, 1);
    c->type = type;
    c->server = g_strdup(server);
    c->target = g_strdup(target);
    c->text = g_strdup(text);
    elaro_engine_submit(engine, c);
}

int main(int argc, char **argv)
{
    Elaro_Engine *engine = elaro_engine_start(argv[0]);
    Elaro_Engine_Command *c = g_new0(Elaro_Engine_Command, 1);
    c->type = ELARO_ENGINE_CONNECT;
    c->host = g_strdup("127.0.0.1");
    c->port = atoi(argv[1]);
    c->nick = g_strdup("tester");
    c->username = g_strdup("tester");
    c->password = g_strdup("");
    c->channels = g_strdup("#presence");
    elaro_engine_submit(engine, c);
    unsigned int phase = 0;
    gboolean bob = FALSE, outsider = FALSE, done = FALSE;
    unsigned int own_phase = 0;
    gint64 deadline = g_get_monotonic_time() + 40 * G_TIME_SPAN_SECOND;
    while (!done && g_get_monotonic_time() < deadline) {
        Elaro_Engine_Event *e = elaro_engine_poll(engine);
        if (!e) { g_usleep(1000); continue; }
        if (e->type == ELARO_ENGINE_JOINED) {
            command(engine, ELARO_ENGINE_SELECT, e->server, "#presence", NULL);
            command(engine, ELARO_ENGINE_INPUT, e->server, "#presence", "/away Out for lunch");
        }
        if (phase == 0 && e->type == ELARO_ENGINE_NICKS) {
            for (unsigned int i = 0; i < e->user_count; i++) {
                if (!strcmp(e->users[i].nick, "alice")) g_assert_true(e->users[i].away);
                if (strcmp(e->users[i].nick, "bob")) continue;
                if (!e->users[i].away) continue;
                g_assert_cmpint(e->users[i].prefix, ==, '+');
                command(engine, ELARO_ENGINE_QUERY, e->server, "bob", NULL);
                command(engine, ELARO_ENGINE_QUERY, e->server, "outsider", NULL);
                phase = 1;
            }
        }
        if (e->type == ELARO_ENGINE_USER_PRESENCE) {
            if (!strcmp(e->target, "tester")) {
                if (e->away && own_phase == 0) {
                    own_phase = 1;
                    command(engine, ELARO_ENGINE_INPUT, e->server, "", "/AWAY -one");
                } else if (!e->away && own_phase == 1) {
                    own_phase = 2;
                    command(engine, ELARO_ENGINE_INPUT, e->server, "bob", "/away -one Out for lunch");
                } else if (e->away && own_phase == 2) {
                    own_phase = 3;
                    command(engine, ELARO_ENGINE_INPUT, e->server, "bob", "/away");
                } else if (!e->away && own_phase == 3) own_phase = 4;
            }
            if (phase == 1 && !strcmp(e->target, "bob") && e->away) {
                command(engine, ELARO_ENGINE_INPUT, e->server, "control", "begin");
                phase = 2;
            } else if (phase == 2) {
                if (!strcmp(e->target, "bob") && !e->away) bob = TRUE;
                if (!strcmp(e->target, "outsider") && e->away) outsider = TRUE;
                if (bob && outsider) {
                    bob = outsider = FALSE;
                    command(engine, ELARO_ENGINE_INPUT, e->server, "control", "next");
                    phase = 3;
                }
            } else if (phase == 3) {
                if (!strcmp(e->target, "bob") && e->away) bob = TRUE;
                if (!strcmp(e->target, "outsider") && !e->away) outsider = TRUE;
                if (bob && outsider) {
                    command(engine, ELARO_ENGINE_USER_INFO, e->server, "bob", NULL);
                    phase = 4;
                }
            } else if (phase == 4 && !strcmp(e->target, "bob")) {
                /* A split WHOIS 311/301/318 must not publish a spurious back. */
                g_assert_true(e->away);
            } else if (phase == 5 && !strcmp(e->target, "bob") && !e->away) {
                command(engine, ELARO_ENGINE_USER_INFO, e->server, "outsider", NULL);
                phase = 6;
            } else if (phase == 6 && !strcmp(e->target, "outsider") && e->away) {
                command(engine, ELARO_ENGINE_USER_INFO, e->server, "outsider", NULL);
                phase = 7;
            } else if (phase == 7 && !strcmp(e->target, "outsider") && !e->away) {
                command(engine, ELARO_ENGINE_INPUT, e->server, "control", "disconnect");
                phase = 8;
            }
        }
        if (phase == 4 && e->type == ELARO_ENGINE_LINE && strstr(e->text, "whois-away-done")) {
            command(engine, ELARO_ENGINE_USER_INFO, e->server, "bob", NULL);
            phase = 5;
        }
        if (phase == 8 && e->type == ELARO_ENGINE_PRESENCE_RESET && !*e->target) done = TRUE;
        elaro_engine_event_free(e);
    }
    elaro_engine_stop(engine);
    g_assert_true(done);
    g_assert_cmpuint(own_phase, ==, 4);
    return 0;
}
