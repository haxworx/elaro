#include "src/fe-efl/elaro_util.h"
#include <Ecore.h>
#include <assert.h>
#include <locale.h>

static Eina_Strbuf *output;
static Ecore_Exe *child;
static int exit_status = -1;

static Eina_Bool
_data(void *data, int type, void *event)
{
    Ecore_Exe_Event_Data *received = event;
    if (received->exe == child)
        eina_strbuf_append_length(output, received->data, received->size);
    return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_exited(void *data, int type, void *event)
{
    Ecore_Exe_Event_Del *ended = event;
    if (ended->exe == child) {
        exit_status = ended->exit_code;
        ecore_main_loop_quit();
    }
    return ECORE_CALLBACK_PASS_ON;
}

int main(void)
{
    const char *url = "https://example.org/a'b?x=$(printf bad)&y=`printf bad`;é\nsecond";
    char number[64], *end;
    char *encoded, *decoded, *quoted, *command, *path;
    Ecore_Event_Handler *received, *ended;
    double value;

    encoded = elaro_avatar_hash_get(" MyEmailAddress@example.com ");
    assert(encoded && !strcmp(encoded, "84059b07d4be67b806386c0aad8070a23f18836bbaae342275dc0a83414c32ee"));
    free(encoded);
    encoded = elaro_email_from_realname("A Person <person@example.org>");
    assert(encoded && !strcmp(encoded, "person@example.org"));
    free(encoded);
    assert(!elaro_email_from_realname("A Person"));
    assert(!elaro_email_from_realname("A Person <not an address>"));
    assert(!elaro_email_from_realname("A Person <person@example.org> trailing"));
    assert(!elaro_email_valid("x@example.org\nUSER bad"));
    assert(!elaro_email_valid("x@@example.org"));
    assert(!elaro_avatar_hash_get(" "));

    assert(ecore_init() > 0);
    char *original_config_home = getenv("XDG_CONFIG_HOME") ? strdup(getenv("XDG_CONFIG_HOME")) : NULL;
    setenv("XDG_CONFIG_HOME", "/tmp/custom-config", 1);
    path = elaro_config_dir_get();
    assert(!strcmp(path, "/tmp/custom-config/elaro"));
    free(path);
    setenv("XDG_CONFIG_HOME", "relative-config", 1);
    path = elaro_config_dir_get();
    char *expected = elaro_path_join(eina_environment_home_get(), ".config", "elaro", NULL);
    assert(!strcmp(path, expected));
    free(path);
    unsetenv("XDG_CONFIG_HOME");
    path = elaro_config_dir_get();
    assert(!strcmp(path, expected));
    free(path); free(expected);
    if (original_config_home) setenv("XDG_CONFIG_HOME", original_config_home, 1);
    free(original_config_home);
    encoded = elaro_uri_escape(url);
    decoded = elaro_uri_unescape(encoded);
    assert(decoded && !strcmp(decoded, url));
    assert(strchr(encoded, '\n') == NULL);
    free(encoded);
    free(decoded);
    assert(!elaro_uri_unescape("%00"));
    assert(!elaro_uri_unescape("bad%"));
    assert(!elaro_uri_unescape("%a"));
    assert(!elaro_uri_unescape("%xz"));
    encoded = elaro_uri_escape("");
    assert(encoded && !*encoded);
    free(encoded);
    path = elaro_path_join("/tmp/", "/elaro", "config.eet", NULL);
    assert(!strcmp(path, "/tmp/elaro/config.eet"));
    free(path);

    /* Protocol numbers must stay locale-independent in both directions. */
    setlocale(LC_NUMERIC, "");
    assert(elaro_double_format(number, sizeof(number), 123.768125));
    assert(strchr(number, '.') && !strchr(number, ','));
    value = eina_convert_strtod_c(number, &end);
    assert(!*end && value > 123.76812 && value < 123.76813);

    /* Exercise Ecore's real process path with shell metacharacters and UTF-8. */
    output = eina_strbuf_new();
    received = ecore_event_handler_add(ECORE_EXE_EVENT_DATA, _data, NULL);
    ended = ecore_event_handler_add(ECORE_EXE_EVENT_DEL, _exited, NULL);
    quoted = elaro_command_quote(url);
    command = elaro_string_printf("printf '%%s' %s", quoted);
    child = ecore_exe_pipe_run(command, ECORE_EXE_USE_SH | ECORE_EXE_PIPE_READ, NULL);
    assert(child);
    free(command);
    free(quoted);
    ecore_main_loop_begin();
    assert(exit_status == 0);
    assert(!strcmp(eina_strbuf_string_get(output), url));
    ecore_event_handler_del(received);
    ecore_event_handler_del(ended);
    eina_strbuf_free(output);
    ecore_shutdown();
    return 0;
}
