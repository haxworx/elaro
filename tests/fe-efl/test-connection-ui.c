#include "src/fe-efl/ui/elaro_ui_private.h"
#include <assert.h>
#include <math.h>
#include <sys/stat.h>
#include <unistd.h>
#include "src/fe-efl/ui/elaro_ui_connection.c"
#include "src/fe-efl/ui/elaro_ui_options.c"

static Elaro_Engine_Command *submitted;
void elaro_engine_submit(Elaro_Engine *engine, Elaro_Engine_Command *command)
{
    assert(!submitted);
    submitted = command;
}

static void edit(Elaro_Ui *ui, Evas_Object *field, const char *text)
{
    _field_set(field, text);
    evas_object_smart_callback_call(field, "changed,user", NULL);
    assert(ui->connection.dirty);
}

static void check_profiles(Elaro_Ui *ui, unsigned int count)
{
    Elaro_Config *stored;
    Eina_Error error = 0;
    assert(!submitted);
    assert(ui->connection.popup);
    assert(eina_list_count(ui->config->connections) == count);
    assert(elm_genlist_items_count(ui->connection.saved) == count);
    stored = elaro_config_load(&error);
    assert(!error);
    assert(eina_list_count(stored->connections) == count);
    elaro_config_free(stored);
}

int main(int argc, char **argv)
{
    Elaro_Ui ui = {0};
    Elaro_Config *selected;
    char *path, *backup;

    ecore_main_loop_glib_always_integrate_disable();
    assert(elm_init(argc, argv) > 0);
    ui.config = elaro_config_load(NULL);
    ui.config->window_x = -320;
    ui.config->window_y = 120;
    ui.config->window_position_set = EINA_TRUE;
    ui.config->buffer_panel_ratio = 0.27;
    ui.config->user_panel_ratio = 0.22;
    ui.win = elm_win_util_standard_add("connection-test", "Connection test");
    /* Scale default logical dimensions once; saved pixel sizes stay exact. */
    double original_scale = elm_config_scale_get();
    elm_config_scale_set(1.5);
    int screen_w = 0, screen_h = 0, width, height;
    elm_win_screen_size_get(ui.win, NULL, NULL, &screen_w, &screen_h);
    elaro_ui_layout_restore(&ui);
    evas_object_geometry_get(ui.win, NULL, NULL, &width, &height);
    assert(width == (screen_w > 0 ? MIN(1920, screen_w * 9 / 10) : 1920));
    assert(height == (screen_h > 0 ? MIN(1350, screen_h * 9 / 10) : 1350));
    assert(!ui.config->window_size_default);
    elaro_ui_layout_restore(&ui);
    int restored_w, restored_h;
    evas_object_geometry_get(ui.win, NULL, NULL, &restored_w, &restored_h);
    assert(restored_w == width && restored_h == height);
    elm_config_scale_set(original_scale);
    ui.history = elm_entry_add(ui.win);
    ui.input = elm_entry_add(ui.win);
    evas_object_resize(ui.win, 1100, 720);
    elaro_ui_connection_show(&ui, NULL, NULL);
    assert(!strcmp(elm_object_text_get(ui.connection.apply), "Save"));
    assert(!ui.connection.selected);
    assert(!elm_genlist_selected_item_get(ui.connection.saved));
    assert(elm_object_disabled_get(ui.connection.connect));
    assert(elm_object_disabled_get(ui.connection.remove));
    {
        assert(!elm_check_state_get(ui.connection.enable_sounds));
        elm_check_state_set(ui.connection.enable_sounds, EINA_TRUE);
        elaro_ui_options_save(&ui);
        assert(ui.config->enable_sounds);
        Elaro_Config *stored = elaro_config_load(NULL);
        assert(stored->enable_sounds);
        elaro_config_free(stored);
    }
    edit(&ui, ui.connection.realname, "A Person");
    edit(&ui, ui.connection.email, "person@example.org");
    elm_check_state_set(ui.connection.auto_reconnect, EINA_TRUE);
    edit(&ui, ui.connection.nick, "SavedNick");
    _save_clicked_cb(&ui, NULL, NULL);
    check_profiles(&ui, 1);
    assert(ui.connection.selected);
    selected = ui.connection.selected;
    assert(ui.config->window_x == -320 && ui.config->window_y == 120);
    assert(ui.config->window_position_set);
    assert(ui.config->buffer_panel_ratio == 0.27 && ui.config->user_panel_ratio == 0.22);
    assert(ui.config->enable_sounds);
    Elaro_Config *sound_config = elaro_config_load(NULL);
    assert(sound_config->enable_sounds);
    elaro_config_free(sound_config);
    assert(selected->auto_reconnect);
    assert(!strcmp(selected->nick, "SavedNick"));
    assert(!strcmp(selected->realname, "A Person"));
    assert(!strcmp(selected->email, "person@example.org"));
    _save_clicked_cb(&ui, NULL, NULL);
    check_profiles(&ui, 1);

    edit(&ui, ui.connection.nick, "UpdatedNick");
    _save_clicked_cb(&ui, NULL, NULL);
    check_profiles(&ui, 1);
    assert(!strcmp(ui.connection.selected->nick, "UpdatedNick"));
    edit(&ui, ui.connection.password, "old-secret");
    edit(&ui, ui.connection.sasl_username, "old-account");
    edit(&ui, ui.connection.sasl_password, "old-sasl-secret");
    elm_check_state_set(ui.connection.sasl, EINA_TRUE);
    _saved_new_cb(&ui, NULL, NULL);
    Evas_Object *cleared[] = { ui.connection.host, ui.connection.nick,
        ui.connection.username, ui.connection.realname, ui.connection.email,
        ui.connection.password, ui.connection.channels,
        ui.connection.sasl_username, ui.connection.sasl_password };
    for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(cleared); i++) {
        char *value = elaro_ui_field_text_get(cleared[i]);
        assert(!*value);
        free(value);
    }
    assert(elm_check_state_get(ui.connection.tls));
    assert(!elm_check_state_get(ui.connection.auto_reconnect));
    assert(!elm_check_state_get(ui.connection.sasl));
    assert(!ui.connection.selected && ui.connection.dirty);
    edit(&ui, ui.connection.host, "irc.example.org");
    edit(&ui, ui.connection.nick, "NewNick");
    edit(&ui, ui.connection.username, "newuser");
    _save_clicked_cb(&ui, NULL, NULL);
    check_profiles(&ui, 2);
    /* Initial selection follows the active server from its row, channel or
     * query; disconnected/unmatched views fall back to the first profile. */
    {
        Elaro_Config *first = eina_list_nth(ui.config->connections, 0);
        Elaro_Config *second = eina_list_nth(ui.config->connections, 1);
        Elaro_Ui_Buffer server = { .server = "display-tag", .target = "",
            .connected = EINA_TRUE, .connection_host = second->host,
            .connection_nick = second->nick, .connection_port = second->port };
        Elaro_Ui_Buffer channel = { .parent = &server, .target = "#chat" };
        Elaro_Ui_Buffer query = { .parent = &server, .target = "someone" };
        Elaro_Ui_Buffer *views[] = { &server, &channel, &query };
        _popup_closed_cb(&ui, NULL, NULL);
        elaro_ui_connection_show(&ui, NULL, NULL);
        assert(ui.connection.selected == first);
        assert(elm_object_item_data_get(elm_genlist_selected_item_get(ui.connection.saved)) == first);
        assert(!elm_object_disabled_get(ui.connection.connect));
        for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(views); i++) {
            _popup_closed_cb(&ui, NULL, NULL);
            ui.current = views[i];
            elaro_ui_connection_show(&ui, NULL, NULL);
            assert(ui.connection.selected == second);
            assert(elm_object_item_data_get(elm_genlist_selected_item_get(ui.connection.saved)) == second);
            char *host = elaro_ui_field_text_get(ui.connection.host);
            assert(!strcmp(host, second->host));
            free(host);
            assert(!ui.connection.dirty);
        }
        _popup_closed_cb(&ui, NULL, NULL);
        server.connection_nick = "ChangedNick";
        elaro_ui_connection_show(&ui, NULL, NULL);
        assert(ui.connection.selected == second);
        _popup_closed_cb(&ui, NULL, NULL);
        server.connected = EINA_FALSE;
        elaro_ui_connection_show(&ui, NULL, NULL);
        assert(ui.connection.selected == first);
        _popup_closed_cb(&ui, NULL, NULL);
        server.connected = EINA_TRUE;
        server.connection_host = "unsaved.example.org";
        elaro_ui_connection_show(&ui, NULL, NULL);
        assert(ui.connection.selected == first);
        ui.current = NULL;
        _popup_closed_cb(&ui, NULL, NULL);
        ui.config->last_connected_host = strdup(second->host);
        ui.config->last_connected_nick = strdup(second->nick);
        ui.config->last_connected_port = second->port;
        elaro_ui_connection_show(&ui, NULL, NULL);
        assert(ui.connection.selected == second);
    }
    _saved_delete_cb(&ui, NULL, NULL);
    check_profiles(&ui, 1);
    _save_clicked_cb(&ui, NULL, NULL);
    check_profiles(&ui, 1);
    assert(!ui.connection.selected);

    /* A failed save must leave the current config and genlist records valid. */
    path = elaro_path_join(getenv("XDG_CONFIG_HOME"), "elaro", "config.eet", NULL);
    backup = elaro_string_printf("%s.backup", path);
    assert(!rename(path, backup));
    assert(!mkdir(path, 0700));
    edit(&ui, ui.connection.nick, "UncommittedNick");
    _save_clicked_cb(&ui, NULL, NULL);
    assert(!submitted);
    assert(eina_list_count(ui.config->connections) == 1);
    assert(elm_genlist_items_count(ui.connection.saved) == 1);
    assert(ui.connection.dirty);
    assert(!rmdir(path));
    assert(!rename(backup, path));
    free(backup);
    free(path);

    elm_genlist_item_selected_set(elm_genlist_first_item_get(ui.connection.saved), EINA_TRUE);
    assert(ui.connection.selected);
    assert(!strcmp(ui.connection.selected->nick, "UpdatedNick"));
    _saved_connect_cb(&ui, NULL, NULL);
    assert(!ui.connection.popup);
    assert(submitted && submitted->type == ELARO_ENGINE_CONNECT);
    assert(submitted->auto_reconnect);
    assert(!strcmp(submitted->nick, "UpdatedNick"));
    assert(!strcmp(submitted->email, "person@example.org"));
    assert(!strcmp(submitted->realname, "A Person"));
    assert(!strcmp(submitted->host, "irc.libera.chat"));
#define FREE(field) free(submitted->field)
    FREE(host); FREE(nick); FREE(username); FREE(realname); FREE(email); FREE(password); FREE(channels);
    FREE(sasl_username); FREE(sasl_password);
#undef FREE
    free(submitted);
    /* Restoring and capturing divider positions uses the two separate panes. */
    ui.buffer_panes = elm_panes_add(ui.win);
    ui.user_panes = elm_panes_add(ui.win);
    elaro_ui_layout_restore(&ui);
    assert(fabs(elm_panes_content_left_size_get(ui.buffer_panes) - 0.27) < 0.000001);
    assert(fabs(elm_panes_content_right_size_get(ui.user_panes) - 0.22) < 0.000001);
    elm_panes_content_left_size_set(ui.buffer_panes, 0.31);
    elm_panes_content_right_size_set(ui.user_panes, 0.24);
    elaro_ui_layout_capture(&ui);
    assert(fabs(ui.config->buffer_panel_ratio - 0.31) < 0.000001);
    assert(fabs(ui.config->user_panel_ratio - 0.24) < 0.000001);
    assert(elaro_config_save(ui.config, NULL));
    Elaro_Config *layout = elaro_config_load(NULL);
    assert(fabs(layout->buffer_panel_ratio - 0.31) < 0.000001);
    assert(fabs(layout->user_panel_ratio - 0.24) < 0.000001);
    assert(layout->window_position_set);
    assert(layout->window_x == ui.config->window_x && layout->window_y == ui.config->window_y);
    elaro_config_free(layout);
    evas_object_del(ui.win);
    elaro_config_free(ui.config);
    elm_shutdown();
    return 0;
}
