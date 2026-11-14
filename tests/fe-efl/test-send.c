#include <glib.h>
#include "src/engine/elaro_engine.h"

static void send_file(Elaro_Engine *engine, const char *server, guint id, const char *path)
{
    Elaro_Engine_Command *c = g_new0(Elaro_Engine_Command, 1);
    c->type = ELARO_ENGINE_TRANSFER_SEND; c->transfer_id = id;
    c->server = g_strdup(server); c->target = g_strdup("alice"); c->text = g_strdup(path);
    elaro_engine_submit(engine, c);
}
int main(int argc, char **argv)
{
    Elaro_Engine *engine = elaro_engine_start(argv[0]);
    Elaro_Engine_Command *c = g_new0(Elaro_Engine_Command, 1);
    c->type = ELARO_ENGINE_CONNECT; c->host = g_strdup("127.0.0.1");
    c->port = atoi(argv[1]); c->nick = g_strdup("tester");
    c->password = g_strdup(""); c->channels = g_strdup("");
    elaro_engine_submit(engine, c);
    gboolean sent = FALSE, invalid = FALSE, progress = FALSE, complete = FALSE, cancelled = FALSE;
    char *server = NULL;
    gint64 deadline = g_get_monotonic_time() + 15 * G_TIME_SPAN_SECOND;
    while (g_get_monotonic_time() < deadline) {
        Elaro_Engine_Event *e = elaro_engine_poll(engine);
        if (!e) { g_usleep(1000); continue; }
        if (!sent && e->type == ELARO_ENGINE_LINE && strstr(e->text, "Ready to send")) {
            server = g_strdup(e->server);
            send_file(engine, server, 0x80000001u, "/nonexistent/elaro-test-file");
            sent = TRUE;
        }
        if (e->type == ELARO_ENGINE_TRANSFER_ERROR && e->transfer_id == 0x80000001u) {
            invalid = TRUE; send_file(engine, server, 0x80000001u, argv[2]);
        }
        if (e->type == ELARO_ENGINE_TRANSFER_UPDATE && e->transfer_id == 0x80000001u && e->transferred > 0) progress = TRUE;
        if (e->type == ELARO_ENGINE_TRANSFER_END && e->transfer_id == 0x80000001u) {
            complete = !strcmp(e->text, "Transfer complete") && e->transferred == e->size;
            send_file(engine, server, 0x80000002u, argv[3]);
        }
        if (e->type == ELARO_ENGINE_TRANSFER_UPDATE && e->transfer_id == 0x80000002u) {
            c = g_new0(Elaro_Engine_Command, 1); c->type = ELARO_ENGINE_TRANSFER_CANCEL;
            c->transfer_id = e->transfer_id; elaro_engine_submit(engine, c);
        }
        if (e->type == ELARO_ENGINE_TRANSFER_END && e->transfer_id == 0x80000002u)
            cancelled = !strcmp(e->text, "Transfer cancelled");
        elaro_engine_event_free(e);
        if (complete && cancelled) break;
    }
    elaro_engine_stop(engine); g_free(server);
    if (!(invalid && progress && complete && cancelled))
        g_printerr("invalid=%d progress=%d complete=%d cancelled=%d\n", invalid, progress, complete, cancelled);
    return invalid && progress && complete && cancelled ? 0 : 1;
}
