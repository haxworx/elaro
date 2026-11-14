#include "src/fe-efl/ui/elaro_ui_private.h"
#include <assert.h>
#include "src/fe-efl/ui/elaro_ui_list.c"
static Elaro_Engine_Command_Type last_type;
static unsigned int submitted;
static char *last_server, *last_text;
void elaro_engine_submit(Elaro_Engine *engine, Elaro_Engine_Command *c)
{
    submitted++; last_type = c->type;
    free(last_server); free(last_text);
    last_server = strdup(c->server); last_text = c->text ? strdup(c->text) : NULL;
    free(c->server); free(c->target); free(c->text); free(c);
}
void elaro_ui_expand(Evas_Object *obj)
{
    evas_object_size_hint_weight_set(obj, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(obj, EVAS_HINT_FILL, EVAS_HINT_FILL);
}
static void batch(Elaro_Ui *ui, unsigned int id, unsigned int count)
{
    Elaro_Engine_Event e = { .type = ELARO_ENGINE_LIST_BATCH, .server = "test", .request_id = id,
        .channel_count = count, .channels = calloc(count, sizeof(Elaro_Engine_Channel)) };
    for (unsigned int i = 0; i < count; i++) {
        e.channels[i].name = elaro_string_printf("#room%u", i);
        e.channels[i].topic = strdup("Topic <tag> & text");
        e.channels[i].users = i;
    }
    elaro_ui_list_event(ui, &e);
    for (unsigned int i = 0; i < count; i++) { free(e.channels[i].name); free(e.channels[i].topic); }
    free(e.channels);
}
static Eina_Bool populated(void *data)
{
    Elaro_Ui_List *b = data;
    if (b->loading) return ECORE_CALLBACK_RENEW;
    ecore_main_loop_quit();
    return ECORE_CALLBACK_CANCEL;
}
int main(int argc, char **argv)
{
    elm_init(argc, argv);
    Elaro_Ui ui = {0};
    ui.win = elm_win_add(NULL, "test-list", ELM_WIN_BASIC);
    for (unsigned int cycle = 0; cycle < 5; cycle++) {
        Elaro_Engine_Event e = { .type = ELARO_ENGINE_LIST_OPEN, .server = "test" };
        elaro_ui_list_event(&ui, &e);
        Elaro_Ui_List *b = ui.channel_list;
        assert(b && b->loading && last_type == ELARO_ENGINE_LIST);
        assert(evas_object_visible_get(b->progress) && elm_progressbar_is_pulsing_get(b->progress));
        assert(!evas_object_visible_get(b->status));
        double weight_x, weight_y, align_x, align_y;
        evas_object_size_hint_weight_get(b->progress, &weight_x, &weight_y);
        evas_object_size_hint_align_get(b->progress, &align_x, &align_y);
        assert(weight_x == EVAS_HINT_EXPAND && align_x == EVAS_HINT_FILL);
        unsigned int before = submitted;
        elaro_ui_list_event(&ui, &e);
        assert(ui.channel_list == b && submitted == before);
        batch(&ui, b->request_id - 1, 1);
        assert(!b->pending);
        batch(&ui, b->request_id, 5000);
        assert(eina_list_count(b->pending) == 5000);
        assert(!elm_genlist_first_item_get(b->list));
        e.type = ELARO_ENGINE_LIST_END; e.request_id = b->request_id;
        elaro_ui_list_event(&ui, &e);
        unsigned int prior = b->count;
        assert(_populate(b) == ECORE_CALLBACK_RENEW);
        assert(b->count > prior && b->count - prior <= 128);
        ecore_timer_add(0.001, populated, b);
        ecore_main_loop_begin();
        assert(!b->loading && b->count == 5000 && !b->pending && !b->idler);
        assert(!evas_object_visible_get(b->progress) && !elm_progressbar_is_pulsing_get(b->progress));
        assert(evas_object_visible_get(b->status));
        Elm_Object_Item *item = elm_genlist_first_item_get(b->list);
        assert(item);
        char *topic = _row_text(elm_object_item_data_get(item), b->list, "elm.text.sub");
        assert(strstr(topic, "&lt;tag&gt;") && strstr(topic, "&amp;")); free(topic);
        elm_genlist_item_selected_set(item, EINA_TRUE);
        _selected(b, NULL, NULL);
        assert(!elm_object_disabled_get(b->join));
        Elaro_Ui_Buffer other = { .server = "other", .target = "#elsewhere" };
        ui.current = &other;
        evas_object_smart_callback_call(b->join, "clicked", NULL);
        assert(last_type == ELARO_ENGINE_INPUT && !strcmp(last_server, "test"));
        assert(!strcmp(last_text, "/join #room0"));
        _refresh(b, NULL, NULL);
        e.request_id = b->request_id; e.type = ELARO_ENGINE_LIST_ERROR; e.text = "Error <text>";
        elaro_ui_list_event(&ui, &e);
        assert(!b->loading && !elm_genlist_first_item_get(b->list));
        assert(!evas_object_visible_get(b->progress) && !elm_progressbar_is_pulsing_get(b->progress));
        assert(evas_object_visible_get(b->status));
        assert(strstr(elm_object_text_get(b->status), "&lt;text&gt;"));
        /* An empty result also stops the pulse and shows the channel count. */
        _refresh(b, NULL, NULL);
        e.request_id = b->request_id; e.type = ELARO_ENGINE_LIST_END;
        elaro_ui_list_event(&ui, &e);
        assert(!evas_object_visible_get(b->progress) && !elm_progressbar_is_pulsing_get(b->progress));
        assert(!strcmp(elm_object_text_get(b->status), "Channels: 0"));
        _refresh(b, NULL, NULL);
        batch(&ui, b->request_id, 5000);
        if (cycle == 4) {
            e.type = ELARO_ENGINE_BUFFER_CLOSED; e.target = "";
            elaro_ui_list_event(&ui, &e);
            assert(!b->available && !b->pending && !b->idler);
            assert(!evas_object_visible_get(b->progress) && !elm_progressbar_is_pulsing_get(b->progress));
            assert(elm_object_disabled_get(b->join) && elm_object_disabled_get(b->refresh));
        }
        elaro_ui_list_shutdown(&ui);
        assert(!ui.channel_list && last_type == ELARO_ENGINE_LIST_CANCEL);
        e.type = ELARO_ENGINE_LIST_END;
        elaro_ui_list_event(&ui, &e);
        ui.current = NULL;
    }
    free(last_server); free(last_text);
    evas_object_del(ui.win);
    elm_shutdown();
    return 0;
}
