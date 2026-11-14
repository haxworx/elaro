#include <glib.h>
#include "src/engine/elaro_engine.h"
int main(int argc, char **argv)
{
    Elaro_Engine *e = elaro_engine_start(argv[0]);
    if (argc == 2 && !strcmp(argv[1], "--stop-immediately")) {
        elaro_engine_stop(e); return 0;
    }
    Elaro_Engine_Command *c = g_new0(Elaro_Engine_Command, 1);
    c->type = ELARO_ENGINE_CONNECT; c->host = g_strdup("127.0.0.1");
    c->username = g_strdup("testuser");
    c->realname = g_strdup("A Person");
    c->email = g_strdup("person@example.org");
    c->port = atoi(argv[1]); c->nick = g_strdup("tester");
    c->password = g_strdup(""); c->channels = g_strdup("#test");
    if (argc > 2 && !strcmp(argv[2], "--sasl")) {
        c->sasl = TRUE;
        c->sasl_username = g_strdup("testaccount"); c->sasl_password = g_strdup("testpass");
    }
    elaro_engine_submit(e, c);
    guint actions = 0, membership = 0, nick_quit = 0;
    guint query_membership = 0, mentions = 0;
    gboolean cannot_send = FALSE, unjoined_cannot_send = FALSE;
    gboolean connection_seen = FALSE, disconnection_seen = FALSE;
    gboolean sent = FALSE, received = FALSE, names = FALSE, acknowledged = FALSE;
    gboolean joined = FALSE, left = FALSE, own_join = FALSE;
    gboolean whois_reply = FALSE, ctcp_reply = FALSE, ping_reply = FALSE, bad_ping = FALSE, time_reply = FALSE;
    guint version_replies = 0, missing_replies = 0;
    gboolean missing_query_sent = FALSE, unrelated_error = FALSE;
    gboolean initial_topic = FALSE, changed_topic = FALSE, cleared_topic = FALSE;
    guint query_opened = 0;
    gboolean closing = FALSE, query_closed = FALSE, room_closed = FALSE, server_notice = FALSE;
    gboolean kick_requested = FALSE, kicked_joined = FALSE, kicked_closed = FALSE;
    gboolean details_received = FALSE;
    gboolean remaining_joined = FALSE, server_query = FALSE, server_closing = FALSE, server_closed = FALSE;
    gint64 deadline = g_get_monotonic_time() + 70 * G_TIME_SPAN_SECOND;
    while (g_get_monotonic_time() < deadline) {
        Elaro_Engine_Event *event = elaro_engine_poll(e);
        if (!event) { g_usleep(1000); continue; }
        if (event->type == ELARO_ENGINE_CONNECTION) {
            g_assert_cmpstr(event->host, ==, "127.0.0.1");
            g_assert_cmpstr(event->nick, ==, "tester");
            g_assert_cmpint(event->port, ==, atoi(argv[1]));
            if (event->connected) connection_seen = TRUE;
            else disconnection_seen = TRUE;
        }
        if (event->type == ELARO_ENGINE_LINE && !strcmp(event->target, "#test") &&
            strstr(event->text, "hello from server")) {
            received = TRUE;
            if (!sent) {
                c = g_new0(Elaro_Engine_Command, 1); c->type = ELARO_ENGINE_INPUT;
                c->server = g_strdup(event->server); c->target = g_strdup("#test");
                c->text = g_strdup("hello from UI"); elaro_engine_submit(e, c);
                c = g_new0(Elaro_Engine_Command, 1); c->type = ELARO_ENGINE_SELECT;
                c->server = g_strdup(event->server); c->target = g_strdup("#test");
                elaro_engine_submit(e, c); sent = TRUE;
                c = g_new0(Elaro_Engine_Command, 1); c->type = ELARO_ENGINE_QUERY;
                c->server = g_strdup(event->server); c->target = g_strdup("alice");
                elaro_engine_submit(e, c);
                c = g_new0(Elaro_Engine_Command, 1); c->type = ELARO_ENGINE_INPUT;
                c->server = g_strdup(event->server); c->target = g_strdup("#test");
                c->text = g_strdup("/msg Missing hello"); elaro_engine_submit(e, c);
                const char *action_targets[] = { "#test", "alice" };
                const char *action_texts[] = { "/me waves back", "/me smiles back" };
                for (guint i = 0; i < 2; i++) {
                    c = g_new0(Elaro_Engine_Command, 1); c->type = ELARO_ENGINE_INPUT;
                    c->server = g_strdup(event->server); c->target = g_strdup(action_targets[i]);
                    c->text = g_strdup(action_texts[i]); elaro_engine_submit(e, c);
                }
                c = g_new0(Elaro_Engine_Command, 1); c->type = ELARO_ENGINE_INPUT;
                c->server = g_strdup(event->server); c->target = g_strdup("#test");
                c->text = g_strdup("blocked message"); elaro_engine_submit(e, c);
                const char *moderation[] = { "/topic -- #test A test topic", "/topic -delete", "/kick zulu Test reason", "/ban *!*@bad.example", "/op alice", "/deop #test zoe", "/voice zulu", "/devoice amber" };
                for (guint i = 0; i < G_N_ELEMENTS(moderation); i++) {
                    c = g_new0(Elaro_Engine_Command, 1); c->type = ELARO_ENGINE_INPUT;
                    c->server = g_strdup(event->server); c->target = g_strdup("#test");
                    c->text = g_strdup(moderation[i]); elaro_engine_submit(e, c);
                }
                const char *queries[] = { "/query alice", "/whois", "/ctcp alice VERSION", "/ping alice" };
                for (guint i = 0; i < G_N_ELEMENTS(queries); i++) {
                    c = g_new0(Elaro_Engine_Command, 1); c->type = ELARO_ENGINE_INPUT;
                    c->server = g_strdup(event->server); c->target = g_strdup("alice");
                    c->text = g_strdup(queries[i]);
                    if (!strcmp(queries[i], "/whois")) c->type = ELARO_ENGINE_USER_INFO;
                    elaro_engine_submit(e, c);
                }
            }
        }
        if (strstr(event->text, "No such nick")) {
            if (strstr(event->text, "missing")) {
                g_assert_cmpstr(event->target, ==, "Missing");
                g_assert_false(event->private_message);
                missing_replies++;
                if (!missing_query_sent) {
                    c = g_new0(Elaro_Engine_Command, 1); c->type = ELARO_ENGINE_INPUT;
                    c->server = g_strdup(event->server); c->target = g_strdup("Missing");
                    c->text = g_strdup("again"); elaro_engine_submit(e, c);
                    missing_query_sent = TRUE;
                }
            } else if (strstr(event->text, "unrelated")) {
                g_assert_cmpstr(event->target, ==, "");
                unrelated_error = TRUE;
            }
        }
        if (event->type == ELARO_ENGINE_LINE && (strstr(event->text, "Cannot send to channel") || strstr(event->text, "You need voice"))) {
            g_assert_false(event->private_message);
            if (strstr(event->text, "#test")) {
                g_assert_true(g_str_has_prefix(event->text, "Cannot send to channel #test"));
                g_assert_cmpstr(event->target, ==, "#test");
                g_assert_nonnull(strstr(event->text, "You need voice (+m)"));
                cannot_send = TRUE;
            } else if (strstr(event->text, "#unjoined")) {
                g_assert_cmpstr(event->target, ==, "");
                unjoined_cannot_send = TRUE;
            }
        }
        if (event->type == ELARO_ENGINE_USER_DETAILS && !strcmp(event->target, "alice")) {
            g_assert_cmpstr(event->text, ==, "Alice <alice@example.org>");
            details_received = TRUE;
        }
        if (event->type == ELARO_ENGINE_TOPIC && !strcmp(event->target, "#test")) {
            if (!strcmp(event->text, "Initial <topic> & text")) initial_topic = TRUE;
            if (!strcmp(event->text, "A test topic")) changed_topic = TRUE;
            if (changed_topic && !*event->text) cleared_topic = TRUE;
        }
        if (event->type == ELARO_ENGINE_LINE && !*event->target && strstr(event->text, "311 tester alice"))
            whois_reply = TRUE;
        if (event->type == ELARO_ENGINE_LINE && !strcmp(event->target, "alice") &&
            strstr(event->text, "CTCP VERSION reply from alice: mock-client")) { ctcp_reply = TRUE; version_replies++; }
        if (event->type == ELARO_ENGINE_LINE) {
            g_assert_null(strchr(event->text, '\001'));
            if (event->mention) {
                g_assert_cmpstr(event->target, ==, "#test");
                g_assert_true(!strcmp(event->text, "<alice> tester: hello") ||
                              !strcmp(event->text, "<alice> hey TESTER!") ||
                              !strcmp(event->text, "* alice greets tester"));
                mentions++;
            }
            if (!strcmp(event->target, "alice") &&
                (!strcmp(event->text, "* alice has joined #test") ||
                 !strcmp(event->text, "* alice has left #test (Taking a break)") ||
                 !strcmp(event->text, "* alice has quit (See you)"))) {
                g_assert_false(event->private_message);
                query_membership++;
            }
            if (!strcmp(event->text, "* alice waves")) {
                g_assert_cmpstr(event->target, ==, "#test");
                g_assert_false(event->private_message); actions++;
            }
            if (!strcmp(event->text, "* alice smiles")) {
                g_assert_cmpstr(event->target, ==, "alice");
                g_assert_true(event->private_message); actions++;
            }
            if (!strcmp(event->text, "* tester waves back") ||
                !strcmp(event->text, "* tester smiles back")) {
                g_assert_false(event->private_message); actions++;
            }
            if (!strcmp(event->text, "* dave is now known as david") ||
                !strcmp(event->text, "* david has quit (Gone fishing)") ||
                !strcmp(event->text, "* eve has quit")) {
                g_assert_cmpstr(event->target, ==, "#test");
                g_assert_false(event->private_message); nick_quit++;
            }
            if (!strcmp(event->text, "* carol has joined #test") ||
                !strcmp(event->text, "* carol has left #test") ||
                !strcmp(event->text, "* bob has left #test (Leaving)")) membership++;
            const char *prefix = "CTCP PING reply from alice: ";
            if (g_str_has_prefix(event->text, prefix)) {
                g_assert_false(event->private_message);
                if (g_str_has_suffix(event->text, "invalid timestamp")) bad_ping = TRUE;
                else {
                    char *end;
                    double seconds = g_ascii_strtod(event->text + strlen(prefix), &end);
                    g_assert_cmpstr(end, ==, " seconds");
                    g_assert_cmpfloat(seconds, >=, 0);
                    /* Irssi timestamps PING before its flood-protected send queue. */
                    g_assert_cmpfloat(seconds, <, 70);
                    ping_reply = TRUE;
                }
            }
            if (!strcmp(event->text, "CTCP TIME reply from alice: Thu Nov 13 12:34:56 2026")) time_reply = TRUE;
        }
        if (event->type == ELARO_ENGINE_LINE && strstr(event->text, "MODE #test +b *!*@bad.example")) {
            c = g_new0(Elaro_Engine_Command, 1); c->type = ELARO_ENGINE_INPUT;
            c->server = g_strdup(event->server); c->target = g_strdup("#test");
            c->text = g_strdup("/unban *!*@bad.example"); elaro_engine_submit(e, c);
        }
        if (event->type == ELARO_ENGINE_QUERY_OPENED && !strcmp(event->target, "alice")) query_opened++;
        if (event->type == ELARO_ENGINE_QUERY_OPENED && !strcmp(event->target, "bob")) server_query = TRUE;
        if (event->type == ELARO_ENGINE_JOINED) {
            /* Another user's JOIN must not steal the selected channel. */
            if (!strcmp(event->target, "#kicked")) kicked_joined = TRUE;
            else if (!strcmp(event->target, "#remaining")) {
                remaining_joined = TRUE;
                c = g_new0(Elaro_Engine_Command, 1); c->type = ELARO_ENGINE_QUERY;
                c->server = g_strdup(event->server); c->target = g_strdup("bob");
                elaro_engine_submit(e, c);
            }
            else {
                if (own_join || strcmp(event->target, "#test")) return 1;
                own_join = TRUE;
            }
        }
        if (strstr(event->text, "UI message received")) acknowledged = TRUE;
        if (event->type == ELARO_ENGINE_NICKS) {
            if (!strcmp(event->text, "@tester\n@zoe\n+amber\n+zelda\nalice\nzulu\n")) {
                names = TRUE;
                if (joined) left = TRUE;
            }
            if (!strcmp(event->text, "@tester\n@zoe\n+amber\n+bob\n+zelda\nalice\nzulu\n")) joined = TRUE;
        }
        /* Once closed, queued channel traffic must not resurrect the view. */
        if (room_closed && !strcmp(event->target, "#test") &&
            (event->type == ELARO_ENGINE_LINE || event->type == ELARO_ENGINE_JOINED))
            g_error("Event for a channel after leaving: %s", event->text);
        if (event->type == ELARO_ENGINE_BUFFER_CLOSED) {
            if (!strcmp(event->target, "alice")) query_closed = TRUE;
            if (!strcmp(event->target, "#test")) room_closed = TRUE;
            if (!strcmp(event->target, "#kicked")) kicked_closed = TRUE;
            if (!*event->target) server_closed = TRUE;
        }
        if (event->type == ELARO_ENGINE_LINE && !*event->target && strstr(event->text, "server status message"))
            server_notice = TRUE;
        if (actions == 4 && membership == 3 && received && names && acknowledged && joined && left && own_join && missing_replies == 2 && unrelated_error && whois_reply && ctcp_reply && ping_reply && bad_ping && time_reply && query_opened == 2 && cannot_send && unjoined_cannot_send && !closing) {
            c = g_new0(Elaro_Engine_Command, 1); c->type = ELARO_ENGINE_INPUT;
            c->server = g_strdup(event->server); c->target = g_strdup("#test");
            c->text = g_strdup("/nick renamed"); elaro_engine_submit(e, c);
            const char *targets[] = { "alice", "#test" };
            for (guint i = 0; i < G_N_ELEMENTS(targets); i++) {
                c = g_new0(Elaro_Engine_Command, 1); c->type = ELARO_ENGINE_CLOSE;
                c->server = g_strdup(event->server); c->target = g_strdup(targets[i]);
                elaro_engine_submit(e, c);
            }
            closing = TRUE;
        }
        if (closing && query_closed && room_closed && server_notice && !kick_requested) {
            c = g_new0(Elaro_Engine_Command, 1); c->type = ELARO_ENGINE_INPUT;
            c->server = g_strdup(event->server); c->target = g_strdup("");
            c->text = g_strdup("/join #kicked");
            elaro_engine_submit(e, c);
            kick_requested = TRUE;
        }
        if (kicked_joined && kicked_closed && remaining_joined && server_query && !server_closing) {
            c = g_new0(Elaro_Engine_Command, 1); c->type = ELARO_ENGINE_CLOSE;
            c->server = g_strdup(event->server); c->target = g_strdup("");
            elaro_engine_submit(e, c);
            server_closing = TRUE;
        }
        elaro_engine_event_free(event);
        if (server_closed) break;
    }
    elaro_engine_stop(e);
    g_assert_true(connection_seen);
    g_assert_true(disconnection_seen);
    g_assert_true(cannot_send);
    g_assert_true(unjoined_cannot_send);
    g_assert_cmpuint(version_replies, ==, 1);
    g_assert_cmpuint(nick_quit, ==, 3);
    g_assert_cmpuint(actions, ==, 4);
    g_assert_cmpuint(membership, ==, 3);
    g_assert_cmpuint(query_membership, ==, 4);
    g_assert_cmpuint(mentions, ==, 3);
    return initial_topic && changed_topic && cleared_topic && received && names && acknowledged && joined && left && own_join && missing_replies == 2 && unrelated_error && whois_reply && ctcp_reply && ping_reply && bad_ping && time_reply && query_opened == 2 && query_closed && room_closed && server_notice && kicked_joined && kicked_closed && remaining_joined && server_query && server_closed && details_received ? 0 : 1;
}
