#include <glib.h>
#include "src/engine/elaro_engine.h"

static void submit(Elaro_Engine *engine, guint id, Elaro_Engine_Command_Type type, const char *path)
{
    Elaro_Engine_Command *c = g_new0(Elaro_Engine_Command, 1);
    c->type = type; c->transfer_id = id; c->text = g_strdup(path);
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
    guint good = 0, reject = 0, broken = 0, cancel = 0;
    gboolean sent = FALSE, collision = FALSE, progress = FALSE;
    gboolean complete = FALSE, rejected = FALSE, failed = FALSE, cancelled = FALSE, cancelling = FALSE;
    gint64 offers_at = 0, deadline = g_get_monotonic_time() + 15 * G_TIME_SPAN_SECOND;
    while (g_get_monotonic_time() < deadline) {
        if (good && reject && broken && cancel && !sent && g_get_monotonic_time() - offers_at > 400000) {
            submit(engine, good, ELARO_ENGINE_TRANSFER_ACCEPT, argv[2]);
            submit(engine, reject, ELARO_ENGINE_TRANSFER_CANCEL, NULL);
            submit(engine, broken, ELARO_ENGINE_TRANSFER_ACCEPT, argv[4]);
            submit(engine, cancel, ELARO_ENGINE_TRANSFER_ACCEPT, argv[5]);
            sent = TRUE;
        }
        Elaro_Engine_Event *e = elaro_engine_poll(engine);
        if (!e) { g_usleep(1000); continue; }
        if (e->type == ELARO_ENGINE_TRANSFER_OFFER) {
            if (!strcmp(e->text, "good.bin")) good = e->transfer_id;
            if (!strcmp(e->text, "reject.bin")) reject = e->transfer_id;
            if (!strcmp(e->text, "broken.bin")) broken = e->transfer_id;
            if (!strcmp(e->text, "cancel.bin")) cancel = e->transfer_id;
            offers_at = g_get_monotonic_time();
        }
        if (e->type == ELARO_ENGINE_TRANSFER_ERROR && e->transfer_id == good) {
            collision = strstr(e->text, "already exists") != NULL;
            submit(engine, good, ELARO_ENGINE_TRANSFER_ACCEPT, argv[3]);
        }
        if (e->type == ELARO_ENGINE_TRANSFER_UPDATE && e->transfer_id == good && e->transferred > 0) progress = TRUE;
        if (e->type == ELARO_ENGINE_TRANSFER_UPDATE && e->transfer_id == cancel && e->transferred > 0 && !cancelling) {
            submit(engine, cancel, ELARO_ENGINE_TRANSFER_CANCEL, NULL); cancelling = TRUE;
        }
        if (e->type == ELARO_ENGINE_TRANSFER_END) {
            if (e->transfer_id == cancel) cancelled = !strcmp(e->text, "Transfer cancelled");
            if (e->transfer_id == good) complete = !strcmp(e->text, "Transfer complete") && e->transferred == e->size;
            if (e->transfer_id == reject) rejected = !strcmp(e->text, "Transfer cancelled");
            if (e->transfer_id == broken) failed = !strcmp(e->text, "Transfer failed or interrupted");
        }
        elaro_engine_event_free(e);
        if (complete && rejected && failed && cancelled) break;
    }
    elaro_engine_stop(engine);
    if (!(complete && rejected && failed && collision && progress && cancelled))
        g_printerr("complete=%d rejected=%d failed=%d collision=%d progress=%d\n", complete, rejected, failed, collision, progress);
    return complete && rejected && failed && collision && progress && cancelled ? 0 : 1;
}
