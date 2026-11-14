#include <glib.h>
#include "src/engine/elaro_ctcp.h"

static void check(const char *reply, gint64 now, const char *expected)
{
    char *text = elaro_ctcp_reply_format("alice", reply, now);
    g_assert_cmpstr(text, ==, expected);
    g_free(text);
}

int main(void)
{
    gint64 now = G_GINT64_CONSTANT(1700000001500000);
    check("PING 1700000000 250000", now, "CTCP PING reply from alice: 1.250 seconds");
    check("ping 1700000000.25", now, "CTCP PING reply from alice: 1.250 seconds");
    check("PING 1700000000", now, "CTCP PING reply from alice: 1.500 seconds");
    check("PING 1700000001 500000", now, "CTCP PING reply from alice: 0.000 seconds");
    const char *invalid[] = { "PING", "PING nonsense", "PING -1 0", "PING 1700000002 0",
        "PING 1700000001 500001", "PING 1700000000 1000000", "PING 1700000000 -1",
        "PING 1700000000 1 junk", "PING 999999999999999999999999 0", "PING nan", "PING inf" };
    for (guint i = 0; i < G_N_ELEMENTS(invalid); i++)
        check(invalid[i], now, "CTCP PING reply from alice: invalid timestamp");
    check("PING 9223372036854 999999", G_MAXINT64, "CTCP PING reply from alice: invalid timestamp");
    check("VERSION mock-client", now, "CTCP VERSION reply from alice: mock-client");
    check("TIME Thu Nov 13 12:34:56 2026", now, "CTCP TIME reply from alice: Thu Nov 13 12:34:56 2026");
    check("CLIENTINFO PING VERSION TIME", now, "CTCP CLIENTINFO reply from alice: PING VERSION TIME");
    check("VERSION mock\002-client\001hidden", now, "CTCP VERSION reply from alice: mock-client");
    return 0;
}
