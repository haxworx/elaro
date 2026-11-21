#include "src/fe-efl/ui/elaro_ui_private.h"
#include <assert.h>
#include <dlfcn.h>

/* Observe clipboard text while retaining the real selection ownership path. */
static char *clipboard_text;
Eina_Bool elm_cnp_selection_set(Evas_Object *obj, Elm_Sel_Type selection,
                               Elm_Sel_Format format, const void *buf, size_t length)
{
    if (selection == ELM_SEL_TYPE_CLIPBOARD && format == ELM_SEL_FORMAT_TEXT) {
        free(clipboard_text);
        clipboard_text = strndup(buf, length);
    }
    Eina_Bool (*native_set)(Evas_Object *, Elm_Sel_Type, Elm_Sel_Format, const void *, size_t);
    native_set = dlsym(RTLD_NEXT, "elm_cnp_selection_set");
    assert(native_set);
    return native_set(obj, selection, format, buf, length);
}

static Evas_Object *find_hoversel(Eo *obj)
{
    Eo *child;
    Evas_Object *found = NULL;
    if (!strcmp(efl_class_name_get(efl_class_get(obj)), "Elm.Hoversel")) return obj;
    Eina_Iterator *children = efl_children_iterator_new(obj);
    EINA_ITERATOR_FOREACH(children, child) {
        found = find_hoversel(child);
        if (found) break;
    }
    eina_iterator_free(children);
    return found;
}

static Evas_Object *find_text_button(Eo *obj, const char *label)
{
    Eo *child;
    Evas_Object *found = NULL;
    if (strstr(efl_class_name_get(efl_class_get(obj)), "Button")) {
        const char *text = elm_object_text_get(obj);
        if (text && !strcmp(text, label)) return obj;
    }
    Eina_Iterator *children = efl_children_iterator_new(obj);
    EINA_ITERATOR_FOREACH(children, child) {
        found = find_text_button(child, label);
        if (found) break;
    }
    eina_iterator_free(children);
    return found;
}

static unsigned int notifications, sounds;
void elaro_ui_notify_sound(Elaro_Ui *ui) { sounds++; }
static unsigned int freed_users;
static void (*user_free)(void *, Evas_Object *);
static void count_user_free(void *data, Evas_Object *obj)
{
    freed_users++;
    user_free(data, obj);
}
void elaro_ui_notify_private(const char *server, const char *nick, const char *message)
{
    notifications++;
}
static unsigned int submitted;
static Eina_Bool server_close_submitted;
static char *listed_server;
void elaro_engine_submit(Elaro_Engine *engine, Elaro_Engine_Command *command)
{
    submitted++;
    if (command->type == ELARO_ENGINE_INPUT && command->text && !strcmp(command->text, "/list")) {
        free(listed_server); listed_server = strdup(command->server);
        assert(!*command->target);
    }
    if (command->type == ELARO_ENGINE_CLOSE && !strcmp(command->server, "test")
        && !*command->target) server_close_submitted = EINA_TRUE;
    free(command->server);
    free(command->target);
    free(command->text);
    free(command);
}
void elaro_ui_links_reset(Elaro_Ui *ui) {}
void elaro_ui_dcc_send_show(Elaro_Ui *ui, const char *server, const char *nick) {}

static void update_users(Elaro_Ui *ui, const char *snapshot)
{
    char **lines = eina_str_split(snapshot, "\n", 0);
    unsigned int count = 0;
    for (char **line = lines; line && *line; line++) if (**line) count++;
    Elaro_Engine_User *users = calloc(count, sizeof(*users));
    unsigned int i = 0;
    for (char **line = lines; line && *line; line++) {
        char *nick = *line;
        if (!*nick) continue;
        users[i].prefix = (*nick == '@' || *nick == '+') ? *nick++ : 0;
        users[i++].nick = nick;
    }
    elaro_ui_users_update(ui, users, count);
    free(users);
    if (lines) free(lines[0]);
    free(lines);
}

static void check_users(Elaro_Ui *ui, const char **expected, unsigned int count)
{
    Elm_Object_Item *item = elm_genlist_first_item_get(ui->nicks);
    unsigned int i;
    for (i = 0; i < count; i++) {
        assert(item);
        char *nick = ui->user_class->func.text_get(elm_object_item_data_get(item), ui->nicks, "elm.text");
        assert(!strcmp(nick, expected[i]));
        free(nick);
        item = elm_genlist_item_next_get(item);
    }
    assert(!item);
    assert(eina_hash_population(ui->users) == count);
    if (ui->current && ui->current->channel) {
        char *expected_count = elaro_string_printf("(%u %s)", count, count == 1 ? "user" : "users");
        assert(strstr(elm_object_text_get(ui->title), expected_count));
        free(expected_count);
    }
}

static void complete(Elaro_Ui *ui, const char *expected)
{
    Evas_Event_Key_Down ev = { .key = "Tab" };
    char *text;
    ev.modifiers = (Evas_Modifier *)evas_key_modifier_get(evas_object_evas_get(ui->input));
    assert(elaro_ui_users_complete(ui, ui->input, &ev));
    text = elm_entry_markup_to_utf8(elm_entry_entry_get(ui->input));
    assert(!strcmp(text, expected));
    free(text);
}

static void input_set(Elaro_Ui *ui, const char *text)
{
    char *markup = elm_entry_utf8_to_markup(text);
    elm_entry_entry_set(ui->input, markup);
    elm_entry_cursor_end_set(ui->input);
    free(markup);
}

static void recall(Elaro_Ui *ui, const char *key, const char *expected)
{
    Evas_Event_Key_Down ev = { .key = (char *)key };
    char *text;
    ev.modifiers = (Evas_Modifier *)evas_key_modifier_get(evas_object_evas_get(ui->input));
    assert(elaro_ui_input_key_down(ui, ui->input, &ev));
    text = elm_entry_markup_to_utf8(elm_entry_entry_get(ui->input));
    assert(!strcmp(text, expected));
    free(text);
}

static Eina_Bool right_click_server(void *data)
{
    Elaro_Ui_Buffer *server = data;
    Evas_Object *row = elm_object_item_track(server->item);
    Evas *canvas = evas_object_evas_get(server->app->buffers);
    Evas_Coord x, y, w, h;
    evas_object_geometry_get(row, &x, &y, &w, &h);
    assert(w > 0 && h > 0);
    evas_event_feed_mouse_in(canvas, 0, NULL);
    evas_event_feed_mouse_move(canvas, x + w / 2, y + h / 2, 0, NULL);
    evas_event_feed_mouse_down(canvas, 3, EVAS_BUTTON_NONE, 0, NULL);
    evas_event_feed_mouse_up(canvas, 3, EVAS_BUTTON_NONE, 0, NULL);
    elm_object_item_untrack(server->item);
    assert(server->app->buffer_menu);
    ecore_main_loop_quit();
    return ECORE_CALLBACK_CANCEL;
}

static void history_loaded(void *data, Evas_Object *obj, void *info)
{
    *(Eina_Bool *)data = EINA_TRUE;
}

static Eina_Bool layout_done(void *data)
{
    ecore_main_loop_quit();
    return ECORE_CALLBACK_CANCEL;
}

static void settle_layout(void)
{
    ecore_timer_add(0.1, layout_done, NULL);
    ecore_main_loop_begin();
}

static void click_close_icon(Elaro_Ui_Buffer *buffer)
{
    elm_genlist_item_show(buffer->item, ELM_GENLIST_ITEM_SCROLLTO_IN);
    settle_layout();
    Evas_Object *button = elm_object_item_part_content_get(buffer->item, "elm.swallow.end");
    assert(button);
    Evas_Coord x, y, w, h;
    Evas *canvas = evas_object_evas_get(button);
    evas_object_geometry_get(button, &x, &y, &w, &h);
    assert(w > 0 && h > 0);
    evas_event_feed_mouse_in(canvas, 0, NULL);
    evas_event_feed_mouse_move(canvas, x + w / 2, y + h / 2, 0, NULL);
    evas_event_feed_mouse_down(canvas, 1, EVAS_BUTTON_NONE, 0, NULL);
    evas_event_feed_mouse_up(canvas, 1, EVAS_BUTTON_NONE, 0, NULL);
    settle_layout();
}

static unsigned int visible_user(Elaro_Ui *ui)
{
    Evas_Coord x, y, w;
    evas_object_geometry_get(ui->nicks, &x, &y, &w, NULL);
    Elm_Object_Item *item = elm_genlist_at_xy_item_get(ui->nicks, x + w / 2, y + 1, NULL);
    assert(item);
    return elm_genlist_item_index_get(item);
}

int main(int argc, char **argv)
{
    Elaro_Ui ui = {0};
    Elaro_Ui_Buffer *channel, *query, *next;
    Evas_Object *focused;
    Elaro_Engine_Event event = { .type = ELARO_ENGINE_JOINED, .server = "test", .target = "#test" };
    const char *first[] = { "Alice", "Zed", "Bob", "Aaron", "Zoe" };
    const char *second[] = { "Bob", "Alice", "Aaron", "New" };

    ecore_main_loop_glib_always_integrate_disable();
    assert(elm_init(argc, argv) > 0);
    /* Match desktop startup even when the user's profile defaults to touch
     * interaction, which disables drag selection in read-only entries. */
    elm_config_desktop_entry_set(EINA_TRUE);
    ui.win = elm_win_util_standard_add("state-test", "State test");
    ui.buffers = elm_genlist_add(ui.win);
    ui.nicks = elm_genlist_add(ui.win);
    ui.user_panes = elm_panes_add(ui.win);
    elm_object_part_content_set(ui.user_panes, "right", ui.nicks);
    elm_panes_content_right_size_set(ui.user_panes, 0.25);
    ui.history = elaro_ui_history_add(ui.win);
    ui.input = elm_entry_add(ui.win);
    ui.topic = elm_entry_add(ui.win);
    ui.title = elm_label_add(ui.win);
    evas_object_data_set(ui.buffers, "app", &ui);
    elaro_ui_buffers_init(&ui);
    elaro_ui_users_init(&ui);
    elaro_ui_buffer_get(&ui, "Status", "");
    elaro_ui_buffer_get(&ui, "test", "");
    elaro_ui_buffer_event(&ui, &event);
    channel = ui.current;
    assert(channel && channel->channel && !strcmp(channel->target, "#test"));
    {
        Elaro_Engine_Event state = { .type = ELARO_ENGINE_CONNECTION,
            .server = "test", .target = "", .text = "", .host = "irc.example.org",
            .nick = "SavedNick", .port = 6697, .connected = true };
        Elaro_Ui_Buffer *server = channel->parent;
        elaro_ui_buffer_event(&ui, &state);
        assert(server->connected && ui.current == channel);
        assert(!strcmp(server->connection_host, "irc.example.org"));
        assert(!strcmp(server->connection_nick, "SavedNick"));
        assert(server->connection_port == 6697);
        state.connected = false;
        elaro_ui_buffer_event(&ui, &state);
        assert(!server->connected && ui.current == channel);
        Elaro_Engine_Event disconnect = { .type = ELARO_ENGINE_LINE,
            .server = "test", .target = "", .text = "Disconnected", .disconnected = true };
        elaro_ui_buffer_event(&ui, &disconnect);
        assert(strstr(eina_strbuf_string_get(server->history), "Disconnected"));
        assert(strstr(eina_strbuf_string_get(channel->history), "Disconnected"));
        assert(ui.current == channel);
    }


    elaro_ui_input_record(&ui, "hello <world> & friends");
    elaro_ui_input_record(&ui, "/me waves");
    input_set(&ui, "unfinished draft");
    recall(&ui, "Up", "/me waves");
    recall(&ui, "Up", "hello <world> & friends");
    recall(&ui, "Up", "hello <world> & friends");
    recall(&ui, "Down", "/me waves");
    recall(&ui, "Down", "unfinished draft");
    recall(&ui, "Down", "unfinished draft");
    query = elaro_ui_buffer_get(&ui, "other-server", "#test");
    elm_genlist_item_selected_set(query->item, EINA_TRUE);
    recall(&ui, "Up", "");
    elaro_ui_input_record(&ui, "other server message");
    input_set(&ui, "");
    recall(&ui, "Up", "other server message");
    elm_genlist_item_selected_set(channel->item, EINA_TRUE);
    recall(&ui, "Down", "unfinished draft");
    {
        Elaro_Ui_Buffer *private = elaro_ui_buffer_get(&ui, "test", "panel-query");
        ecore_timer_add(0.25, layout_done, NULL);
        ecore_main_loop_begin();
        double width = elm_panes_content_right_size_get(ui.user_panes);
        elaro_ui_buffer_select(private);
        assert(ui.user_panel_animator);
        ecore_timer_add(0.25, layout_done, NULL);
        ecore_main_loop_begin();
        assert(!ui.user_panel_animator);
        assert(ui.user_panel_collapsed);
        assert(elm_panes_content_right_size_get(ui.user_panes) == 0.0);
        assert(!elm_object_part_content_get(ui.user_panes, "right"));
        elaro_ui_buffer_select(query);
        assert(ui.user_panel_collapsed); /* Query to query retains the saved width. */
        elaro_ui_buffer_select(channel->parent);
        assert(ui.current == channel->parent && ui.user_panel_collapsed);
        assert(elm_panes_content_right_size_get(ui.user_panes) == 0.0);
        elaro_ui_buffer_select(elaro_ui_buffer_get(&ui, "Status", ""));
        assert(ui.user_panel_collapsed);
        assert(elm_panes_content_right_size_get(ui.user_panes) == 0.0);
        elaro_ui_buffer_select(channel);
        assert(ui.user_panel_animator);
        ecore_timer_add(0.25, layout_done, NULL);
        ecore_main_loop_begin();
        assert(!ui.user_panel_animator);
        assert(!ui.user_panel_collapsed);
        assert(elm_object_part_content_get(ui.user_panes, "right") == ui.nicks);
        assert(elm_panes_content_right_size_get(ui.user_panes) == width);
        Elaro_Engine_Event closed = { .type = ELARO_ENGINE_BUFFER_CLOSED,
            .server = "test", .target = "panel-query" };
        elaro_ui_buffer_event(&ui, &closed);
    }
    recall(&ui, "Up", "/me waves");
    elaro_ui_input_record(&ui, "edited and sent");
    input_set(&ui, "");
    recall(&ui, "Up", "edited and sent");
    recall(&ui, "Down", "");
    event.type = ELARO_ENGINE_BUFFER_CLOSED;
    event.server = "other-server";
    event.target = "";
    elaro_ui_buffer_event(&ui, &event);
    event.server = "test";
    event.target = "#test";
    event.type = ELARO_ENGINE_JOINED;

    update_users(&ui, "Zoe\n@Zed\nAaron\n+Bob\n@Alice\n");
    check_users(&ui, first, EINA_C_ARRAY_LENGTH(first));
    update_users(&ui, "@Bob\n+Alice\nAaron\nNew\n");
    check_users(&ui, second, EINA_C_ARRAY_LENGTH(second));
    /* Exercise the real key hook as well as completion/cycling below. */
    elm_entry_key_down_intercept_set(ui.input, elaro_ui_input_key_down, &ui);
    evas_object_show(ui.input);
    evas_object_show(ui.win);
    elm_object_focus_set(ui.input, EINA_TRUE);
    input_set(&ui, "a");
    focused = evas_focus_get(evas_object_evas_get(ui.input));
    assert(focused);
    evas_event_feed_key_down(evas_object_evas_get(ui.input), "Tab", "Tab", "\t", NULL, 0, NULL);
    evas_event_feed_key_up(evas_object_evas_get(ui.input), "Tab", "Tab", "\t", NULL, 0, NULL);
    {
        char *text = elm_entry_markup_to_utf8(elm_entry_entry_get(ui.input));
        assert(!strcmp(text, "Aaron: "));
        assert(evas_focus_get(evas_object_evas_get(ui.input)) == focused);
        free(text);
    }
    input_set(&ui, "a");
    complete(&ui, "Aaron: ");
    complete(&ui, "Alice: ");
    complete(&ui, "Aaron: ");
    input_set(&ui, "hello AL please");
    elm_entry_cursor_pos_set(ui.input, 8);
    complete(&ui, "hello Alice please");
    input_set(&ui, "héllo Bo");
    complete(&ui, "héllo Bob");
    input_set(&ui, "no-match");
    complete(&ui, "no-match");
    input_set(&ui, "");
    complete(&ui, "");
    update_users(&ui, "Alice\nAlfred\n");
    input_set(&ui, "al");
    complete(&ui, "Alfred: ");
    complete(&ui, "Alice: ");
    update_users(&ui, "Alice\n");
    input_set(&ui, "al");
    complete(&ui, "Alice: ");
    update_users(&ui, "");
    check_users(&ui, NULL, 0);

    /* Each channel retains its viewport across chats and delayed snapshots. */
    {
        Elaro_Ui_Buffer *other = elaro_ui_buffer_get(&ui, "test", "#scroll");
        Elaro_Ui_Buffer *chat = elaro_ui_buffer_get(&ui, "test", "scroll-query");
        Eina_Strbuf *names = eina_strbuf_new();
        unsigned int saved;
        other->channel = EINA_TRUE;
        for (unsigned int i = 0; i < 200; i++)
            eina_strbuf_append_printf(names, "user%03u\n", i);
        elaro_ui_expand(ui.nicks);
        elm_win_resize_object_add(ui.win, ui.nicks);
        evas_object_resize(ui.win, 240, 300);
        evas_object_show(ui.nicks);
        update_users(&ui, eina_strbuf_string_get(names));
        settle_layout();
        Elm_Object_Item *row = elm_genlist_first_item_get(ui.nicks);
        for (unsigned int i = 0; i < 80; i++) row = elm_genlist_item_next_get(row);
        elm_genlist_item_show(row, ELM_GENLIST_ITEM_SCROLLTO_TOP);
        settle_layout();
        saved = visible_user(&ui);
        assert(saved > 50);
        elaro_ui_buffer_select(other);
        update_users(&ui, eina_strbuf_string_get(names));
        settle_layout();
        assert(visible_user(&ui) == 1);
        assert(channel->user_scroll_index == saved);
        elaro_ui_buffer_select(channel);
        /* Switch again before receiving the requested snapshot. */
        elaro_ui_buffer_select(chat);
        assert(channel->user_scroll_index == saved);
        elaro_ui_buffer_select(channel);
        update_users(&ui, eina_strbuf_string_get(names));
        settle_layout();
        assert(visible_user(&ui) == saved);
        elaro_ui_buffer_select(other);
        update_users(&ui, eina_strbuf_string_get(names));
        settle_layout();
        assert(visible_user(&ui) == 1);
        elaro_ui_buffer_select(channel);
        update_users(&ui, "Alice\nBob\n");
        settle_layout();
        assert(visible_user(&ui) == 1);
        update_users(&ui, "");
        elm_win_resize_object_del(ui.win, ui.nicks);
        evas_object_hide(ui.nicks);
        eina_strbuf_free(names);
        Elaro_Engine_Event close = { .type = ELARO_ENGINE_BUFFER_CLOSED,
            .server = "test", .target = "#scroll" };
        elaro_ui_buffer_event(&ui, &close);
        close.target = "scroll-query";
        elaro_ui_buffer_event(&ui, &close);
    }

    /* Inline prefix cells align bodies independently of each nickname's glyph
     * widths. Wrapping, live appends, font changes and copying keep working. */
    {
        Elaro_Ui_Buffer *columns = elaro_ui_buffer_get(&ui, "test", "column-query");
        Elaro_Engine_Event line = { .type = ELARO_ENGINE_LINE, .server = "test",
            .target = "column-query", .text = "<iii> First message with enough words to wrap across several lines while keeping the message body aligned with the next speaker." };
        elaro_ui_buffer_event(&ui, &line);
        elaro_ui_buffer_select(columns);
        elm_win_resize_object_add(ui.win, ui.history);
        evas_object_show(ui.history);
        for (int pass = 0; pass < 3; pass++) {
            evas_object_resize(ui.win, pass == 1 ? 480 : 700, 300);
            elm_config_font_overlay_set("entry", pass == 1 ? "Serif" : "Sans", pass == 2 ? 18 : 14);
            elm_config_font_overlay_apply();
            settle_layout();
            if (pass == 0) {
                int short_width, long_width;
                const char *cell = strstr(elm_entry_entry_get(ui.history), "<item ");
                assert(cell);
                cell = strstr(cell + 1, "<item ");
                assert(cell && sscanf(cell, "<item absize=%d", &short_width) == 1);
                line.text = "<WWWWWWWWWWWWWWW> Second message https://example.org/"
                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
                elaro_ui_buffer_event(&ui, &line);
                line.text = "<WWWWWWWWWWWWWWW> Same speaker again";
                elaro_ui_buffer_event(&ui, &line);
                line.text = "<iii> Author rotated back";
                elaro_ui_buffer_event(&ui, &line);
                line.text = "<iii> Another consecutive message";
                elaro_ui_buffer_event(&ui, &line);
                line.text = "* A system notice breaks the group";
                elaro_ui_buffer_event(&ui, &line);
                line.text = "<iii> Visible after the notice";
                elaro_ui_buffer_event(&ui, &line);
                settle_layout();
                cell = strstr(elm_entry_entry_get(ui.history), "<item ");
                assert(cell);
                cell = strstr(cell + 1, "<item ");
                assert(cell && sscanf(cell, "<item absize=%d", &long_width) == 1);
                assert(long_width > short_width);
            }
            /* Exactly the two consecutive messages hide their nickname;
             * rotation and a system notice restore it, also after reflow. */
            const char *rendered = elm_entry_entry_get(ui.history);
            unsigned int repeats = 0;
            for (const char *at = rendered; (at = strstr(at, ":repeat>")); at++) repeats++;
            assert(repeats == 2);
            char *text = elm_entry_markup_to_utf8(rendered);
            char *first = strstr(text, "First"), *second = strstr(text, "Second");
            assert(first && second);
            Evas_Object *tb = elm_entry_textblock_get(ui.history);
            Evas_Textblock_Cursor *cur = evas_object_textblock_cursor_new(tb);
            Evas_Coord first_x, second_x, wrap_x;
            char *prefix = strndup(text, first - text);
            evas_textblock_cursor_pos_set(cur, eina_unicode_utf8_get_len(prefix));
            free(prefix);
            int first_line = evas_textblock_cursor_pen_geometry_get(cur, &first_x, NULL, NULL, NULL);
            prefix = strndup(text, second - text);
            evas_textblock_cursor_pos_set(cur, eina_unicode_utf8_get_len(prefix));
            free(prefix);
            int second_line = evas_textblock_cursor_pen_geometry_get(cur, &second_x, NULL, NULL, NULL);
            assert(second_line > first_line + 1);
            assert(evas_textblock_cursor_line_set(cur, first_line + 1));
            evas_textblock_cursor_line_char_first(cur);
            evas_textblock_cursor_pen_geometry_get(cur, &wrap_x, NULL, NULL, NULL);
            assert(abs(first_x - second_x) <= 1);
            assert(abs(first_x - wrap_x) <= 1);
            assert(strstr(elm_entry_entry_get(ui.history), "href=url:"));
            /* An unbroken URL wraps, stays clickable, and never needs a
             * horizontal scroll beyond the message column. */
            char *url = strstr(text, "https://example.org/");
            assert(url);
            prefix = strndup(text, url - text);
            int url_pos = eina_unicode_utf8_get_len(prefix);
            free(prefix);
            evas_textblock_cursor_pos_set(cur, url_pos);
            int url_line = evas_textblock_cursor_pen_geometry_get(cur, NULL, NULL, NULL, NULL);
            evas_textblock_cursor_pos_set(cur, url_pos + 140);
            assert(evas_textblock_cursor_pen_geometry_get(cur, NULL, NULL, NULL, NULL) > url_line);
            Evas_Coord viewport_w, content_w;
            elm_scroller_region_get(ui.history, NULL, NULL, &viewport_w, NULL);
            elm_scroller_child_size_get(ui.history, &content_w, NULL);
            assert(content_w <= viewport_w + ELM_SCALE_SIZE(4));
            evas_textblock_cursor_free(cur);
            free(text);
            elm_entry_select_all(ui.history);
            elm_entry_selection_copy(ui.history);
            settle_layout();
            assert(clipboard_text && strstr(clipboard_text, "] <iii> First message"));
            assert(strstr(clipboard_text, "] <WWWWWWWWWWWWWWW> Second message https://example.org/"));
            assert(strstr(clipboard_text, "] <WWWWWWWWWWWWWWW> Same speaker again"));
            assert(strstr(clipboard_text, "] <iii> Another consecutive message"));
            assert(!strstr(clipboard_text, "chat-prefix:"));
            assert(strchr(clipboard_text, '\n'));
            elm_entry_select_none(ui.history);
            /* Exercise the keyboard path too: read-only Edje text does not
             * implement the editable entry's Ctrl+C handler. */
            evas_focus_in(evas_object_evas_get(ui.history));
            elm_entry_select_all(ui.history);
            settle_layout();
            free(clipboard_text);
            clipboard_text = NULL;
            Evas *canvas = evas_object_evas_get(ui.history);
            evas_key_modifier_on(canvas, "Control");
            evas_event_feed_key_down(canvas, "c", "c", NULL, NULL, 0, NULL);
            evas_event_feed_key_up(canvas, "c", "c", NULL, NULL, 0, NULL);
            evas_key_modifier_off(canvas, "Control");
            settle_layout();
            assert(clipboard_text && strstr(clipboard_text, "First message"));
            char *draft = strdup(elm_entry_entry_get(ui.input));
            elm_entry_entry_set(ui.input, "");
            elm_entry_selection_paste(ui.input);
            settle_layout();
            char *pasted = elm_entry_markup_to_utf8(elm_entry_entry_get(ui.input));
            assert(!strcmp(pasted, clipboard_text));
            free(pasted);
            elm_entry_entry_set(ui.input, draft);
            free(draft);
            elm_entry_select_none(ui.history);
            settle_layout();
            /* Select just message text with the mouse, not the timestamp or
             * nickname cells, then copy through the keyboard shortcut. */
            text = elm_entry_markup_to_utf8(elm_entry_entry_get(ui.history));
            char *word = strstr(text, "Visible after the notice");
            assert(word);
            prefix = strndup(text, word - text);
            int word_pos = eina_unicode_utf8_get_len(prefix);
            free(prefix);
            free(text);
            cur = evas_object_textblock_cursor_new(tb);
            Evas_Coord x, y, start_x, end_x, word_y, word_h;
            evas_object_geometry_get(tb, &x, &y, NULL, NULL);
            evas_textblock_cursor_pos_set(cur, word_pos);
            evas_textblock_cursor_pen_geometry_get(cur, &start_x, &word_y, NULL, &word_h);
            evas_textblock_cursor_pos_set(cur, word_pos + 7);
            evas_textblock_cursor_pen_geometry_get(cur, &end_x, NULL, NULL, NULL);
            evas_textblock_cursor_free(cur);
            evas_event_feed_mouse_in(canvas, 0, NULL);
            evas_event_feed_mouse_move(canvas, x + start_x, y + word_y + word_h / 2, 0, NULL);
            evas_event_feed_mouse_down(canvas, 1, EVAS_BUTTON_NONE, 0, NULL);
            evas_event_feed_mouse_move(canvas, x + end_x, y + word_y + word_h / 2, 0, NULL);
            evas_event_feed_mouse_up(canvas, 1, EVAS_BUTTON_NONE, 0, NULL);
            settle_layout();
            assert(elm_entry_selection_get(ui.history));
            free(clipboard_text);
            clipboard_text = NULL;
            evas_key_modifier_on(canvas, "Control");
            evas_event_feed_key_down(canvas, "c", "c", NULL, NULL, 0, NULL);
            evas_event_feed_key_up(canvas, "c", "c", NULL, NULL, 0, NULL);
            evas_key_modifier_off(canvas, "Control");
            settle_layout();
            assert(clipboard_text && !strcmp(clipboard_text, "Visible"));
            if (pass == 2) {
                evas_event_feed_mouse_down(canvas, 3, EVAS_BUTTON_NONE, 0, NULL);
                evas_event_feed_mouse_up(canvas, 3, EVAS_BUTTON_NONE, 0, NULL);
                settle_layout();
                Evas_Object *menu = find_hoversel(ui.win);
                assert(menu && elm_hoversel_expanded_get(menu));
                Elm_Object_Item *item, *copy_all = NULL;
                const Eina_List *l;
                EINA_LIST_FOREACH(elm_hoversel_items_get(menu), l, item) {
                    if (!strcmp(elm_object_item_text_get(item), "Copy all")) copy_all = item;
                }
                assert(copy_all);
                Evas_Object *copy_button = find_text_button(ui.win, "Copy all");
                assert(copy_button);
                evas_object_smart_callback_call(copy_button, "clicked", NULL);
                settle_layout();
                assert(clipboard_text && strstr(clipboard_text, "First message"));
                assert(strstr(clipboard_text, "Visible after the notice"));
            }
            elm_entry_select_none(ui.history);
        }
        elm_config_font_overlay_unset("entry");
        elm_config_font_overlay_apply();
        elaro_ui_buffer_select(channel);
        line.text = "<iii> Received while viewing another conversation";
        elaro_ui_buffer_event(&ui, &line);
        elaro_ui_buffer_select(columns);
        settle_layout();
        unsigned int repeats = 0;
        for (const char *at = elm_entry_entry_get(ui.history); (at = strstr(at, ":repeat>")); at++) repeats++;
        assert(repeats == 3);
        elaro_ui_buffer_select(channel);
        line.type = ELARO_ENGINE_BUFFER_CLOSED;
        elaro_ui_buffer_event(&ui, &line);
        elm_win_resize_object_del(ui.win, ui.history);
        evas_object_hide(ui.history);
    }

    /* A server's large history loads asynchronously, then follows new output. */
    {
        Elaro_Ui_Buffer *server = elaro_ui_buffer_get(&ui, "test", "");
        Elaro_Engine_Event line = { .type = ELARO_ENGINE_LINE, .server = "test",
            .target = "", .text = "A server message long enough to wrap across several lines in this viewport." };
        Evas_Coord y, h, content_h;
        Eina_Bool loaded = EINA_FALSE;
        evas_object_smart_callback_add(ui.history, "text,set,done", history_loaded, &loaded);
        for (unsigned int i = 0; i < 2000; i++) elaro_ui_buffer_event(&ui, &line);
        elm_win_resize_object_add(ui.win, ui.history);
        evas_object_resize(ui.win, 240, 300);
        elaro_ui_buffer_select(server);
        for (int i = 0; i < 20 && !loaded; i++) settle_layout();
        assert(loaded);
        settle_layout();
        char *plain = elm_entry_markup_to_utf8(elm_entry_entry_get(ui.history));
        assert(elm_entry_cursor_pos_get(ui.history) == eina_unicode_utf8_get_len(plain));
        free(plain);
        elm_scroller_region_get(ui.history, NULL, &y, NULL, &h);
        elm_scroller_child_size_get(ui.history, NULL, &content_h);
        /* The entry theme adds a small inset around the text viewport. */
        assert(content_h > h && y + h >= content_h - ELM_SCALE_SIZE(4));
        for (unsigned int i = 0; i < 10; i++) elaro_ui_buffer_event(&ui, &line);
        settle_layout();
        elm_scroller_region_get(ui.history, NULL, &y, NULL, &h);
        elm_scroller_child_size_get(ui.history, NULL, &content_h);
        assert(y + h >= content_h - ELM_SCALE_SIZE(4));
        elm_scroller_region_show(ui.history, 0, content_h / 3, 1, h);
        settle_layout();
        loaded = EINA_FALSE;
        evas_object_resize(ui.win, 300, 300);
        for (int i = 0; i < 20 && !loaded; i++) settle_layout();
        assert(loaded);
        settle_layout();
        elm_scroller_region_get(ui.history, NULL, &y, NULL, &h);
        elm_scroller_child_size_get(ui.history, NULL, &content_h);
        assert(y > 0 && y + h < content_h * 0.8);
        /* Switching away from a scrolled history must return to live output.
         * Exercise both a completed switch and switches during deferred reflow. */
        elaro_ui_buffer_select(channel);
        settle_layout();
        elaro_ui_buffer_select(server);
        settle_layout();
        elm_scroller_region_get(ui.history, NULL, &y, NULL, &h);
        elm_scroller_child_size_get(ui.history, NULL, &content_h);
        assert(y + h >= content_h - ELM_SCALE_SIZE(4));
        elm_scroller_region_show(ui.history, 0, content_h / 3, 1, h);
        settle_layout();
        evas_object_resize(ui.win, 260, 300);
        elaro_ui_buffer_select(channel);
        elaro_ui_buffer_select(server);
        for (int i = 0; i < 5; i++) settle_layout();
        elm_scroller_region_get(ui.history, NULL, &y, NULL, &h);
        elm_scroller_child_size_get(ui.history, NULL, &content_h);
        assert(y + h >= content_h - ELM_SCALE_SIZE(4));
        evas_object_smart_callback_del_full(ui.history, "text,set,done", history_loaded, &loaded);
        elaro_ui_buffer_select(channel);
        elm_win_resize_object_del(ui.win, ui.history);
        evas_object_hide(ui.history);
    }

    query = elaro_ui_buffer_get(&ui, "test", "alice");
    event.type = ELARO_ENGINE_LINE;
    event.target = "alice";
    event.text = "hello <world>";
    elaro_ui_buffer_event(&ui, &event);
    assert(query->unread && ui.current == channel);
    assert(ui.buffer_class->func.state_get(query, ui.buffers, "private_unread"));
    assert(!ui.buffer_class->func.state_get(channel, ui.buffers, "private_unread"));
    assert(strstr(eina_strbuf_string_get(query->history), "hello &lt;world&gt;"));
    elm_genlist_item_selected_set(query->item, EINA_TRUE);
    assert(!query->unread && ui.current == query);
    assert(!ui.buffer_class->func.state_get(query, ui.buffers, "private_unread"));
    assert(!strstr(elm_object_text_get(ui.title), " users)"));
    assert(!strstr(elm_object_text_get(ui.title), " user)"));
    assert(!ui.completion.matches);
    {
        Evas_Event_Key_Down ev = { .key = "Tab" };
        ev.modifiers = (Evas_Modifier *)evas_key_modifier_get(evas_object_evas_get(ui.input));
        assert(!elaro_ui_users_complete(&ui, ui.input, &ev));
    }
    /* Status lines remain in history without marking inactive channels unread. */
    event.target = channel->target;
    event.text = "* bob has joined #test";
    elaro_ui_buffer_event(&ui, &event);
    assert(!channel->unread && !channel->mention);
    assert(strstr(eina_strbuf_string_get(channel->history), "bob has joined"));
    char *status_label = ui.buffer_class->func.text_get(channel, ui.buffers, "elm.text");
    assert(!strstr(status_label, "<b>"));
    free(status_label);
    event.channel_message = true;
    event.text = "<bob> hello";
    elaro_ui_buffer_event(&ui, &event);
    assert(channel->unread && !channel->mention);
    char *message_label = ui.buffer_class->func.text_get(channel, ui.buffers, "elm.text");
    assert(strstr(message_label, "<b>"));
    free(message_label);
    /* Channel mentions retain their event and clear when the channel is read. */
    event.target = channel->target;
    event.mention = true;
    elaro_ui_buffer_event(&ui, &event);
    assert(channel->unread && channel->mention);
    assert(ui.buffer_class->func.state_get(channel, ui.buffers, "mention"));
    assert(!ui.buffer_class->func.state_get(channel, ui.buffers, "private_unread"));
    char *mention_label = ui.buffer_class->func.text_get(channel, ui.buffers, "elm.text");
    assert(!strstr(mention_label, "<color="));
    free(mention_label);
    elaro_ui_buffer_select(channel);
    assert(!ui.buffer_class->func.state_get(channel, ui.buffers, "mention"));
    elaro_ui_buffer_select(query);
    event.target = "alice";
    event.mention = false;
    event.type = ELARO_ENGINE_BUFFER_CLOSED;
    elaro_ui_buffer_event(&ui, &event);
    /* Closing the last row selects the previous conversation. */
    assert(ui.current == channel);
    assert(elm_genlist_item_selected_get(channel->item));
    assert(eina_list_count(ui.views) == 3);

    event.type = ELARO_ENGINE_JOINED;
    event.target = "#next";
    elaro_ui_buffer_event(&ui, &event);
    next = ui.current;
    assert(next != channel && !strcmp(next->target, "#next"));
    elm_genlist_item_selected_set(channel->item, EINA_TRUE);
    event.type = ELARO_ENGINE_BUFFER_CLOSED;
    event.target = "#test";
    elaro_ui_buffer_event(&ui, &event);
    assert(ui.current == next);
    assert(elm_genlist_item_selected_get(next->item));

    /* Closing an inactive row leaves the selection alone. */
    elaro_ui_buffer_get(&ui, "test", "bob");
    event.target = "bob";
    elaro_ui_buffer_event(&ui, &event);
    assert(ui.current == next);
    event.target = "#next";
    elaro_ui_buffer_event(&ui, &event);
    assert(!strcmp(ui.current->server, "test") && !*ui.current->target);
    assert(elm_genlist_item_selected_get(ui.current->item));
    assert(submitted > 0);

    /* Repeatedly leave a busy selected channel with all retained state populated. */
    {
        unsigned int baseline = eina_list_count(ui.views);
        Eina_Strbuf *names = eina_strbuf_new();
        char message[1024];
        memset(message, 'x', sizeof(message) - 1);
        message[sizeof(message) - 1] = 0;
        for (unsigned int i = 0; i < 500; i++)
            eina_strbuf_append_printf(names, "user%u\n", i);
        for (unsigned int cycle = 0; cycle < 5; cycle++) {
            Elaro_Ui_Buffer *previous = ui.current, *busy;
            event.type = ELARO_ENGINE_JOINED;
            event.target = "#busy";
            elaro_ui_buffer_event(&ui, &event);
            busy = ui.current;
            elm_genlist_item_selected_set(previous->item, EINA_TRUE);
            event.type = ELARO_ENGINE_LINE;
            event.text = message;
            for (unsigned int i = 0; i < 600; i++)
                elaro_ui_buffer_event(&ui, &event);
            assert(eina_strbuf_length_get(busy->history) <= 256 * 1024);
            elm_genlist_item_selected_set(busy->item, EINA_TRUE);
            update_users(&ui, eina_strbuf_string_get(names));
            assert(eina_hash_population(ui.users) == 500);
            for (unsigned int i = 0; i < 250; i++)
                elaro_ui_input_record(&ui, message);
            assert(eina_list_count(ui.current->input_history) == 200);
            input_set(&ui, "user");
            recall(&ui, "Up", message);
            event.type = ELARO_ENGINE_BUFFER_CLOSED;
            elaro_ui_buffer_event(&ui, &event);
            assert(eina_list_count(ui.views) == baseline);
            assert(!eina_hash_population(ui.users));
            assert(!ui.completion.matches);
            assert(strcmp(ui.current->target, "#busy"));
        }
        eina_strbuf_free(names);
    }

    /* Closing either kind of conversation selects the last remaining sibling,
     * even when channels and queries were opened in alternating order. */
    {
        Elaro_Ui_Buffer *first_channel = elaro_ui_buffer_get(&ui, "close-order", "#first");
        elaro_ui_buffer_get(&ui, "close-order", "alice");
        Elaro_Ui_Buffer *last_channel = elaro_ui_buffer_get(&ui, "close-order", "#last");
        Elaro_Ui_Buffer *last_query = elaro_ui_buffer_get(&ui, "close-order", "bob");
        Elaro_Ui_Buffer *server = first_channel->parent;
        Elaro_Engine_Event close = { .type = ELARO_ENGINE_BUFFER_CLOSED,
            .server = "close-order", .target = "#first" };
        first_channel->channel = last_channel->channel = EINA_TRUE;
        elaro_ui_buffer_select(first_channel);
        elaro_ui_buffer_event(&ui, &close);
        assert(ui.current == last_query);
        assert(elm_genlist_item_selected_get(last_query->item));
        close.target = "bob";
        elaro_ui_buffer_event(&ui, &close);
        assert(ui.current == last_channel);
        assert(elm_genlist_item_selected_get(last_channel->item));
        /* An inactive close must not change the active conversation. */
        close.target = "alice";
        elaro_ui_buffer_event(&ui, &close);
        assert(ui.current == last_channel);
        close.target = "#last";
        elaro_ui_buffer_event(&ui, &close);
        assert(ui.current == server);
        close.target = "";
        elaro_ui_buffer_event(&ui, &close);
        elaro_ui_buffer_select(elaro_ui_buffer_get(&ui, "test", ""));
    }

    /* The close icon consumes a real mouse click, without selecting its row
     * or submitting SELECT before the asynchronous close reply arrives. */
    {
        Elaro_Ui_Buffer *active = elaro_ui_buffer_get(&ui, "close-click", "#active");
        active->channel = EINA_TRUE;
        elaro_ui_buffer_select(active);
        elaro_ui_expand(ui.buffers);
        elm_win_resize_object_add(ui.win, ui.buffers);
        evas_object_resize(ui.win, 420, 600);
        evas_object_show(ui.buffers);
        const char *targets[] = { "someone", "#inactive" };
        for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(targets); i++) {
            Elaro_Ui_Buffer *inactive = elaro_ui_buffer_get(&ui, "close-click", targets[i]);
            inactive->channel = i == 1;
            Elaro_Engine_Event event = { .type = ELARO_ENGINE_LINE, .channel_message = true,
                .server = "close-click", .target = (char *)targets[i], .text = "unread message" };
            elaro_ui_buffer_event(&ui, &event);
            assert(inactive->unread);
            input_set(&ui, "keep typing");
            settle_layout();
            unsigned int before = submitted;
            click_close_icon(inactive);
            assert(submitted == before + 1);
            assert(ui.current == active);
            assert(inactive->unread);
            char *text = elm_entry_markup_to_utf8(elm_entry_entry_get(ui.input));
            assert(!strcmp(text, "keep typing"));
            free(text);
            event.type = ELARO_ENGINE_BUFFER_CLOSED;
            elaro_ui_buffer_event(&ui, &event);
            assert(ui.current == active);
        }
        Elaro_Engine_Event close = { .type = ELARO_ENGINE_BUFFER_CLOSED,
            .server = "close-click", .target = "" };
        elaro_ui_buffer_event(&ui, &close);
        elm_win_resize_object_del(ui.win, ui.buffers);
        evas_object_hide(ui.buffers);
        elaro_ui_buffer_select(elaro_ui_buffer_get(&ui, "test", ""));
    }

    /* Rejoining an existing channel preserves its row, history and selection. */
    {
        Elaro_Ui_Buffer *restored = elaro_ui_buffer_get(&ui, "test", "#restored");
        restored->channel = EINA_TRUE;
        eina_strbuf_append(restored->history, "retained history");
        Elaro_Ui_Buffer *selected_view = ui.current;
        unsigned int count = eina_list_count(ui.views);
        Elaro_Engine_Event rejoined = { .type = ELARO_ENGINE_JOINED, .server = "test", .target = "#restored" };
        elaro_ui_buffer_event(&ui, &rejoined);
        assert(elaro_ui_buffer_get(&ui, "test", "#restored") == restored);
        assert(!strcmp(eina_strbuf_string_get(restored->history), "retained history"));
        assert(eina_list_count(ui.views) == count);
        assert(ui.current == selected_view);
    }

    /* Collapsing destroys rows, never conversations. Exercise live events and
     * selection while the active conversation is hidden, then repeated rebuilds. */
    {
        Elaro_Ui_Buffer *server = elaro_ui_buffer_get(&ui, "test", "");
        Elaro_Ui_Buffer *active = elaro_ui_buffer_get(&ui, "test", "#tree");
        Elaro_Ui_Buffer *hidden;
        Elaro_Ui_Buffer *other = elaro_ui_buffer_get(&ui, "tree-other", "Someone");
        Elaro_Engine_Event line = { .type = ELARO_ENGINE_LINE, .server = "test",
            .target = "hidden-query", .text = "a hidden message" };
        char *draft;
        unsigned int count;

        assert(elm_genlist_item_parent_get(active->item) == server->item);
        assert(elm_genlist_item_parent_get(other->item) == other->parent->item);
        assert(!strcmp(other->parent->server, "tree-other"));
        /* Expander mouse clicks toggle the tree without selecting its server. */
        elm_win_resize_object_add(ui.win, ui.buffers);
        evas_object_show(ui.buffers);
        elaro_ui_buffer_select(other);
        elm_genlist_item_bring_in(server->item, ELM_GENLIST_ITEM_SCROLLTO_TOP);
        ecore_timer_add(0.4, layout_done, NULL);
        ecore_main_loop_begin();
        settle_layout();
        Evas_Object *view = elm_object_item_part_content_get(server->item, "elm.swallow.end");
        Evas_Object *arrow = NULL;
        for (; view && !arrow; view = evas_object_smart_parent_get(view))
            if (!strcmp(evas_object_type_get(view), "edje"))
                arrow = (Evas_Object *)edje_object_part_object_get(view, "arrow");
        assert(arrow && !evas_object_propagate_events_get(arrow));
        count = submitted;
        for (int i = 0; i < 2; i++) {
            Evas_Coord x, y, w, h;
            Evas *canvas = evas_object_evas_get(arrow);
            evas_object_geometry_get(arrow, &x, &y, &w, &h);
            assert(w > 0 && h > 0);
            evas_event_feed_mouse_in(canvas, 0, NULL);
            evas_event_feed_mouse_move(canvas, x + w / 2, y + h / 2, 0, NULL);
            evas_event_feed_mouse_down(canvas, 1, EVAS_BUTTON_NONE, 0, NULL);
            evas_event_feed_mouse_up(canvas, 1, EVAS_BUTTON_NONE, 0, NULL);
            settle_layout();
            assert(elm_genlist_item_expanded_get(server->item) == (i == 1));
            assert(ui.current == other && submitted == count);
        }
        elm_win_resize_object_del(ui.win, ui.buffers);
        evas_object_hide(ui.buffers);
        elaro_ui_buffer_select(active);
        input_set(&ui, "keep this draft");
        update_users(&ui, "Alice\nBob\n");
        count = submitted;
        evas_object_smart_callback_call(ui.buffers, "contract,request", server->item);
        assert(!elm_genlist_item_expanded_get(server->item));
        assert(!active->item && ui.current == active);
        assert(!elm_genlist_item_selected_get(server->item));
        assert(eina_hash_population(ui.users) == 2);
        elaro_ui_buffer_event(&ui, &line);
        hidden = elaro_ui_buffer_get(&ui, "test", "hidden-query");
        assert(hidden->parent == server && !hidden->item && hidden->unread);
        assert(ui.buffer_class->func.state_get(server, ui.buffers, "private_unread"));
        assert(strstr(eina_strbuf_string_get(hidden->history), "a hidden message"));
        assert(!elm_genlist_item_expanded_get(server->item));
        line.mention = true;
        elaro_ui_buffer_event(&ui, &line); /* Escalate an already unread row. */
        assert(hidden->mention);
        /* Theme changes replace the widget, retaining collapsed conversations
         * and their unread state without submitting a selection command. */
        for (int rebuild = 0; rebuild < 3; rebuild++) {
            Evas_Object *old_list = ui.buffers;
            elaro_ui_buffers_recreate(&ui);
            assert(ui.buffers != old_list);
            assert(ui.current == active && submitted == count);
            assert(!elm_genlist_item_expanded_get(server->item));
            assert(!active->item && !hidden->item);
            assert(hidden->mention && hidden->unread);
            assert(elm_genlist_item_expanded_get(other->parent->item));
            assert(other->item);
            assert(!strcmp(elm_entry_entry_get(ui.input), "keep this draft"));
        }
        char *label = ui.buffer_class->func.text_get(server, ui.buffers, "elm.text");
        assert(!strstr(label, "<color=") && strstr(label, "<b>"));
        assert(ui.buffer_class->func.state_get(server, ui.buffers, "mention"));
        free(label);
        elaro_ui_buffer_select(hidden);
        assert(!hidden->mention && !hidden->unread);
        assert(!ui.buffer_class->func.state_get(server, ui.buffers, "private_unread"));
        label = ui.buffer_class->func.text_get(server, ui.buffers, "elm.text");
        assert(!strstr(label, "<color="));
        assert(!ui.buffer_class->func.state_get(server, ui.buffers, "mention"));
        free(label);
        elaro_ui_buffer_select(active);
        elm_genlist_item_expanded_set(server->item, EINA_FALSE);
        line.mention = false;
        update_users(&ui, "Alice\nBob\n");
        count = submitted;
        evas_object_smart_callback_call(ui.buffers, "expand,request", server->item);
        assert(ui.current == active && active->item && hidden->item);
        elaro_ui_buffers_recreate(&ui);
        assert(ui.current == active && submitted == count);
        assert(elm_genlist_item_expanded_get(server->item));
        assert(elm_genlist_item_selected_get(active->item));
        assert(hidden->item);

        assert(elm_genlist_item_selected_get(active->item));
        assert(submitted == count);
        assert(eina_hash_population(ui.users) == 2);
        /* A collapsed server restores its own last chat after visiting another server. */
        elm_genlist_item_expanded_set(server->item, EINA_FALSE);
        assert(!elm_genlist_item_selected_get(server->item));
        elaro_ui_buffer_select(other);
        elm_genlist_item_selected_set(server->item, EINA_TRUE);
        assert(ui.current == active && !active->item);
        assert(!elm_genlist_item_expanded_get(server->item));
        draft = elm_entry_markup_to_utf8(elm_entry_entry_get(ui.input));
        assert(!strcmp(draft, "keep this draft"));
        free(draft);
        /* Expanding another server leaves the active conversation selected. */
        elaro_ui_buffer_select(other);
        elm_genlist_item_expanded_set(server->item, EINA_TRUE);
        assert(ui.current == other);
        assert(elm_genlist_item_selected_get(other->item));
        /* Selecting the expanded server replaces its remembered child. */
        elaro_ui_buffer_select(server);
        assert(ui.current == server && !server->last_selected);
        elm_genlist_item_expanded_set(server->item, EINA_FALSE);
        elaro_ui_buffer_select(other);
        elm_genlist_item_selected_set(server->item, EINA_TRUE);
        assert(ui.current == server);
        elm_genlist_item_expanded_set(server->item, EINA_TRUE);
        assert(ui.current == server);
        assert(elm_genlist_item_selected_get(server->item));
        for (unsigned int i = 0; i < 50; i++) {
            elm_genlist_item_expanded_set(server->item, EINA_FALSE);
            assert(!active->item && !hidden->item);
            elm_genlist_item_expanded_set(server->item, EINA_TRUE);
            assert(active->item && hidden->item);
            assert(eina_list_count(elm_genlist_item_subitems_get(server->item)) == 3);
        }
        elm_genlist_item_expanded_set(server->item, EINA_FALSE);
        elaro_ui_buffer_select(hidden);
        assert(hidden == ui.current && !hidden->unread);
        assert(elm_genlist_item_expanded_get(server->item));
        assert(!strcmp(active->input_draft, "keep this draft"));
        elaro_ui_buffer_select(active);
        draft = elm_entry_markup_to_utf8(elm_entry_entry_get(ui.input));
        assert(!strcmp(draft, "keep this draft"));
        free(draft);
        elm_genlist_item_expanded_set(server->item, EINA_FALSE);
        line.type = ELARO_ENGINE_BUFFER_CLOSED;
        line.target = "hidden-query";
        elaro_ui_buffer_event(&ui, &line);
        assert(ui.current == active && !active->item);
        line.target = "#tree";
        elaro_ui_buffer_event(&ui, &line);
        assert(ui.current != active && !strcmp(ui.current->server, "test"));
        assert(server->last_selected != active && server->last_selected != hidden);
        line.server = "tree-other";
        line.target = "";
        elm_genlist_item_expanded_set(other->parent->item, EINA_FALSE);
        elaro_ui_buffer_event(&ui, &line);
    }

    /* The server context menu targets the clicked server, not the current chat. */
    {
        Elaro_Ui_Buffer *server = elaro_ui_buffer_get(&ui, "test", "");
        Elaro_Ui_Buffer *other = elaro_ui_buffer_get(&ui, "other", "#chat");
        elm_genlist_item_selected_set(other->item, EINA_TRUE);
        /* Exercise a real button-3 click on the visible server row. */
        elaro_ui_expand(ui.buffers);
        elm_win_resize_object_add(ui.win, ui.buffers);
        evas_object_resize(ui.win, 420, 600);
        evas_object_show(ui.buffers);
        evas_object_show(ui.win);
        elm_genlist_item_bring_in(server->item, ELM_GENLIST_ITEM_SCROLLTO_TOP);
        ecore_timer_add(0.3, right_click_server, server);
        ecore_main_loop_begin();
        assert(ui.buffer_menu);
        const Eina_List *items = elm_menu_items_get(ui.buffer_menu);
        assert(eina_list_count(items) == 1);
        Elm_Object_Item *action = eina_list_data_get(items);
        assert(!strcmp(elm_object_item_text_get(action), "List channels"));
        elm_object_item_signal_emit(action, "elm,action,click", "elm");
        for (int i = 0; i < 10; i++) ecore_main_loop_iterate_may_block(EINA_FALSE);
        assert(listed_server && !strcmp(listed_server, "test"));
        assert(!ui.buffer_menu && ui.current == other);
        evas_object_smart_callback_call(ui.buffers, "clicked,right", other->item);
        assert(!ui.buffer_menu);
        Elaro_Ui_Buffer *status = elaro_ui_buffer_get(&ui, "Status", "");
        evas_object_smart_callback_call(ui.buffers, "clicked,right", status->item);
        assert(!ui.buffer_menu);
        /* A dismissed menu releases its copied destination. */
        evas_object_smart_callback_call(ui.buffers, "clicked,right", server->item);
        assert(ui.buffer_menu);
        evas_object_smart_callback_call(ui.buffer_menu, "dismissed", NULL);
        assert(!ui.buffer_menu);
        Elaro_Engine_Event remove_other = { .type = ELARO_ENGINE_BUFFER_CLOSED,
            .server = "other", .target = "" };
        elaro_ui_buffer_event(&ui, &remove_other);
    }

    /* The server close button submits an empty target, and its reply removes
     * every conversation for that server while preserving another network. */
    {
        Elaro_Ui_Buffer *server = elaro_ui_buffer_get(&ui, "test", "");
        Elaro_Ui_Buffer *other = elaro_ui_buffer_get(&ui, "other", "#busy");
        Evas_Object *button = ui.buffer_class->func.content_get(server, ui.buffers,
                                                               "elm.swallow.end");
        assert(button);
        evas_object_smart_callback_call(button, "clicked", NULL);
        assert(server_close_submitted);
        evas_object_del(button);
        elaro_ui_buffer_get(&ui, "test", "#busy");
        query = elaro_ui_buffer_get(&ui, "test", "someone");
        elm_genlist_item_selected_set(query->item, EINA_TRUE);
        event.type = ELARO_ENGINE_BUFFER_CLOSED;
        event.target = "";
        elm_genlist_item_expanded_set(server->item, EINA_FALSE);
        elaro_ui_buffer_event(&ui, &event);
        assert(eina_list_count(ui.views) == 3);
        assert(strcmp(ui.current->server, "test"));
        assert(elaro_ui_buffer_get(&ui, "other", "#busy") == other);
        /* Closing an inactive server also removes its child conversations. */
        event.server = "other";
        elaro_ui_buffer_event(&ui, &event);
        assert(eina_list_count(ui.views) == 1);
        assert(!strcmp(ui.current->server, "Status"));
        assert(!ui.buffer_class->func.content_get(ui.current, ui.buffers, "elm.swallow.end"));
    }

    {
        Elaro_Config config = { .notify_private_messages = 1 };
        Elaro_Engine_Event pm = { .type = ELARO_ENGINE_LINE, .server = "test",
                                 .target = "Someone", .text = "<Someone> hello",
                                 .private_message = true };
        elm_genlist_item_selected_set(elaro_ui_buffer_get(&ui, "test", "#active")->item, EINA_TRUE);
        ui.config = &config;
        elaro_ui_buffer_event(&ui, &pm);
        assert(notifications == 1 && sounds == 0);
        config.enable_sounds = 1;
        Elaro_Ui_Buffer *saved = ui.current;
        ui.current = elaro_ui_buffer_get(&ui, "test", "Someone");
        elaro_ui_buffer_event(&ui, &pm);
        assert(notifications == 1);
        assert(sounds == 0); /* Selected private conversations stay quiet. */
        ui.current = saved;
        config.notify_private_messages = 0;
        elaro_ui_buffer_event(&ui, &pm);
        assert(notifications == 1);
        assert(sounds == 1); /* Sound works with desktop notifications disabled. */
        config.notify_private_messages = 1;
        elaro_ui_buffer_event(&ui, &pm);
        assert(notifications == 2 && sounds == 2);
        config.enable_sounds = 0;
        elaro_ui_buffer_event(&ui, &pm);
        assert(notifications == 3 && sounds == 2);
        pm.private_message = false;
        config.enable_sounds = 1;
        elaro_ui_buffer_event(&ui, &pm);
        assert(notifications == 3 && sounds == 2);
        ui.config = NULL;
    }

    /* Presence updates do not create or select chats and survive tree collapse. */
    {
        Elaro_Engine_User users[] = {
            { .nick = "Alice", .away = true }, { .nick = "Bob", .prefix = '@', .away = true },
            { .nick = "Carol", .prefix = '+', .away = true }
        };
        Elaro_Ui_Buffer *chat = elaro_ui_buffer_get(&ui, "test", "Bob");
        Elaro_Ui_Buffer *other = elaro_ui_buffer_get(&ui, "other", "Bob");
        Elaro_Ui_Buffer *room = elaro_ui_buffer_get(&ui, "test", "#presence");
        Elaro_Engine_Event presence = { .type = ELARO_ENGINE_USER_PRESENCE,
            .server = "test", .target = "bOB", .away = true };
        Evas_Object *icon;
        unsigned int count = eina_list_count(ui.views);
        room->channel = EINA_TRUE;
        elaro_ui_buffer_select(room);
        elaro_ui_users_update(&ui, users, 3);
        const char *order[] = { "Bob", "Carol", "Alice" };
        check_users(&ui, order, 3);
        Elm_Object_Item *bob = elm_genlist_first_item_get(ui.nicks);
        const char *roles[] = { "emblem-important", "audio-input-microphone", "user-available" };
        Elm_Object_Item *row = bob;
        for (unsigned int i = 0; i < 3; i++) {
            icon = ui.user_class->func.content_get(elm_object_item_data_get(row), ui.nicks,
                                                   "elm.swallow.icon");
            assert(!strcmp(elm_icon_standard_get(icon), roles[i]));
            evas_object_del(icon);
            icon = ui.user_class->func.content_get(elm_object_item_data_get(row), ui.nicks,
                                                   "elm.swallow.end");
            assert(!strcmp(elm_icon_standard_get(icon), "user-idle"));
            evas_object_del(icon);
            row = elm_genlist_item_next_get(row);
        }
        elm_genlist_item_expanded_set(chat->parent->item, EINA_FALSE);
        elaro_ui_buffer_event(&ui, &presence);
        elaro_ui_users_presence(&ui, &presence);
        assert(chat->away && !chat->item && !other->away);
        assert(ui.current == room && eina_list_count(ui.views) == count);
        presence.away = false;
        elaro_ui_buffer_event(&ui, &presence);
        elaro_ui_users_presence(&ui, &presence);
        assert(!chat->away);
        assert(!ui.user_class->func.content_get(elm_object_item_data_get(bob), ui.nicks,
                                                "elm.swallow.end"));
        icon = ui.user_class->func.content_get(elm_object_item_data_get(bob), ui.nicks,
                                               "elm.swallow.icon");
        assert(!strcmp(elm_icon_standard_get(icon), "emblem-important"));
        evas_object_del(icon);
        presence.away = true;
        elaro_ui_buffer_event(&ui, &presence);
        elaro_ui_buffer_select(chat);
        icon = ui.buffer_class->func.content_get(chat, ui.buffers, "elm.swallow.icon");
        assert(!strcmp(elm_icon_standard_get(icon), "user-idle"));
        evas_object_del(icon);
        presence.type = ELARO_ENGINE_PRESENCE_RESET;
        presence.target = "";
        elaro_ui_buffer_event(&ui, &presence);
        assert(!chat->away && !other->away);
        presence.type = ELARO_ENGINE_USER_PRESENCE;
        presence.target = "nobody";
        elaro_ui_buffer_event(&ui, &presence);
        assert(eina_list_count(ui.views) == count);
    }

    /* Closing populated channels must release every row, including completion
     * matches and any cached profile details, across repeated join/part cycles. */
    elaro_ui_users_clear(&ui);
    user_free = ui.user_class->func.del;
    ui.user_class->func.del = count_user_free;
    for (unsigned int cycle = 0; cycle < 100; cycle++) {
        Elaro_Engine_Event joined = { .type = ELARO_ENGINE_JOINED,
            .server = "test", .target = "#lifecycle" };
        Elaro_Engine_Event details = { .type = ELARO_ENGINE_USER_INFO,
            .server = "test", .target = "Alice", .text = "Alice <alice@example.org>" };
        elaro_ui_buffer_event(&ui, &joined);
        elaro_ui_buffer_select(elaro_ui_buffer_get(&ui, "test", "#lifecycle"));
        update_users(&ui, "Alice\nBob\nCarol\n");
        elaro_ui_users_details(&ui, &details);
        input_set(&ui, "A");
        complete(&ui, "Alice: ");
        joined.type = ELARO_ENGINE_BUFFER_CLOSED;
        elaro_ui_buffer_event(&ui, &joined);
        assert(freed_users == (cycle + 1) * 3);
        assert(!eina_hash_population(ui.users));
        assert(!elm_genlist_first_item_get(ui.nicks));
        assert(!ui.completion.matches && !ui.completion.text);
    }
    /* Overflowing topics move while idle. Replacing the topic with short
     * text resets the offset. Deleting the entry also removes its timer. */
    {
        Evas_Object *window = elm_win_util_standard_add("topic-scroll", "Topic");
        Evas_Object *entry = elaro_ui_entry_add(window, EINA_FALSE);
        elm_win_resize_object_add(window, entry);
        evas_object_resize(window, 240, 60);
        evas_object_show(entry);
        evas_object_show(window);
        elm_entry_entry_set(entry,
            "A long channel topic that extends well beyond the available width of the window");
        elaro_ui_entry_auto_scroll_enable(entry);
        elm_object_focus_set(entry, EINA_FALSE);
        ecore_timer_add(2.2, layout_done, NULL);
        ecore_main_loop_begin();
        int x;
        elm_scroller_region_get(entry, &x, NULL, NULL, NULL);
        assert(x > 0);
        elm_entry_entry_set(entry, "Short topic");
        elm_object_focus_set(entry, EINA_FALSE);
        ecore_timer_add(0.2, layout_done, NULL);
        ecore_main_loop_begin();
        elm_scroller_region_get(entry, &x, NULL, NULL, NULL);
        assert(x == 0);
        evas_object_del(window);
    }
    elaro_ui_users_shutdown(&ui);
    elaro_ui_buffers_shutdown(&ui);
    evas_object_del(ui.win);
    elm_shutdown();
    free(listed_server);
    free(clipboard_text);
    return 0;
}
