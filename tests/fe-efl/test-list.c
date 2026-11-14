#include <glib.h>
#include "src/engine/elaro_engine.h"
static void submit(Elaro_Engine *engine, const char *server, Elaro_Engine_Command_Type type,
                   guint id, const char *text)
{
    Elaro_Engine_Command *c = g_new0(Elaro_Engine_Command, 1);
    c->type = type; c->server = g_strdup(server); c->target = g_strdup("");
    c->request_id = id; c->text = g_strdup(text);
    elaro_engine_submit(engine, c);
}
int main(int argc, char **argv)
{
    Elaro_Engine *engine = elaro_engine_start(argv[0]);
    Elaro_Engine_Command *c = g_new0(Elaro_Engine_Command, 1);
    c->type = ELARO_ENGINE_CONNECT; c->host = g_strdup("127.0.0.1");
    c->port = atoi(argv[1]); c->nick = g_strdup("tester");
    c->username = g_strdup("tester"); c->realname = g_strdup("Tester");
    c->channels = g_strdup(""); c->password = g_strdup("");
    elaro_engine_submit(engine, c);
    guint stage = 0, count = 0;
    gboolean opened = FALSE, joined = FALSE, done = FALSE, chat = FALSE;
    gint64 deadline = g_get_monotonic_time() + 40 * G_TIME_SPAN_SECOND;
    while (!done && g_get_monotonic_time() < deadline) {
        Elaro_Engine_Event *e = elaro_engine_poll(engine);
        if (!e) { g_usleep(1000); continue; }
        if (e->type == ELARO_ENGINE_LINE) {
            if (!strcmp(e->text, "<alice> hello during list")) chat = TRUE;
            g_assert_null(strstr(e->text, "322 tester"));
            g_assert_null(strstr(e->text, "321 tester"));
            g_assert_null(strstr(e->text, "323 tester"));
            if (strstr(e->text, "376 tester")) submit(engine, e->server, ELARO_ENGINE_INPUT, 0, "/list");
            if (!strcmp(e->text, "<mock> cancel-list")) {
                g_assert_cmpuint(stage, ==, 4);
                submit(engine, e->server, ELARO_ENGINE_LIST_CANCEL, 4, NULL);
                stage = 5; submit(engine, e->server, ELARO_ENGINE_LIST, stage, NULL);
            }
        }
        if (e->type == ELARO_ENGINE_LIST_OPEN) {
            opened = TRUE; stage = 1;
            submit(engine, e->server, ELARO_ENGINE_LIST, stage, NULL);
        }
        if (e->type == ELARO_ENGINE_JOINED) { g_assert_cmpstr(e->target, ==, "#Room2"); joined = TRUE; }
        if (e->type == ELARO_ENGINE_LIST_BATCH) {
            g_assert_cmpuint(e->request_id, ==, stage);
            g_assert_true(stage == 1 || stage == 5);
            g_assert_cmpuint(e->channel_count, <=, 128);
            for (guint i = 0; i < e->channel_count; i++) {
                char *expected = g_strdup_printf("#%s%u", count % 2 ? "room" : "Room", count);
                g_assert_cmpstr(e->channels[i].name, ==, expected);
                g_assert_cmpuint(e->channels[i].users, ==, count);
                g_assert_cmpstr(e->channels[i].topic, ==, "Topic <tag> & text");
                g_free(expected); count++;
            }
        }
        if (e->type == ELARO_ENGINE_LIST_END) {
            g_assert_cmpuint(e->request_id, ==, stage);
            if (stage == 1) {
                g_assert_cmpuint(count, ==, 5000);
                submit(engine, e->server, ELARO_ENGINE_INPUT, 0, "/join #Room2");
                count = 0; stage = 2;
                submit(engine, e->server, ELARO_ENGINE_LIST, stage, NULL);
            } else if (stage == 2) {
                g_assert_cmpuint(count, ==, 0); stage = 3;
                submit(engine, e->server, ELARO_ENGINE_LIST, stage, NULL);
            } else {
                g_assert_cmpuint(stage, ==, 5); g_assert_cmpuint(count, ==, 1); done = TRUE;
            }
        }
        if (e->type == ELARO_ENGINE_LIST_ERROR) {
            g_assert_cmpuint(stage, ==, 3);
            g_assert_cmpuint(e->request_id, ==, 3);
            g_assert_cmpstr(e->text, ==, "LIST Please try again later");
            stage = 4; submit(engine, e->server, ELARO_ENGINE_LIST, stage, NULL);
        }
        elaro_engine_event_free(e);
    }
    elaro_engine_stop(engine);
    g_assert_true(opened && joined && done && chat);
    return 0;
}
