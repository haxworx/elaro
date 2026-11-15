#include "src/fe-efl/ui/elaro_ui_private.h"
#include <Ecore_Evas.h>
#include <assert.h>
#include <stdint.h>
#include <math.h>

/* Observe requests to the compositor while retaining real pointer routing. */
static unsigned int move_requests;
static Evas_Object *move_window;
static Elm_Win_Move_Resize_Mode move_mode;
Eina_Bool elm_win_move_resize_start(Evas_Object *window, Elm_Win_Move_Resize_Mode mode)
{
   move_requests++;
   move_window = window;
   move_mode = mode;
   return EINA_TRUE;
}

/* Exercise actual canvas pointer delivery without an IRC engine. */
static void _close_window_cb(void *data, Evas_Object *obj, void *info) {}
void elaro_engine_submit(Elaro_Engine *engine, Elaro_Engine_Command *command)
{
   elaro_engine_command_free(command);
}
void elaro_ui_external_link_open(Elaro_Ui *ui, const char *url) {}
#include "src/fe-efl/ui/elaro_ui_theme.h"

static Ecore_Evas *test_canvas;

static Eina_Bool _stop(void *data)
{
   ecore_main_loop_quit();
   return ECORE_CALLBACK_CANCEL;
}
static void _settle(void)
{
   if (test_canvas) ecore_evas_manual_render(test_canvas);
   ecore_timer_add(0.06, _stop, NULL);
   ecore_main_loop_begin();
}
static void _click_at(Evas *canvas, int x, int y)
{
   unsigned int timestamp = (unsigned int)(ecore_time_get() * 1000);
   evas_event_feed_mouse_in(canvas, timestamp, NULL);
   evas_event_feed_mouse_move(canvas, x, y, timestamp, NULL);
   evas_event_feed_mouse_down(canvas, 1, EVAS_BUTTON_NONE, timestamp + 1, NULL);
   evas_event_feed_mouse_up(canvas, 1, EVAS_BUTTON_NONE, timestamp + 2, NULL);
   _settle();
}
static void _click(Evas_Object *object)
{
   int x, y, w, h;
   assert(object);
   _settle();
   evas_object_geometry_get(object, &x, &y, &w, &h);
   assert(w > 0 && h > 0);
   _click_at(evas_object_evas_get(object), x + w / 2, y + h / 2);
}
static void _type_key(Evas_Object *field)
{
   Evas *canvas = evas_object_evas_get(field);
   elm_entry_entry_set(field, "");
   evas_focus_in(canvas);
   unsigned int stamp = (unsigned int)(ecore_time_get() * 1000);
   evas_event_feed_key_down(canvas, "a", "a", "a", "a", stamp, NULL);
   evas_event_feed_key_up(canvas, "a", "a", "a", "a", stamp + 1, NULL);
   _settle();
   assert(!strcmp(elm_entry_entry_get(field), "a"));
}

static void _type_in(Evas_Object *field)
{
   _click(field);
   _type_key(field);
}

static void _deleted(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   *(Eina_Bool *)data = EINA_TRUE;
}
static void _close_log(const Eina_Log_Domain *domain, Eina_Log_Level level,
                       const char *file, const char *function, int line,
                       const char *format, void *data, va_list args)
{
   if (level <= EINA_LOG_LEVEL_ERR) (*(unsigned int *)data)++;
   eina_log_print_cb_stderr(domain, level, file, function, line, format, NULL, args);
}
static void _close_and_wait(Evas_Object *popup, Evas_Object *button)
{
   Eina_Bool deleted = EINA_FALSE;
   unsigned int errors = 0;
   assert(popup);
   evas_object_event_callback_add(popup, EVAS_CALLBACK_DEL, _deleted, &deleted);
   int log_level = eina_log_level_get();
   int eo_level = eina_log_domain_level_get("eo");
   int elm_level = eina_log_domain_level_get("elementary");
   eina_log_level_set(EINA_LOG_LEVEL_ERR);
   eina_log_domain_level_set("eo", EINA_LOG_LEVEL_ERR);
   eina_log_domain_level_set("elementary", EINA_LOG_LEVEL_ERR);
   eina_log_print_cb_set(_close_log, &errors);
   if (button) _click(button);
   else evas_object_smart_callback_call(popup, "block,clicked", NULL);
   for (int i = 0; !deleted && i < 10; i++) _settle();
   eina_log_print_cb_set(eina_log_print_cb_stderr, NULL);
   eina_log_level_set(log_level);
   eina_log_domain_level_set("eo", eo_level);
   eina_log_domain_level_set("elementary", elm_level);
   assert(deleted);
   assert(errors == 0);
}
static char *_row_text(void *data, Evas_Object *obj, const char *part)
{
   return strdup(data);
}
static int selected, closed;
static void _row_selected(void *data, Evas_Object *obj, void *info) { selected++; }
static void _row_close(void *data, Evas_Object *obj, void *info) { closed++; }
static void _tree_expand(void *data, Evas_Object *obj, void *info)
{
   elm_genlist_item_expanded_set(info, EINA_TRUE);
}
static void _tree_contract(void *data, Evas_Object *obj, void *info)
{
   elm_genlist_item_expanded_set(info, EINA_FALSE);
}
static void _close_alignment_check(Elm_Object_Item **items)
{
   int expected_x = 0, expected_w = 0;
   for (int i = 0; i < 3; i++)
     {
        Evas_Object *close = elm_object_item_part_content_get(items[i], "elm.swallow.end");
        assert(close);
        int x, w;
        evas_object_geometry_get(close, &x, NULL, &w, NULL);
        assert(w > 0);
        if (!i) { expected_x = x; expected_w = w; }
        else assert(x == expected_x && w == expected_w);
     }
}
static Evas_Object *_row_content(void *data, Evas_Object *obj, const char *part)
{
   Evas_Object *button;
   if (strcmp(part, "elm.swallow.end")) return NULL;
   button = elm_button_add(obj);
   elm_object_text_set(button, "×");
   evas_object_propagate_events_set(button, EINA_FALSE);
   evas_object_smart_callback_add(button, "clicked", _row_close, NULL);
   return button;
}
/* Inspect inline timestamp/nickname textblocks, which Edje can cache on reflow. */
static int _prefix_colors_check(Evas_Object *object, const char *color)
{
   int found = 0;
   if (!strcmp(evas_object_type_get(object), "textblock"))
     {
        const char *markup = evas_object_textblock_text_markup_get(object);
        if (markup && (strstr(markup, "Alice") || strstr(markup, "[12:34]"))
            && !strstr(markup, "Theme persistence"))
          {
             const Evas_Textblock_Style *style = evas_object_textblock_style_user_peek(object);
             assert(style && strstr(evas_textblock_style_get(style), color));
             assert(!strstr(evas_textblock_style_get(style), "color=#00000000"));
             assert(strstr(evas_textblock_style_get(style), "font_size="));
             int width, height;
             evas_object_textblock_size_native_get(object, &width, &height);
             assert(width > 0 && height > 0);
             int cell_width, cell_height;
             evas_object_geometry_get(object, NULL, NULL, &cell_width, &cell_height);
             assert(cell_width > 0 && cell_height >= height);
             assert(evas_object_visible_get(object));
             found++;
          }
     }
   Eina_List *members = evas_object_smart_members_get(object), *node;
   Evas_Object *child;
   EINA_LIST_FOREACH(members, node, child) found += _prefix_colors_check(child, color);
   eina_list_free(members);
   return found;
}
/* Optional render artifacts for visual inspection; the test uses buffer Evas. */
static void _snapshot(Elaro_Ui *ui, const char *directory, int index, const char *name)
{
   Ecore_Evas *source;
   Evas_Object *image;
   int w, h;
   char *path;
   const void *pixels;
   if (!directory) return;
   for (int i = 0; i < 5; i++) _settle();
   source = ecore_evas_ecore_evas_get(evas_object_evas_get(ui->win));
   ecore_evas_manual_render(source);
   pixels = ecore_evas_buffer_pixels_get(source);
   assert(pixels);
   ecore_evas_geometry_get(source, NULL, NULL, &w, &h);
   image = evas_object_image_add(evas_object_evas_get(ui->win));
   evas_object_image_size_set(image, w, h);
   evas_object_image_data_copy_set(image, (void *)pixels);
   path = elaro_string_printf("%s/%s-%s.png", directory, theme_names[index], name);
   assert(evas_object_image_save(image, path, NULL, NULL));
   free(path);
   evas_object_del(image);
}

static void _context_menu_check(Elaro_Ui *ui, const char *artifacts, int index)
{
   const char *list_file, *list_group;
   edje_object_file_get(elm_layout_edje_get(ui->buffers), &list_file, &list_group);
   assert(!strcmp(list_file, ui->theme_path));
   assert(!strcmp(list_group, "elm/genlist/base/default"));
   for (int users = 0; users < 2; users++)
     {
        Evas_Object *menu = elm_menu_add(ui->win);
        int activated = 0;
        Elm_Object_Item *item = elm_menu_item_add(menu, NULL, NULL,
            users ? "Private message" : "List channels", _row_selected, NULL);
        Elm_Object_Item *separator = NULL, *disabled = NULL;
        if (users)
          {
             elm_menu_item_add(menu, NULL, NULL, "Whois", _row_selected, NULL);
             separator = elm_menu_item_separator_add(menu, NULL);
             disabled = elm_menu_item_add(menu, NULL, NULL, "Kick", _row_selected, NULL);
             elm_object_item_disabled_set(disabled, EINA_TRUE);
          }
        elm_menu_move(menu, users ? 550 : 30, 200);
        evas_object_show(menu);
        _settle();
        const char *file, *group;
        Evas_Object *view = elm_menu_item_object_get(item);
        edje_object_file_get(elm_layout_edje_get(view), &file, &group);
        assert(!strcmp(file, ui->theme_path));
        assert(!strcmp(group, "elm/menu/item/default"));
        if (separator)
          {
             edje_object_file_get(elm_layout_edje_get(elm_menu_item_object_get(separator)), &file, &group);
             assert(!strcmp(file, ui->theme_path));
             assert(!strcmp(group, "elm/menu/separator/default"));
          }
        int x, y, w, h;
        evas_object_geometry_get(view, &x, &y, &w, &h);
        assert(w > 0 && h >= ELM_SCALE_SIZE(40));
        evas_event_feed_mouse_move(evas_object_evas_get(view), x + w / 2, y + h / 2, 0, NULL);
        _settle();
        assert(elm_menu_item_selected_get(item));
        assert(!strcmp(edje_object_part_state_get(elm_layout_edje_get(view), "surface", NULL), "active"));
        _snapshot(ui, artifacts, index, users ? "user-context" : "server-context");
        activated = selected;
        if (disabled)
          {
             _click(elm_menu_item_object_get(disabled));
             assert(selected == activated);
             evas_object_show(menu);
             _settle();
          }
        _click(view);
        assert(selected == activated + 1);
        evas_object_del(menu);
     }
}

static void _progress_check(Elaro_Ui *ui, const char *artifacts, int index)
{
   Evas_Object *popup = elm_popup_add(ui->win);
   Evas_Object *box = elm_box_add(popup);
   elm_object_part_text_set(popup, "title,text", "Progress");
   elm_box_padding_set(box, 0, ELM_SCALE_SIZE(12));
   elm_object_content_set(popup, box);
   evas_object_show(box);
   for (int vertical = 0; vertical < 2; vertical++)
     {
        Evas_Object *bar = elm_progressbar_add(box);
        elm_progressbar_horizontal_set(bar, !vertical);
        elm_progressbar_span_size_set(bar, ELM_SCALE_SIZE(240));
        elm_object_text_set(bar, vertical ? "Transfer" : "Receiving image");
        evas_object_size_hint_weight_set(bar, EVAS_HINT_EXPAND, 0);
        evas_object_size_hint_align_set(bar, EVAS_HINT_FILL, 0.5);
        elm_box_pack_end(box, bar);
        evas_object_show(bar);
        elm_progressbar_value_set(bar, 0.25);
        evas_object_show(popup);
        _settle();
        Evas_Object *edje = elm_layout_edje_get(bar);
        const char *file, *group;
        edje_object_file_get(edje, &file, &group);
        assert(!strcmp(file, ui->theme_path));
        assert(!strcmp(group, vertical ? "elm/progressbar/vertical/default" : "elm/progressbar/horizontal/default"));
        int tw, th, bw, bh;
        edje_object_part_geometry_get(edje, "track", NULL, NULL, &tw, &th);
        edje_object_part_geometry_get(edje, "bar", NULL, NULL, &bw, &bh);
        assert(tw > 0 && th > 0);
        if (abs((vertical ? bh : bw) - (vertical ? th : tw) / 4) > 2)
          {
             int tx, ty, cx, cy, cw, ch, sx, sy, sw, sh;
             edje_object_part_geometry_get(edje, "track", &tx, &ty, NULL, NULL);
             edje_object_part_geometry_get(edje, "elm.cur.progressbar", &cx, &cy, &cw, &ch);
             edje_object_part_geometry_get(edje, "elm.swallow.bar", &sx, &sy, &sw, &sh);
             fprintf(stderr, "Progress orientation=%d track=%d,%d %dx%d fill=%dx%d cursor=%d,%d %dx%d span=%d,%d %dx%d at 25%%\n", vertical, tx, ty, tw, th, bw, bh, cx, cy, cw, ch, sx, sy, sw, sh);
          }
        assert(abs((vertical ? bh : bw) - (vertical ? th : tw) / 4) <= 2);
        elm_progressbar_value_set(bar, 0.75);
        _settle();
        edje_object_part_geometry_get(edje, "bar", NULL, NULL, &bw, &bh);
        assert(abs((vertical ? bh : bw) - (vertical ? th : tw) * 3 / 4) <= 2);
        Evas_Object *label = (Evas_Object *)edje_object_part_object_get(edje, "elm.text");
        assert(!strcmp(evas_object_text_text_get(label), vertical ? "Transfer" : "Receiving image"));
        _snapshot(ui, artifacts, index, vertical ? "progress-vertical" : "progress");
        elm_progressbar_inverted_set(bar, EINA_TRUE);
        _settle();
        edje_object_part_geometry_get(edje, "bar", NULL, NULL, &bw, &bh);
        assert(abs((vertical ? bh : bw) - (vertical ? th : tw) * 3 / 4) <= 2);
        elm_progressbar_inverted_set(bar, EINA_FALSE);
        elm_progressbar_pulse_set(bar, EINA_TRUE);
        elm_progressbar_pulse(bar, EINA_TRUE);
        _settle();
        assert(!strcmp(edje_object_part_state_get(edje, "fraction-clip", NULL), "pulse"));
        assert(!strcmp(edje_object_part_state_get(edje, "pulse-clip", NULL), "active"));
        int x, y, nx, ny;
        edje_object_part_geometry_get(edje, "pulse", &x, &y, NULL, NULL);
        for (int frame = 0; frame < 4; frame++) _settle();
        edje_object_part_geometry_get(edje, "pulse", &nx, &ny, NULL, NULL);
        assert(vertical ? ny != y : nx != x);
        _snapshot(ui, artifacts, index, vertical ? "pulse-vertical" : "pulse");
        double scale = elm_config_scale_get();
        elm_config_scale_set(scale * 1.25);
        _settle();
        assert(!strcmp(edje_object_part_state_get(edje, "pulse-clip", NULL), "active"));
        elm_config_scale_set(scale);
        _settle();
        elm_progressbar_pulse(bar, EINA_FALSE);
        _settle();
        assert(!strcmp(edje_object_part_state_get(edje, "pulse-clip", NULL), "default"));
        elm_progressbar_pulse_set(bar, EINA_FALSE);
        elm_progressbar_value_set(bar, 0.5);
        _settle();
        assert(!strcmp(edje_object_part_state_get(edje, "fraction-clip", NULL), "default"));
        elm_object_disabled_set(bar, EINA_TRUE);
        _settle();
        assert(!strcmp(edje_object_part_state_get(edje, "disabled-clip", NULL), "disabled"));
        evas_object_del(bar);
     }
   evas_object_del(popup);
}

static void _subwindow_check(Elaro_Ui *ui, const char *artifacts, int index)
{
   Evas_Object *win = elm_win_add(ui->win, "dialog-theme-test", ELM_WIN_DIALOG_BASIC);
   elm_win_title_set(win, "Channel list — Elaro");
   Evas_Object *body = elaro_ui_window_content_add(win);
   Evas_Object *label = elm_label_add(body);
   elm_object_text_set(label, "Secondary window controls");
   elm_box_pack_end(body, label);
   evas_object_show(label);
   evas_object_resize(win, 480, 260);
   evas_object_show(win);
   Ecore_Evas *main_canvas = test_canvas;
   test_canvas = ecore_evas_ecore_evas_get(evas_object_evas_get(win));
   _settle();
   assert(elm_win_borderless_get(win));
   Evas_Object *frame = evas_object_data_get(win, "elaro-window-frame");
   assert(frame);
   const char *file, *group;
   edje_object_file_get(elm_layout_edje_get(frame), &file, &group);
   assert(!strcmp(file, ui->theme_path));
   assert(!strcmp(group, "elaro/dialog/frame"));
   const char *assets[] = { "minimize", "maximize", "close" };
   double scale = elm_config_scale_get();
   for (int pass = 0; pass < 3; pass++)
     {
        elm_config_scale_set(pass == 1 ? scale * 1.25 : scale);
        _settle();
        for (int i = 0; i < 3; i++)
          {
             Evas_Object *button = evas_object_data_get(win, assets[i]);
             Evas_Object *icon = elm_object_part_content_get(button, "icon");
             Evas_Object *edje = elm_layout_edje_get(button);
             Evas_Object *surface = (Evas_Object *)edje_object_part_object_get(edje, "surface");
             Evas *canvas = evas_object_evas_get(button);
             int x, y, w, h, r, g, b, a;
             evas_event_feed_mouse_in(canvas, 0, NULL);
             evas_event_feed_mouse_move(canvas, 0, 0, 0, NULL);
             _settle();
             evas_object_color_get(surface, &r, &g, &b, &a);
             assert(a == 0);
             evas_object_geometry_get(button, &x, &y, &w, &h);
             evas_event_feed_mouse_move(canvas, x + w / 2, y + h / 2, 1, NULL);
             _settle();
             assert(!strcmp(edje_object_part_state_get(edje, "surface", NULL), "over"));
             evas_object_color_get(surface, &r, &g, &b, &a);
             assert(a > 0);
             char expected[64];
             snprintf(expected, sizeof(expected), "elaro/icons/%s", assets[i]);
             elm_image_file_get(icon, &file, &group);
             assert(!strcmp(file, ui->theme_path) && !strcmp(group, expected));
             evas_object_geometry_get(icon, NULL, NULL, &w, &h);
             assert(w == ELM_SCALE_SIZE(20) && h == ELM_SCALE_SIZE(20));
          }
     }
   Evas_Object *drag = evas_object_data_get(win, "elaro-window-drag");
   int x, y, w, h;
   evas_object_geometry_get(drag, &x, &y, &w, &h);
   assert(w > 0 && h > 0);
   Evas *canvas = evas_object_evas_get(win);
   unsigned int stamp = (unsigned int)(ecore_time_get() * 1000);
   unsigned int before = move_requests;
   evas_event_feed_mouse_in(canvas, stamp, NULL);
   evas_event_feed_mouse_move(canvas, x + w / 2, y + h / 2, stamp, NULL);
   evas_event_feed_mouse_down(canvas, 1, EVAS_BUTTON_NONE, stamp + 1, NULL);
   assert(move_requests == before + 1);
   assert(move_window == win && move_mode == ELM_WIN_MOVE_RESIZE_MOVE);
   evas_event_feed_mouse_up(canvas, 1, EVAS_BUTTON_NONE, stamp + 2, NULL);
   before = move_requests;
   evas_event_feed_mouse_move(canvas, 1, 1, stamp + 3, NULL);
   evas_event_feed_mouse_down(canvas, 1, EVAS_BUTTON_NONE, stamp + 4, NULL);
   assert(move_requests == before + 1);
   assert(move_window == win && move_mode == (ELM_WIN_MOVE_RESIZE_TOP | ELM_WIN_MOVE_RESIZE_LEFT));
   evas_event_feed_mouse_up(canvas, 1, EVAS_BUTTON_NONE, stamp + 5, NULL);
   Elaro_Ui snapshot = *ui;
   snapshot.win = win;
   _snapshot(&snapshot, artifacts, index, "secondary-window");
   test_canvas = main_canvas;
   _close_and_wait(win, evas_object_data_get(win, "close"));
}

static void _check_visual(Evas_Object *check, Eina_Bool selected)
{
   _settle();
   assert(elm_check_state_get(check) == selected);
   Evas_Object *edje = elm_layout_edje_get(check);
   const char *state = edje_object_part_state_get(edje, "mark", NULL);
   assert(state && !strcmp(state, selected ? "checked" : "default"));
}

static void _single_line_scroll_check(Evas_Object *entry)
{
   Evas *canvas = evas_object_evas_get(entry);
   Eina_Bool horizontal, vertical;
   elm_scroller_bounce_get(entry, &horizontal, &vertical);
   assert(!horizontal && !vertical);
   assert(elm_scroller_movement_block_get(entry) == ELM_SCROLLER_MOVEMENT_BLOCK_VERTICAL);
   char *saved = strdup(elm_entry_entry_get(entry));
   /* Desktop font overlays must affect themed entries without app overrides. */
   int small_height, large_height;
   elm_config_font_overlay_set("entry", "Sans", 11);
   elm_config_font_overlay_apply();
   _settle();
   evas_object_textblock_size_native_get(elm_entry_textblock_get(entry), NULL, &small_height);
   elm_config_font_overlay_set("entry", "Sans", 20);
   elm_config_font_overlay_apply();
   _settle();
   evas_object_textblock_size_native_get(elm_entry_textblock_get(entry), NULL, &large_height);
   assert(large_height > small_height);
   elm_config_font_overlay_unset("entry");
   elm_config_font_overlay_apply();
   _settle();
   Eina_Strbuf *long_line = eina_strbuf_new();
   for (int i = 0; i < 30; i++) eina_strbuf_append(long_line, "A long single line that should scroll horizontally while staying vertically still. ");
   elm_entry_entry_set(entry, eina_strbuf_string_get(long_line));
   eina_strbuf_free(long_line);
   elm_object_focus_set(entry, EINA_TRUE);
   elm_entry_cursor_end_set(entry);
   _settle();
   int x, y, w, h;
   evas_object_geometry_get(entry, &x, &y, &w, &h);
   unsigned int stamp = (unsigned int)(ecore_time_get() * 1000);
   evas_event_feed_mouse_move(canvas, x + w / 2, y + h / 2, stamp, NULL);
   evas_event_feed_mouse_wheel(canvas, 0, 3, stamp + 1, NULL);
   _settle();
   elm_scroller_region_get(entry, &x, &y, NULL, NULL);
   assert(y == 0);
   assert(x > 0);
   elm_entry_entry_set(entry, saved);
   free(saved);
}

static int _visible_prefixes_check(Evas_Object *object, int top, int bottom, unsigned int foreground)
{
   int found = 0;
   if (!strcmp(evas_object_type_get(object), "textblock"))
     {
        const char *markup = evas_object_textblock_text_markup_get(object);
        int x, y, w, h, width, height;
        evas_object_geometry_get(object, &x, &y, &w, &h);
        if (markup && !strcmp(markup, "[12:34]") && y >= top && y + h <= bottom)
          {
             evas_object_textblock_size_native_get(object, &width, &height);
             assert(width > 0 && height > 0 && h >= height);
             assert(evas_object_visible_get(object));
             int canvas_w, canvas_h;
             ecore_evas_geometry_get(test_canvas, NULL, NULL, &canvas_w, &canvas_h);
             const uint32_t *pixels = ecore_evas_buffer_pixels_get(test_canvas);
             Eina_Bool ink = EINA_FALSE;
             for (int py = y; py < y + h && py < canvas_h; py++)
                for (int px = x; px < x + w && px < canvas_w; px++)
                   if (px >= 0 && py >= 0 &&
                       (pixels[py * canvas_w + px] & 0xffffff) == foreground)
                      ink = EINA_TRUE;
             assert(ink);
             found++;
          }
     }
   Eina_List *members = evas_object_smart_members_get(object), *node;
   Evas_Object *child;
   EINA_LIST_FOREACH(members, node, child)
      found += _visible_prefixes_check(child, top, bottom, foreground);
   eina_list_free(members);
   return found;
}

static void _history_scroll_check(Elaro_Ui *ui)
{
   size_t length = eina_strbuf_length_get(ui->current->history);
   int r, g, b, a;
   edje_color_class_get("/fg/normal/entry/text", &r, &g, &b, &a,
                        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
   unsigned int foreground = (r << 16) | (g << 8) | b;
   for (int i = 0; i < 100; i++)
     {
        const char *line = i % 2 ? "[12:34] &lt;Alice&gt; Scrollable themed history"
                                : "[12:34] &lt;Bob&gt; Scrollable themed history";
        eina_strbuf_append(ui->current->history, line);
        eina_strbuf_append(ui->current->history, "<br/>");
        elaro_ui_history_append(ui, line);
        if (!(i % 10))
          {
             elm_cache_all_flush();
             _settle();
          }
     }
   _settle();
   int x, y, w, h, content_h;
   evas_object_geometry_get(ui->history, &x, &y, &w, &h);
   assert(_visible_prefixes_check(ui->history, y, y + h, foreground) > 2);
   elm_scroller_child_size_get(ui->history, NULL, &content_h);
   assert(content_h > h);
   elm_scroller_region_show(ui->history, 0, 0, w, h);
   _settle();
   assert(_visible_prefixes_check(ui->history, y, y + h, foreground) > 2);
   Evas *canvas = evas_object_evas_get(ui->history);
   unsigned int stamp = (unsigned int)(ecore_time_get() * 1000);
   evas_event_feed_mouse_move(canvas, x + w / 2, y + h / 2, stamp, NULL);
   evas_event_feed_mouse_wheel(canvas, 0, 3, stamp + 1, NULL);
   _settle();
   elm_scroller_region_get(ui->history, NULL, &y, NULL, NULL);
   assert(y > 0);
   elm_entry_select_all(ui->history);
   assert(elm_entry_selection_get(ui->history));
   elm_entry_select_none(ui->history);
   eina_strbuf_remove(ui->current->history, length, eina_strbuf_length_get(ui->current->history));
   elaro_ui_history_refresh(ui);
   _settle();
}

static void _placeholder_check(Evas_Object *entry, const char *path)
{
   char *saved = strdup(elm_entry_entry_get(entry));
   Evas_Object *edje = evas_object_smart_parent_get(elm_entry_textblock_get(entry));
   Evas_Object *guide = (Evas_Object *)edje_object_part_object_get(edje, "elm.guide");
   assert(guide);
   const char *style = evas_textblock_style_get(evas_object_textblock_style_get(guide));
   assert(style && strstr(style, "font_style=Italic"));
   char *value = edje_file_data_get(path, "/fg/normal/entry/text/guide");
   int r, g, b, a;
   assert(value && sscanf(value, "%d %d %d %d", &r, &g, &b, &a) == 4);
   free(value);
   char color[32];
   snprintf(color, sizeof(color), "color=#%02x%02x%02x%02x", r, g, b, a);
   assert(strstr(style, color));
   const char *typed_style = evas_textblock_style_get(evas_object_textblock_style_get(elm_entry_textblock_get(entry)));
   assert(typed_style && !strstr(typed_style, "font_style=Italic"));
   elm_entry_entry_set(entry, "");
   _settle();
   assert(!strcmp(edje_object_part_state_get(edje, "elm.guide", NULL), "default"));
   elm_entry_entry_set(entry, "Typed text");
   _settle();
   assert(!strcmp(edje_object_part_state_get(edje, "elm.guide", NULL), "hidden"));
   elm_entry_entry_set(entry, saved);
   free(saved);
}

static void
_resize_check(Elaro_Ui *ui)
{
   static const char *grips[] = {"top", "bottom", "left", "right", "tl", "tr", "bl", "br"};
   static const Elm_Win_Move_Resize_Mode modes[] = {
      ELM_WIN_MOVE_RESIZE_TOP, ELM_WIN_MOVE_RESIZE_BOTTOM,
      ELM_WIN_MOVE_RESIZE_LEFT, ELM_WIN_MOVE_RESIZE_RIGHT,
      ELM_WIN_MOVE_RESIZE_TOP | ELM_WIN_MOVE_RESIZE_LEFT,
      ELM_WIN_MOVE_RESIZE_TOP | ELM_WIN_MOVE_RESIZE_RIGHT,
      ELM_WIN_MOVE_RESIZE_BOTTOM | ELM_WIN_MOVE_RESIZE_LEFT,
      ELM_WIN_MOVE_RESIZE_BOTTOM | ELM_WIN_MOVE_RESIZE_RIGHT
   };
   for (unsigned int edge = 0; edge < EINA_C_ARRAY_LENGTH(grips); edge++)
     {
        int x, y, w, h, ox, oy;
        Evas *canvas = evas_object_evas_get(ui->win);
        evas_object_geometry_get(ui->frame, &ox, &oy, NULL, NULL);
        assert(edje_object_part_geometry_get(elm_layout_edje_get(ui->frame), grips[edge], &x, &y, &w, &h));
        unsigned int before = move_requests;
        unsigned int stamp = (unsigned int)(ecore_time_get() * 1000);
        evas_event_feed_mouse_move(canvas, ox + x + w / 2, oy + y + h / 2, stamp, NULL);
        evas_event_feed_mouse_down(canvas, 1, EVAS_BUTTON_NONE, stamp + 1, NULL);
        /* Must reach the window API before mouse-up or a main loop turn. */
        assert(move_requests == before + 1);
        assert(move_window == ui->win && move_mode == modes[edge]);
        evas_event_feed_mouse_up(canvas, 1, EVAS_BUTTON_NONE, stamp + 2, NULL);
     }
}

int main(int argc, char **argv)
{
   Eina_Error error = 0;
   Elaro_Ui ui = {0};
   Elaro_Ui_Buffer conversation = {0};
   Evas_Object *panes;
   Evas_Object *entry, *secret, *button, *icon, *row, *end, *tabs, *check;
   Elm_Genlist_Item_Class *classes[3];
   Elm_Object_Item *items[3];
   const char *file, *group;
   const char *artifacts = argc > 1 ? argv[1] : NULL;
   (void)_frame_move;
   (void)_frame_resize;
   (void)_window_minimize;
   (void)_window_maximize;
   assert(elm_init(argc, argv));
   ui.config = elaro_config_load(&error);
   assert(ui.config && !error);
   ui.win = elm_win_add(NULL, "themes", ELM_WIN_BASIC);
   assert(ui.win);
   test_canvas = ecore_evas_ecore_evas_get(evas_object_evas_get(ui.win));
   _theme_apply(&ui);
   ui.frame = elm_layout_add(ui.win);
   assert(elm_layout_file_set(ui.frame, ui.theme_path, "elaro/frame"));
   elaro_ui_expand(ui.frame);
   elm_win_resize_object_add(ui.win, ui.frame);
   ui.content = elm_box_add(ui.win);
   elaro_ui_expand(ui.content);
   elm_object_part_content_set(ui.frame, "elaro.content", ui.content);
   _frame_bind(&ui);
   /* Reserve the header hit area, as the real window's toolbar does. */
   Evas_Object *header = evas_object_rectangle_add(evas_object_evas_get(ui.win));
   evas_object_color_set(header, 0, 0, 0, 0);
   evas_object_pass_events_set(header, EINA_TRUE);
   elaro_ui_size_min_set(header, 0, 48);
   elm_box_pack_end(ui.content, header);
   evas_object_show(header);
   entry = elaro_ui_entry_add(ui.content, EINA_FALSE);
   elm_object_part_text_set(entry, "guide", "Example placeholder");
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_entry_set(entry, "Unsent draft");
   elm_box_pack_end(ui.content, entry);
   evas_object_show(entry);
   secret = elaro_ui_entry_add(ui.content, EINA_TRUE);
   elm_entry_single_line_set(secret, EINA_TRUE);
   elm_entry_scrollable_set(secret, EINA_TRUE);
   elm_entry_password_set(secret, EINA_TRUE);
   elm_entry_entry_set(secret, "masked-example");
   elm_box_pack_end(ui.content, secret);
   evas_object_show(secret);
   check = elm_check_add(ui.content);
   elm_object_text_set(check, "Remember this choice");
   elm_check_state_set(check, EINA_TRUE);
   elm_box_pack_end(ui.content, check);
   evas_object_show(check);
   button = elaro_ui_icon_button_add(ui.content, "menu", "Menu", _menu_show, &ui);
   elm_box_pack_end(ui.content, button);
   icon = elm_object_part_content_get(button, "icon");
   assert(icon);
   ui.buffers = elm_genlist_add(ui.content);
   elm_genlist_tree_effect_enabled_set(ui.buffers, EINA_FALSE);
   elaro_ui_expand(ui.buffers);
   elm_genlist_select_mode_set(ui.buffers, ELM_OBJECT_SELECT_MODE_ALWAYS);
   panes = elm_panes_add(ui.content);
   elaro_ui_expand(panes);
   elm_box_pack_end(ui.content, panes);
   elm_object_part_content_set(panes, "left", ui.buffers);
   ui.history = elaro_ui_history_add(panes);
   Evas_Object *heading = elm_label_add(ui.content);
   elm_object_style_set(heading, "elaro/title");
   elm_object_text_set(heading, "#theme-test");
   evas_object_size_hint_weight_set(heading, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(heading, EVAS_HINT_FILL, 0.5);
   elm_box_pack_start(ui.content, heading);
   evas_object_pass_events_set(heading, EINA_TRUE);
   evas_object_show(heading);
   elaro_ui_expand(ui.history);
   elm_object_part_content_set(panes, "right", ui.history);
   elm_panes_content_left_size_set(panes, 0.4);
   evas_object_show(panes);
   evas_object_show(ui.history);
   conversation.server = "test";
   conversation.target = "#test";
   conversation.channel = EINA_TRUE;
   conversation.history = eina_strbuf_new();
   ui.current = &conversation;
   elaro_ui_history_refresh(&ui);
   /* New channels can receive their first messages before the first layout. */
   evas_object_resize(ui.history, 1, 1);
   eina_strbuf_append(conversation.history, "[12:34] &lt;Alice&gt; Theme persistence<br/>");
   elaro_ui_history_append(&ui, "[12:34] &lt;Alice&gt; Theme persistence");
   int initial_width, initial_height;
   assert(sscanf(strstr(elm_entry_entry_get(ui.history), "<item "),
                 "<item absize=%dx%d", &initial_width, &initial_height) == 2);
   assert(initial_width > 0 && initial_height > 0);
   for (int j = 0; j < 3; j++)
     {
        classes[j] = elm_genlist_item_class_new();
        classes[j]->func.text_get = _row_text;
        classes[j]->func.content_get = _row_content;
        classes[j]->item_style = j ? "default_style" : "default";
        items[j] = elm_genlist_item_append(ui.buffers, classes[j],
                                          j == 0 ? "Network" : j == 1 ? "#enlightenment" : "Alice",
                                          j ? items[0] : NULL, j == 0 ? ELM_GENLIST_ITEM_TREE : ELM_GENLIST_ITEM_NONE,
                                          _row_selected, NULL);
     }
   evas_object_smart_callback_add(ui.buffers, "expand,request", _tree_expand, NULL);
   evas_object_smart_callback_add(ui.buffers, "contract,request", _tree_contract, NULL);
   elm_genlist_item_expanded_set(items[0], EINA_TRUE);
   evas_object_resize(ui.win, 900, 760);
   evas_object_show(ui.win);
   _settle();
   assert(_prefix_colors_check(ui.history, "color=#e0e9f8ff") == 2);
   _close_alignment_check(items);
   /* First launch must leave room around Settings at desktop scale 2. */
   double startup_scale = elm_config_scale_get();
   elm_config_scale_set(2.0);
   elaro_ui_layout_restore(&ui);
   elaro_ui_connection_show(&ui, NULL, NULL);
   _settle();
   int startup_w, startup_h, popup_x, popup_y, popup_w, popup_h;
   evas_object_geometry_get(ui.win, NULL, NULL, &startup_w, &startup_h);
   evas_object_geometry_get(elm_object_content_get(ui.connection.popup),
                            &popup_x, &popup_y, &popup_w, &popup_h);
   assert(popup_x > 0 && popup_y > 0);
   assert(popup_x + popup_w < startup_w);
   assert(popup_y + popup_h < startup_h);
   _close_and_wait(ui.connection.popup, evas_object_data_get(ui.connection.popup, "close"));
   elm_config_scale_set(startup_scale);
   evas_object_resize(ui.win, 900, 760);
   _settle();
   /* Scale up and back down without reopening widgets or losing the draft. */
   double original_scale = elm_config_scale_get();
   elaro_ui_box_padding_set(ui.content, 3, 5);
   for (int pass = 0; pass < 3; pass++)
     {
        double factor = pass == 0 ? 2.0 : pass == 1 ? 1.25 : 1.0;
        elm_config_scale_set(original_scale * factor);
        _settle();
        _close_alignment_check(items);
        int bx, by;
        Eina_Size2D icon_min = efl_gfx_hint_size_combined_min_get(icon);
        assert(icon_min.w == ELM_SCALE_SIZE(22) && icon_min.h == ELM_SCALE_SIZE(22));
        elm_box_padding_get(ui.content, &bx, &by);
        assert(bx == ELM_SCALE_SIZE(3) && by == ELM_SCALE_SIZE(5));
        int x, y, w, h;
        edje_object_part_geometry_get(elm_layout_edje_get(ui.frame), "elaro.content", &x, &y, &w, &h);
        assert(abs(x - ELM_SCALE_SIZE(6)) <= 1);
        evas_object_geometry_get(ui.resize_grips[7], NULL, NULL, &w, &h);
        assert(w == ELM_SCALE_SIZE(6) && h == ELM_SCALE_SIZE(6));
        Eina_Size2D header_min = efl_gfx_hint_size_combined_min_get(header);
        assert(header_min.h == ELM_SCALE_SIZE(48));
        _resize_check(&ui);
        int entry_h, secret_h;
        evas_object_geometry_get(entry, NULL, NULL, NULL, &entry_h);
        evas_object_geometry_get(secret, NULL, NULL, NULL, &secret_h);
        assert(entry_h == ELM_SCALE_SIZE(48) && secret_h == entry_h);
        _check_visual(check, EINA_TRUE);
        _single_line_scroll_check(entry);
        assert(!strcmp(elm_entry_entry_get(entry), "Unsent draft"));
     }
   elm_object_focus_set(secret, EINA_TRUE);
   _type_key(secret);
   elm_entry_entry_set(secret, "masked-example");
   _click(button);
   assert(ui.app_menu && evas_object_visible_get(ui.app_menu));
   _resize_check(&ui);
   for (int i = 0; i < 4; i++)
     {
        Elaro_Config *saved;
        _click(ui.theme_buttons[i]);
        assert(ui.config->theme == i);
        assert(!evas_object_visible_get(ui.theme_status));
        Evas_Object *separator = evas_object_data_get(ui.app_menu, "separator");
        edje_object_file_get(elm_layout_edje_get(separator), &file, &group);
        assert(!strcmp(file, ui.theme_path));
        assert(!strcmp(group, "elm/separator/horizontal/default"));
        int sy, sh, settings_y;
        evas_object_geometry_get(separator, NULL, &sy, NULL, &sh);
        evas_object_geometry_get(evas_object_data_get(ui.app_menu, "settings"), NULL, &settings_y, NULL, NULL);
        assert(sh == ELM_SCALE_SIZE(1));
        assert(settings_y - sy - sh <= ELM_SCALE_SIZE(4));
        saved = elaro_config_load(&error);
        assert(saved && !error && saved->theme == i);
        elaro_config_free(saved);
        assert(elm_object_part_content_get(ui.frame, "elaro.content") == ui.content);
        edje_object_file_get(elm_layout_edje_get(heading), &file, &group);
        assert(!strcmp(file, ui.theme_path));
        assert(!strcmp(group, "elm/label/base/elaro/title"));
        int heading_w, heading_h;
        evas_object_geometry_get(heading, NULL, NULL, &heading_w, &heading_h);
        assert(heading_w > 0 && heading_h > 0);
        edje_object_file_get(evas_object_smart_parent_get(elm_entry_textblock_get(ui.history)), &file, &group);
        assert(!strcmp(file, ui.theme_path));
        assert(!strcmp(group, "elm/entry/base-noedit-mixedwrap/elaro/history"));
        assert(!strcmp(elm_entry_entry_get(entry), "Unsent draft"));
        elm_image_file_get(icon, &file, &group);
        assert(!strcmp(file, ui.theme_path));
        assert(!strcmp(group, "elaro/icons/menu"));
        assert(edje_file_group_exists(file, "elaro/about/hero"));
        assert(elm_entry_password_get(secret));
        assert(!strcmp(elm_entry_entry_get(secret), "masked-example"));
        edje_object_file_get(evas_object_smart_parent_get(elm_entry_textblock_get(secret)), &file, &group);
        assert(!strcmp(file, ui.theme_path));
        assert(!strcmp(group, "elm/entry/base-password/default"));
        _snapshot(&ui, artifacts, i, "menu");
        _close_and_wait(ui.app_menu, evas_object_data_get(ui.app_menu, "close"));
        assert(!ui.app_menu);
        _context_menu_check(&ui, artifacts, i);
        _progress_check(&ui, artifacts, i);
        _subwindow_check(&ui, artifacts, i);
        _placeholder_check(entry, ui.theme_path);
        _check_visual(check, EINA_TRUE);
        _click(check);
        _check_visual(check, EINA_FALSE);
        elm_object_theme_set(check, NULL);
        _check_visual(check, EINA_FALSE);
        _click(check);
        evas_event_feed_mouse_move(evas_object_evas_get(check), 0, 0, 0, NULL);
        _check_visual(check, EINA_TRUE);
        elm_object_disabled_set(check, EINA_TRUE);
        _check_visual(check, EINA_TRUE);
        elm_object_disabled_set(check, EINA_FALSE);
        _check_visual(check, EINA_TRUE);
        assert(strstr(elm_entry_entry_get(ui.history), "Theme persistence"));
        int cr, cg, cb, ca;
        assert(edje_color_class_get("/fg/normal/entry/text", &cr, &cg, &cb, &ca,
                                    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
        int tr, tg, tb, ta;
        evas_object_color_get(edje_object_part_object_get(elm_layout_edje_get(heading), "elm.text"),
                              &tr, &tg, &tb, &ta);
        assert(tr == cr && tg == cg && tb == cb && ta == ca);
        char expected_color[32];
        snprintf(expected_color, sizeof(expected_color), "color=#%02x%02x%02x%02x", cr, cg, cb, ca);
        assert(!elm_entry_text_style_user_peek(ui.history));
        /* Toolkit re-theming (also used for opacity/desktop changes) must
         * retain system fonts and regenerate copied inline styles. */
        elm_object_theme_set(ui.history, NULL);
        elm_theme_flush(NULL);
        _settle();
        assert(strstr(elm_entry_entry_get(ui.history), "Theme persistence"));
        assert(!elm_entry_text_style_user_peek(ui.history));
        assert(_prefix_colors_check(ui.history, expected_color) > 0);
        /* Desktop palette updates must not override this theme's explicit
         * history foreground on newly created inline cells. */
        edje_color_class_set("/fg/normal/entry/text", 0, 0, 0, 0,
                             0, 0, 0, 0, 0, 0, 0, 0);
        elm_entry_entry_set(ui.history, "");
        elaro_ui_history_refresh(&ui);
        _settle();
        assert(_prefix_colors_check(ui.history, expected_color) == 2);
        int history_y, history_h;
        evas_object_geometry_get(ui.history, NULL, &history_y, NULL, &history_h);
        assert(_visible_prefixes_check(ui.history, history_y, history_y + history_h,
                                       (cr << 16) | (cg << 8) | cb) > 0);
        edje_color_class_set("/fg/normal/entry/text", cr, cg, cb, ca,
                             cr, cg, cb, ca, cr, cg, cb, ca);
        elm_config_font_overlay_set("entry", "Sans", 24);
        elm_config_font_overlay_apply();
        _settle();
        assert(_prefix_colors_check(ui.history, expected_color) == 2);
        elm_config_font_overlay_unset("entry");
        elm_config_font_overlay_apply();
        _settle();
        assert(elm_object_part_content_get(panes, "right") == ui.history);
        assert(elm_object_part_content_get(panes, "left") == ui.buffers);
        assert(fabs(elm_panes_content_left_size_get(panes) - 0.4) < 0.02);
        Evas_Object *pane_edje = elm_layout_edje_get(panes);
        edje_object_file_get(pane_edje, &file, &group);
        assert(!strcmp(file, ui.theme_path));
        int px, py, pw, ph;
        edje_object_part_geometry_get(pane_edje, "elm.bar", &px, &py, &pw, &ph);
        int ox, oy;
        evas_object_geometry_get(pane_edje, &ox, &oy, NULL, NULL);
        unsigned int stamp = (unsigned int)(ecore_time_get() * 1000);
        Evas *canvas = evas_object_evas_get(panes);
        evas_event_feed_mouse_move(canvas, ox + px + pw / 2, oy + py + ph / 2, stamp, NULL);
        evas_event_feed_mouse_down(canvas, 1, EVAS_BUTTON_NONE, stamp + 1, NULL);
        evas_event_feed_mouse_move(canvas, ox + px + pw / 2 + 60, oy + py + ph / 2, stamp + 2, NULL);
        evas_event_feed_mouse_up(canvas, 1, EVAS_BUTTON_NONE, stamp + 3, NULL);
        assert(elm_panes_content_left_size_get(panes) > 0.44);
        elm_panes_content_left_size_set(panes, 0.4);
        _history_scroll_check(&ui);
        _snapshot(&ui, artifacts, i, "history");
        unsigned int before_move = move_requests;
        _click_at(evas_object_evas_get(ui.win), 120, 25);
        assert(move_requests == before_move + 1);
        assert(move_window == ui.win && move_mode == ELM_WIN_MOVE_RESIZE_MOVE);
        _resize_check(&ui);
        _close_alignment_check(items);
        Evas_Object *tree_view = elm_object_item_part_content_get(items[0], "elm.swallow.end");
        Evas_Object *tree_edje = NULL;
        for (Evas_Object *parent = tree_view; parent && !tree_edje; parent = evas_object_smart_parent_get(parent))
          if (!strcmp(evas_object_type_get(parent), "edje") && edje_object_part_exists(parent, "arrow")) tree_edje = parent;
        assert(tree_edje);
        Evas_Object *arrow = (Evas_Object *)edje_object_part_object_get(tree_edje, "arrow");
        evas_object_propagate_events_set(arrow, EINA_FALSE);
        int before_expand = selected;
        _click(arrow);
        assert(!elm_genlist_item_expanded_get(items[0]));
        assert(selected == before_expand);
        _click(arrow);
        assert(elm_genlist_item_expanded_get(items[0]));
        assert(selected == before_expand);
        _close_alignment_check(items);
        for (int j = 0; j < 3; j++)
          {
             int before = selected;
             elm_genlist_item_selected_set(items[j], EINA_FALSE);
             row = elm_object_item_track(items[j]);
             assert(row);
             _click(row);
             elm_object_item_untrack(items[j]);
             assert(selected == before + 1);
             assert(elm_genlist_selected_item_get(ui.buffers) == items[j]);
             end = elm_object_item_part_content_get(items[j], "elm.swallow.end");
             assert(end);
             before = selected;
             int closed_before = closed;
             _click(end);
             assert(closed == closed_before + 1);
             assert(selected == before);
          }
        _click(button);
        _click(evas_object_data_get(ui.app_menu, "settings"));
        assert(ui.connection.popup);
        assert(!strcmp(elm_object_part_text_get(ui.connection.popup, "title,text"), "Settings"));
        /* Desktop settings can be reloaded while this popup stays open. */
        elm_config_reload();
        _settle();
        int reload_r, reload_g, reload_b, reload_a;
        assert(edje_color_class_get("/fg/normal/popup/text/title",
                                    &reload_r, &reload_g, &reload_b, &reload_a,
                                    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
        assert(reload_r == cr && reload_g == cg && reload_b == cb && reload_a == ca);
        assert(elm_entry_password_get(ui.connection.password));
        edje_object_file_get(evas_object_smart_parent_get(elm_entry_textblock_get(ui.connection.password)),
                             &file, &group);
        assert(!strcmp(file, ui.theme_path));
        assert(!strcmp(group, "elm/entry/base-password/default"));
        _snapshot(&ui, artifacts, i, "settings");
        Evas_Object *fields[] = {ui.connection.host, ui.connection.port, ui.connection.nick,
           ui.connection.username, ui.connection.realname, ui.connection.email,
           ui.connection.password, ui.connection.channels,
           ui.connection.sasl_username, ui.connection.sasl_password};
        for (unsigned int f = 0; f < EINA_C_ARRAY_LENGTH(fields); f++)
          {
             int height;
             evas_object_geometry_get(fields[f], NULL, NULL, NULL, &height);
             assert(height == ELM_SCALE_SIZE(48));
          }
        _type_in(ui.connection.host);
        assert(ui.connection.dirty);
        int edited_height;
        evas_object_geometry_get(ui.connection.host, NULL, NULL, NULL, &edited_height);
        assert(edited_height == ELM_SCALE_SIZE(48));
        tabs = evas_object_data_get(ui.connection.popup, "pages");
        int tx, ty, tw, th;
        evas_object_geometry_get(tabs, &tx, &ty, &tw, &th);
        assert(th >= ELM_SCALE_SIZE(48));
        _click_at(evas_object_evas_get(tabs), tx + tw * 3 / 4, ty + th / 2);
        assert(ui.connection.options_visible);
        Evas_Object *save_label = (Evas_Object *)edje_object_part_object_get(
            elm_layout_edje_get(ui.connection.apply), "elm.text");
        assert(save_label);
        assert(!strcmp(evas_object_text_text_get(save_label), "Save options"));
        _snapshot(&ui, artifacts, i, "options");
        evas_object_geometry_get(tabs, &tx, &ty, &tw, &th);
        _click_at(evas_object_evas_get(tabs), tx + tw / 4, ty + th / 2);
        assert(!ui.connection.options_visible);
        _close_and_wait(ui.connection.popup, evas_object_data_get(ui.connection.popup, "close"));
        assert(!ui.connection.popup);
        /* Reopening must restore focus and keyboard input after modal teardown. */
        _click(button);
        _click(evas_object_data_get(ui.app_menu, "settings"));
        assert(ui.connection.popup);
        _type_in(ui.connection.host);
        tabs = evas_object_data_get(ui.connection.popup, "pages");
        evas_object_geometry_get(tabs, &tx, &ty, &tw, &th);
        _click_at(evas_object_evas_get(tabs), tx + tw * 3 / 4, ty + th / 2);
        assert(ui.connection.options_visible);
        _close_and_wait(ui.connection.popup, NULL);
        assert(!ui.connection.popup);
        _click(button);
        _click(evas_object_data_get(ui.app_menu, "about"));
        assert(ui.about);
        _snapshot(&ui, artifacts, i, "about");
        tabs = evas_object_data_get(ui.about, "pages");
        int x, y, w, h;
        evas_object_geometry_get(tabs, &x, &y, &w, &h);
        assert(h >= ELM_SCALE_SIZE(48));
        _click_at(evas_object_evas_get(tabs), x + w * 5 / 6, y + h / 2);
        assert(strstr(elm_entry_entry_get(evas_object_data_get(ui.about, "text")),
                      "GNU GENERAL PUBLIC LICENSE"));
        _close_and_wait(ui.about, evas_object_data_get(ui.about, "close"));
        assert(!ui.about);
        _click(button);
     }
   for (int j = 0; j < 3; j++) elm_genlist_item_class_free(classes[j]);
   test_canvas = NULL;
   ui.current = NULL;
   eina_strbuf_free(conversation.history);
   evas_object_del(ui.win);
   assert(!ui.theme_icons);
   _theme_shutdown(&ui);
   elaro_config_free(ui.config);
   elm_shutdown();
   return 0;
}
