#include <glib.h>
#include "src/fe-efl/ui/elaro_ui_private.h"

static Evas_Object *popup;
static unsigned int launches;
static const char *test_url = "https://example.org/a?x=1&y=<two>";

static Evas_Object *capture_popup(Evas_Object *parent)
{
    popup = elm_popup_add(parent);
    return popup;
}
static Ecore_Exe *capture_spawn(const char *command, Ecore_Exe_Flags flags, const void *data)
{
    char *quoted = elaro_command_quote(test_url);
    char *launcher = elaro_executable_find("xdg-open");
    char *executable = elaro_command_quote(launcher);
    char *expected = elaro_string_printf("%s %s", executable, quoted);
    free(executable);
    free(launcher);
    g_assert_cmpstr(command, ==, expected);
    g_assert_cmpint(flags, ==, ECORE_EXE_USE_SH);
    free(quoted);
    free(expected);
    launches++;
    return (Ecore_Exe *)&launches;
}
#define elm_popup_add capture_popup
#define ecore_exe_pipe_run capture_spawn
#include "src/fe-efl/ui/elaro_ui_links.c"
#undef elm_popup_add
#undef ecore_exe_pipe_run

void elaro_ui_image_preview(Elaro_Ui *ui, const char *url) {}
void elaro_ui_music_show(Elaro_Ui *ui, const char *url) {}
Eina_Bool elaro_ui_text_image_url_check(const char *url) { return FALSE; }
Eina_Bool elaro_ui_text_audio_url_check(const char *url) { return FALSE; }

static void click(const char *part)
{
    evas_object_smart_callback_call(elm_object_part_content_get(popup, part), "clicked", NULL);
}
int main(int argc, char **argv)
{
    ecore_main_loop_glib_always_integrate_disable();
    g_assert_cmpint(elm_init(argc, argv), >, 0);
    Elaro_Ui ui = {0};
    ui.win = elm_win_util_standard_add("links-test", "Links test");
    ui.config = elaro_config_load(NULL);
    _external_confirm(&ui, test_url);
    g_assert_cmpuint(launches, ==, 0);
    click("button2");
    g_assert_cmpuint(launches, ==, 0);
    g_assert_false(ui.config->always_allow_external_links);
    _external_confirm(&ui, test_url);
    click("button1");
    g_assert_cmpuint(launches, ==, 1);
    g_assert_false(ui.config->always_allow_external_links);
    _external_confirm(&ui, test_url);
    Evas_Object *box = elm_object_content_get(popup);
    Eina_List *items = elm_box_children_get(box);
    elm_check_state_set(eina_list_nth(items, 2), TRUE);
    eina_list_free(items);
    click("button1");
    g_assert_cmpuint(launches, ==, 2);
    elaro_config_free(ui.config);
    ui.config = elaro_config_load(NULL);
    g_assert_true(ui.config->always_allow_external_links);
    _external_confirm(&ui, test_url);
    g_assert_cmpuint(launches, ==, 3);
    evas_object_del(ui.win);
    elaro_config_free(ui.config);
    elm_shutdown();
    return 0;
}
