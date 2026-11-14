#include <src/common.h>
#include <src/core/core.h>
#include <src/core/settings.h>
#include <src/core/signals.h>

void irc_init(void);
void irc_deinit(void);

int main(int argc, char **argv)
{
    char *path;

    core_preinit(argv[0]);
    elaro_gui = ELARO_GUI_EFL;
    core_init();
    irc_init();
    signal_emit("elaro init finished", 0);
    g_assert_cmpstr(settings_get_str("nick"), ==, "elaro");
    g_assert_cmpstr(settings_get_str("user_name"), ==, "elaro");
    g_assert_cmpstr(settings_get_str("real_name"), ==, "elaro user");

    /* Saved engine settings must still override the compiled defaults. */
    path = g_build_filename(g_get_user_config_dir(), "elaro", "config", NULL);
    g_assert_cmpstr(get_elaro_config(), ==, path);
    g_assert_true(g_file_set_contents(path,
        "settings = { core = { nick = \"SavedNick\"; user_name = \"saveduser\"; "
        "real_name = \"Saved Name\"; }; };", -1, NULL));
    g_assert_true(settings_reread(path));
    g_assert_cmpstr(settings_get_str("nick"), ==, "SavedNick");
    g_assert_cmpstr(settings_get_str("user_name"), ==, "saveduser");
    g_assert_cmpstr(settings_get_str("real_name"), ==, "Saved Name");
    g_free(path);
    irc_deinit();
    core_deinit();
    return 0;
}
