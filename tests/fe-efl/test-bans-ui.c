#include "src/fe-efl/ui/elaro_ui_private.h"
#include <assert.h>
/* Inspect the window's controls while exercising real Elementary lifetimes. */
#include "src/fe-efl/ui/elaro_ui_bans.c"
static unsigned int submitted;
static Elaro_Engine_Command_Type last_type;
static char *last_mask;
void elaro_engine_submit(Elaro_Engine *engine, Elaro_Engine_Command *c)
{
    submitted++; last_type = c->type;
    if (c->type == ELARO_ENGINE_USER_INFO) {
        assert(!strcmp(c->server, "test"));
        assert(!strcmp(c->target, "alice"));
    }
    free(last_mask); last_mask = c->text ? strdup(c->text) : NULL;
    free(c->server); free(c->target); free(c->text); free(c);
}
void elaro_ui_expand(Evas_Object *obj)
{
    evas_object_size_hint_weight_set(obj, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(obj, EVAS_HINT_FILL, EVAS_HINT_FILL);
}
static void click_heading(Elaro_Ui *ui)
{
    Evas *canvas = evas_object_evas_get(ui->title_button);
    Evas_Coord x, y, w, h;
    for (int i = 0; i < 5; i++) ecore_main_loop_iterate_may_block(EINA_FALSE);
    evas_smart_objects_calculate(canvas);
    evas_object_geometry_get(ui->title_button, &x, &y, &w, &h);
    assert(w > 0 && h > 0);
    evas_event_feed_mouse_in(canvas, 0, NULL);
    evas_event_feed_mouse_move(canvas, x + w / 2, y + h / 2, 0, NULL);
    evas_event_feed_mouse_down(canvas, 1, EVAS_BUTTON_NONE, 0, NULL);
    evas_event_feed_mouse_up(canvas, 1, EVAS_BUTTON_NONE, 0, NULL);
    for (int i = 0; i < 5; i++) ecore_main_loop_iterate_may_block(EINA_FALSE);
}
int main(int argc, char **argv)
{
    elm_init(argc, argv);
    Elaro_Ui ui = {0};
    Elaro_Ui_Buffer channel = { .server = "test", .target = "#test", .channel = EINA_TRUE };
    ui.win = elm_win_add(NULL, "test-bans", ELM_WIN_BASIC);
    ui.current = &channel;
    Evas_Object *heading = elaro_ui_bans_heading_add(&ui, ui.win);
    elm_object_text_set(ui.title, "<align=center><b>test — #test (2 users)</b></align>");
    elm_win_resize_object_add(ui.win, heading);
    evas_object_resize(ui.win, 640, 100);
    evas_object_show(ui.win);
    for (unsigned int cycle = 0; cycle < 10; cycle++) {
        click_heading(&ui);
        Elaro_Ui_Bans *b = ui.bans;
        assert(b && b->loading && elm_object_disabled_get(b->unban));
        unsigned int count = submitted;
        click_heading(&ui);
        assert(ui.bans == b && submitted == count);
        Elaro_Engine_Event e = { .server = "test", .target = "#test", .type = ELARO_ENGINE_BAN_ENTRY,
            .request_id = b->request_id, .text = "*!*@<bad>&example" };
        e.request_id--;
        elaro_ui_bans_event(&ui, &e);
        assert(!elm_genlist_first_item_get(b->list));
        e.request_id++;
        elaro_ui_bans_event(&ui, &e);
        e.type = ELARO_ENGINE_BAN_END;
        elaro_ui_bans_event(&ui, &e);
        Elm_Object_Item *item = elm_genlist_first_item_get(b->list);
        assert(item && !elm_genlist_item_next_get(item));
        char *markup = _mask_text(elm_object_item_data_get(item), b->list, "elm.text");
        assert(strstr(markup, "&lt;bad&gt;&amp;")); free(markup);
        elm_genlist_item_selected_set(item, EINA_TRUE);
        _selected(b, NULL, NULL);
        assert(!elm_object_disabled_get(b->unban));
        _unban(b, NULL, NULL);
        assert(last_type == ELARO_ENGINE_UNBAN);
        assert(!strcmp(last_mask, "*!*@<bad>&example"));
        assert(!elm_genlist_first_item_get(b->list));
        e.request_id = b->request_id; e.type = ELARO_ENGINE_BAN_END;
        elaro_ui_bans_event(&ui, &e);
        assert(!strcmp(elm_object_text_get(b->status), "No bans in this channel."));
        _refresh(b, NULL, NULL);
        e.request_id = b->request_id; e.type = ELARO_ENGINE_BAN_ERROR; e.text = "Error <text>";
        elaro_ui_bans_event(&ui, &e);
        assert(!elm_object_disabled_get(b->refresh));
        assert(strstr(elm_object_text_get(b->status), "&lt;text&gt;"));
        e.type = ELARO_ENGINE_BUFFER_CLOSED;
        elaro_ui_bans_event(&ui, &e);
        assert(elm_object_disabled_get(b->refresh));
        elaro_ui_bans_shutdown(&ui);
        assert(!ui.bans);

        /* Closing must release masks even before the server ends its reply. */
        click_heading(&ui);
        b = ui.bans;
        assert(b && b->loading);
        e.request_id = b->request_id; e.type = ELARO_ENGINE_BAN_ENTRY;
        e.text = "*!*@remaining.example";
        elaro_ui_bans_event(&ui, &e);
        assert(elm_genlist_first_item_get(b->list));
        if (cycle % 2) {
            e.type = ELARO_ENGINE_BAN_END;
            elaro_ui_bans_event(&ui, &e);
        }
        evas_object_del(b->win);
        assert(!ui.bans);
        /* Late replies after window deletion must be harmless. */
        e.type = ELARO_ENGINE_BAN_END;
        elaro_ui_bans_event(&ui, &e);
    }
    Elaro_Ui_Buffer query = { .server = "test", .target = "alice" };
    ui.current = &query;
    unsigned int count = submitted;
    click_heading(&ui);
    assert(submitted == count + 1 && last_type == ELARO_ENGINE_USER_INFO);
    assert(!ui.bans);
    Elaro_Ui_Buffer server = { .server = "test", .target = "" };
    ui.current = &server;
    click_heading(&ui);
    assert(submitted == count + 1);
    free(last_mask);
    evas_object_del(ui.win);
    elm_shutdown();
    return 0;
}
