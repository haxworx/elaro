#include "elaro_ui_private.h"

/* Prefixes are separate inline textblocks with an exact width. Their glyph
 * advances never affect the message column, including with proportional fonts. */
static Evas_Object *
_history_textblock(Evas_Object *history)
{
   Evas_Object *source = elm_entry_textblock_get(history);
   Evas_Object *text = evas_object_textblock_add(evas_object_evas_get(history));
   const Evas_Textblock_Style *source_style;
   Evas_Textblock_Style *style;

   /* Edje owns and can replace these styles on a later theme/cache update.
    * Give inline objects independent snapshots, including user overrides. */
   source_style = evas_object_textblock_style_get(source);
   if (source_style)
     {
        style = evas_textblock_style_new();
        evas_textblock_style_set(style, evas_textblock_style_get(source_style));
        evas_object_textblock_style_set(text, style);
        evas_textblock_style_free(style);
     }
   source_style = evas_object_textblock_style_user_peek(source);
   if (source_style)
     {
        style = evas_textblock_style_new();
        evas_textblock_style_set(style, evas_textblock_style_get(source_style));
        evas_object_textblock_style_user_push(text, style);
        evas_textblock_style_free(style);
     }
   evas_object_scale_set(text, evas_object_scale_get(source));
   return text;
}

/* Encode the prefix in the item itself so retained history and selections do
 * not depend on pointers or a separate lifetime-sensitive lookup table. */
static char *
_history_prefix_decode(const char *encoded, size_t length)
{
   char *result;
   size_t i;

   if (length % 2) return NULL;
   result = malloc(length / 2 + 1);
   if (!result) return NULL;
   for (i = 0; i < length; i += 2)
     {
        const char *digits = "0123456789abcdef";
        const char *a = strchr(digits, encoded[i]);
        const char *b = strchr(digits, encoded[i + 1]);
        if (!a || !b || (!encoded[i] || !encoded[i + 1])
            || (a == digits && b == digits))
          {
             free(result);
             return NULL;
          }
        result[i / 2] = ((a - digits) << 4) | (b - digits);
     }
   result[length / 2] = 0;
   return result;
}

static Evas_Object *
_history_prefix_item(void *data, Evas_Object *history, const char *item)
{
   Evas_Object *text;
   Evas_Textblock_Style *style;
   const Evas_Textblock_Style *font_style;
   const char *font, *end;
   char *prefix, *format;
   Eina_Bool timestamp;

   if (strncmp(item, "chat-prefix:", 12)) return NULL;
   if (strstr(item + 12, ":repeat"))
     {
        text = evas_object_rectangle_add(evas_object_evas_get(history));
        evas_object_color_set(text, 0, 0, 0, 0);
        evas_object_pass_events_set(text, EINA_TRUE);
        return text;
     }
   prefix = _history_prefix_decode(item + 12, strlen(item + 12));
   if (!prefix) return NULL;
   timestamp = prefix[0] == '[';
   /* The stored trailing space belongs to copied text, not visual alignment. */
   size_t length = strlen(prefix);
   if (length && prefix[length - 1] == ' ') prefix[length - 1] = 0;
   text = _history_textblock(history);
   style = evas_textblock_style_new();
   /* The user style can be DEFAULT='' or a partial override. Use the resolved
    * base style so inline cells always have a font and size of their own. */
   font_style = evas_object_textblock_style_get(elm_entry_textblock_get(history));
   font = font_style ? evas_textblock_style_get(font_style) : "DEFAULT=''";
   if (!font) font = "DEFAULT=''";
   end = strstr(font, "DEFAULT='");
   if (end) {
      end += 9;
      while (*end && (*end != '\'' || (end > font && end[-1] == '\\'))) end++;
   }
   /* Keep the entry's resolved foreground. The desktop's global color class
    * can change independently and must not make just the prefixes invisible. */
   format = elaro_string_printf("%.*s wrap=none ellipsis=1.0 align=%s left_margin=0 right_margin=%d'%s",
                                end ? (int)(end - font) : 9, font,
                                timestamp ? "left" : "right", timestamp ? 0 : ELM_SCALE_SIZE(8),
                                end && *end ? end + 1 : "");
   evas_textblock_style_set(style, format);
   free(format);
   evas_object_textblock_style_user_push(text, style);
   evas_textblock_style_free(style);
   evas_object_textblock_text_markup_set(text, prefix);
   evas_object_pass_events_set(text, EINA_TRUE);
   free(prefix);
   return text;
}

static void
_history_cell_append(Eina_Strbuf *out, const char *prefix, size_t length,
                     Evas_Coord width, Evas_Coord height, Eina_Bool repeated)
{
   size_t i;

   eina_strbuf_append_printf(out, "<item absize=%dx%d vsize=full href=chat-prefix:", width, height);
   for (i = 0; i < length; i++)
      eina_strbuf_append_printf(out, "%02x", (unsigned char)prefix[i]);
   eina_strbuf_append(out, repeated ? ":repeat></item>" : "></item>");
}

static char *
_history_columns(Evas_Object *history, const char *markup, Eina_Bool reset)
{
   Evas_Object *measure;
   Evas_Coord width, gutter, height, timestamp_width;
   Evas_Coord *saved_gutter = evas_object_data_get(history, "history-gutter");
   Eina_Strbuf *out;
   const char *line, *end, *body, *nick_end, *nickname;
   char *result, *author = evas_object_data_del(history, "history-author");
   Eina_Bool repeated;

   if (reset)
     {
        free(author);
        author = NULL;
     }
   measure = _history_textblock(history);
   evas_object_textblock_text_markup_set(measure, "[00:00] &lt;nickname123&gt;  ");
   evas_object_textblock_size_native_get(measure, &gutter, &height);
   evas_object_textblock_text_markup_set(measure, "[00:00] ");
   evas_object_textblock_size_native_get(measure, &timestamp_width, NULL);
   /* Fit actual nicknames, including proportional fonts and IRC markup. Keep
    * the existing width on append so every message uses the same columns. */
   if (!reset && saved_gutter && *saved_gutter > gutter) gutter = *saved_gutter;
   for (line = markup; *line; line = end + 5)
     {
        end = strstr(line, "<br/>");
        if (!end) end = line + strlen(line);
        if (end - line >= 12 && line[0] == '[' && line[6] == ']' && line[7] == ' '
            && !strncmp(line + 8, "&lt;", 4))
          {
             nick_end = strstr(line + 12, "&gt; ");
             if (nick_end && nick_end + 5 <= end)
               {
                  Evas_Coord nickname_width;
                  char *nick = strndup(line + 8, nick_end + 4 - (line + 8));

                  evas_object_textblock_text_markup_set(measure, nick);
                  evas_object_textblock_size_native_get(measure, &nickname_width, NULL);
                  free(nick);
                  nickname_width += timestamp_width + ELM_SCALE_SIZE(8);
                  if (nickname_width > gutter) gutter = nickname_width;
               }
          }
        if (!*end) break;
     }
   evas_object_del(measure);
   evas_object_geometry_get(history, NULL, NULL, &width, NULL);
   /* Before the first layout the entry can be only one pixel wide. Do not
    * bake zero-width inline items into the history at that provisional size.
    * Wait until the available gutter can hold a timestamp and nickname gap;
    * the resize reflow will apply the normal limit once layout is ready. */
   if (width * 0.45 > timestamp_width + ELM_SCALE_SIZE(8)
       && gutter > width * 0.45)
      gutter = width * 0.45;
   if (timestamp_width > gutter) timestamp_width = gutter;
   if (saved_gutter) *saved_gutter = gutter;
   out = eina_strbuf_new();
   if (!out) goto allocation_failed;
   for (line = markup; *line; line = end + 5)
     {
        end = strstr(line, "<br/>");
        if (!end) end = line + strlen(line);
        nickname = body = line;
        if (end - line >= 8 && line[0] == '[' && line[6] == ']' && line[7] == ' ')
          {
             nickname = body = line + 8;
             if (!strncmp(body, "&lt;", 4))
               {
                  nick_end = strstr(body + 4, "&gt; ");
                  if (nick_end && nick_end < end) body = nick_end + 5;
               }
          }
        repeated = author && body > nickname && strlen(author) == (size_t)(body - nickname)
                   && !strncmp(author, nickname, body - nickname);
        free(author);
        author = body > nickname ? strndup(nickname, body - nickname) : NULL;
        if (body == nickname)
           _history_cell_append(out, line, body - line, gutter, height, EINA_FALSE);
        else
          {
             _history_cell_append(out, line, nickname - line, timestamp_width, height, EINA_FALSE);
             _history_cell_append(out, nickname, body - nickname, gutter - timestamp_width, height,
                                  repeated);
          }
        eina_strbuf_append_printf(out, "<left_margin=+%d>", gutter);
        eina_strbuf_append_length(out, body, end - body);
        eina_strbuf_append(out, "</left_margin><ps/>");
        if (!*end) break;
     }
   if (author) evas_object_data_set(history, "history-author", author);
   result = eina_strbuf_string_steal(out);
   eina_strbuf_free(out);
   return result;

allocation_failed:
   free(author);
   return NULL;
}

static void
_history_refresh(Elaro_Ui *ui, Eina_Bool preserve_scroll)
{
   char *markup;
   Evas_Coord y, h, content_h;
   double *position;

   if (!ui->history || !ui->current) return;
   if (!preserve_scroll)
     {
        Ecore_Job *job = evas_object_data_del(ui->history, "history-position-job");
        if (job) ecore_job_del(job);
        free(evas_object_data_del(ui->history, "history-position"));
        /* Switching conversations must follow the latest message even if a
         * resize/reflow arrives before Elementary finishes loading the text. */
        position = malloc(sizeof(*position));
        if (!position) return;
        *position = 1.0;
        evas_object_data_set(ui->history, "history-position", position);
     }
   if (preserve_scroll && !evas_object_data_get(ui->history, "history-position"))
     {
        elm_scroller_region_get(ui->history, NULL, &y, NULL, &h);
        elm_scroller_child_size_get(ui->history, NULL, &content_h);
        if (content_h > h && y + h < content_h - ELM_SCALE_SIZE(4))
          {
             position = malloc(sizeof(*position));
             if (!position) return;
             *position = (double)y / (content_h - h);
             evas_object_data_set(ui->history, "history-position", position);
          }
     }
   evas_object_data_set(ui->history, "history-ui", ui);
   markup = _history_columns(ui->history, eina_strbuf_string_get(ui->current->history), EINA_TRUE);
   /* Edje reuses inline objects with identical item names. Drop those cells
    * before a style reflow so they cannot retain the previous theme's color. */
   if (evas_object_data_del(ui->history, "history-style-dirty"))
      elm_entry_entry_set(ui->history, "");
   elm_entry_entry_set(ui->history, markup);
   free(markup);
}

static void
_history_reflow(void *data)
{
   Evas_Object *history = data;
   Elaro_Ui *ui = evas_object_data_get(history, "history-ui");

   evas_object_data_del(history, "history-reflow");
   if (ui) _history_refresh(ui, EINA_TRUE);
}

static void
_history_resize(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   if (!evas_object_data_get(obj, "history-reflow"))
      evas_object_data_set(obj, "history-reflow", ecore_job_add(_history_reflow, obj));
}

static void
_history_font_changed(void *data, Evas_Object *obj, void *info)
{
   evas_object_data_set(obj, "history-style-dirty", (void *)1);
   _history_resize(NULL, NULL, obj, NULL);
}

/* Restore real text when copying an inline prefix, including selections that
 * span several messages. Run after Elementary has stored its default markup. */
static void
_history_copy(void *data)
{
   Evas_Object *history = data;
   const char *p, *start, *end, *close;
   char *selection;
   Eina_Strbuf *markup;
   char *prefix, *plain = NULL;

   evas_object_data_del(history, "history-copy-job");
   selection = evas_object_data_del(history, "history-copy-selection");
   if (!selection) return;
   markup = eina_strbuf_new();
   if (!markup) goto done;
   for (p = selection; (start = strstr(p, "<item ")); p = close + 7)
     {
        end = strstr(start, "chat-prefix:");
        close = strstr(start, "</item>");
        if (!end || !close || end > close) break;
        end += 12;
        size_t length = strspn(end, "0123456789abcdef");
        prefix = _history_prefix_decode(end, length);
        if (!prefix) break;
        eina_strbuf_append_length(markup, p, start - p);
        eina_strbuf_append(markup, prefix);
        free(prefix);
     }
   eina_strbuf_append(markup, p);
   eina_strbuf_replace_all(markup, "<ps/>", "<br/>");
   plain = elm_entry_markup_to_utf8(eina_strbuf_string_get(markup));
   if (!plain) goto done;
   elm_cnp_selection_set(history, ELM_SEL_TYPE_CLIPBOARD, ELM_SEL_FORMAT_TEXT, plain, strlen(plain) + 1);
done:
   free(selection);
   free(plain);
   if (markup) eina_strbuf_free(markup);
}

static void
_history_copy_requested(void *data, Evas_Object *obj, void *info)
{
   const char *selection = elm_entry_selection_get(obj);
   char *copy;

   /* Snapshot before Elementary updates clipboard ownership, which can clear
    * the live selection before the deferred clipboard conversion runs. */
   free(evas_object_data_del(obj, "history-copy-selection"));
   if (!selection) return;
   copy = strdup(selection);
   if (!copy) return;
   evas_object_data_set(obj, "history-copy-selection", copy);
   if (!evas_object_data_get(obj, "history-copy-job"))
      evas_object_data_set(obj, "history-copy-job", ecore_job_add(_history_copy, obj));
}

static void
_history_del(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   Ecore_Job *job = evas_object_data_get(obj, "history-reflow");
   if (job) ecore_job_del(job);
   job = evas_object_data_get(obj, "history-copy-job");
   if (job) ecore_job_del(job);
   job = evas_object_data_get(obj, "history-position-job");
   if (job) ecore_job_del(job);
   free(evas_object_data_del(obj, "history-position"));
   free(evas_object_data_del(obj, "history-author"));
   free(evas_object_data_del(obj, "history-gutter"));
   free(evas_object_data_del(obj, "history-copy-selection"));
}

static void
_history_position_restore(void *data)
{
   Evas_Object *obj = data;
   double *position = evas_object_data_del(obj, "history-position");
   Evas_Coord h, content_h;

   evas_object_data_del(obj, "history-position-job");
   if (!position) return;
   if (*position >= 1.0) elm_entry_cursor_end_set(obj);
   elm_entry_calc_force(obj);
   elm_scroller_region_get(obj, NULL, NULL, NULL, &h);
   elm_scroller_child_size_get(obj, NULL, &content_h);
   elm_scroller_region_show(obj, 0, *position * (content_h > h ? content_h - h : 0), 1, h);
   free(position);
}

static void
_history_loaded(void *data, Evas_Object *obj, void *info)
{
   /* Restore the viewport after Elementary has processed the loaded entry's
    * cursor and size changes, which otherwise override a synchronous scroll. */
   if (evas_object_data_get(obj, "history-position"))
     {
        if (!evas_object_data_get(obj, "history-position-job"))
           evas_object_data_set(obj, "history-position-job",
                                ecore_job_add(_history_position_restore, obj));
     }
   else elm_entry_cursor_end_set(obj);
}

/* Read-only entries skip Edje's editable-entry keyboard handler. Bind copy
 * keys only while this history owns a selection, using Evas's native key API. */
static void
_history_copy_keys_clear(void *data, Evas_Object *obj, void *info)
{
   Evas_Modifier_Mask control = evas_key_modifier_mask_get(evas_object_evas_get(obj), "Control");

   if (!evas_object_data_del(obj, "history-copy-keys")) return;
   evas_object_key_ungrab(obj, "c", control, 0);
   evas_object_key_ungrab(obj, "C", control, 0);
   evas_object_key_ungrab(obj, "Insert", control, 0);
}

static void
_history_copy_keys_set(void *data, Evas_Object *obj, void *info)
{
   Evas_Modifier_Mask control = evas_key_modifier_mask_get(evas_object_evas_get(obj), "Control");

   if (evas_object_data_get(obj, "history-copy-keys") || !elm_entry_selection_get(obj)) return;
   Eina_Bool grabbed = evas_object_key_grab(obj, "c", control, 0, EINA_FALSE);
   grabbed |= evas_object_key_grab(obj, "C", control, 0, EINA_FALSE);
   grabbed |= evas_object_key_grab(obj, "Insert", control, 0, EINA_FALSE);
   if (grabbed) evas_object_data_set(obj, "history-copy-keys", (void *)1);
}

static void
_history_key_down(void *data, Evas *evas, Evas_Object *obj, void *event)
{
   Evas_Event_Key_Down *ev = event;

   if (!ev->key || (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD)
       || !evas_key_modifier_is_set(ev->modifiers, "Control")
       || evas_key_modifier_is_set(ev->modifiers, "Alt")
       || !elm_entry_selection_get(obj)) return;
   if (strcasecmp(ev->key, "c") && strcmp(ev->key, "Insert")) return;
   elm_entry_selection_copy(obj);
   ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;
}

static void
_history_copy_all(void *data, Evas_Object *obj, void *info)
{
   Evas_Object *history = data;

   elm_entry_select_all(history);
   elm_entry_selection_copy(history);
}

Evas_Object *
elaro_ui_history_add(Evas_Object *parent)
{
   Evas_Object *history;
   Evas_Coord *gutter = calloc(1, sizeof(*gutter));

   if (!gutter) return NULL;
   history = elaro_ui_read_view_add(parent);
   elm_object_style_set(history, "elaro/history");
   evas_object_data_set(history, "history-gutter", gutter);
   elm_entry_line_wrap_set(history, ELM_WRAP_MIXED);
   elm_entry_select_allow_set(history, EINA_TRUE);
   elm_entry_context_menu_disabled_set(history, EINA_FALSE);
   /* Keep a useful native context menu even before any text is selected. */
   elm_entry_context_menu_item_add(history, _("Copy all"), NULL, ELM_ICON_NONE,
                                   _history_copy_all, history);
   evas_object_event_callback_add(history, EVAS_CALLBACK_KEY_DOWN, _history_key_down, NULL);
   evas_object_smart_callback_add(history, "selection,changed", _history_copy_keys_set, NULL);
   evas_object_smart_callback_add(history, "selection,cleared", _history_copy_keys_clear, NULL);
   elm_scroller_gravity_set(history, 0.0, 1.0);
   evas_object_smart_callback_add(history, "text,set,done", _history_loaded, NULL);
   elm_entry_item_provider_prepend(history, _history_prefix_item, NULL);
   evas_object_event_callback_add(history, EVAS_CALLBACK_RESIZE, _history_resize, NULL);
   evas_object_event_callback_add(history, EVAS_CALLBACK_DEL, _history_del, NULL);
   /* Inline cells own copied text styles; rebuild them after toolkit re-themes
    * as well as font edits, including window opacity and desktop changes. */
   evas_object_smart_callback_add(history, "theme,changed", _history_font_changed, NULL);
   evas_object_smart_callback_add(history, "selection,copy", _history_copy_requested, NULL);
   return history;
}

void
elaro_ui_history_refresh(Elaro_Ui *ui)
{
   _history_refresh(ui, EINA_FALSE);
}

void
elaro_ui_history_append(Elaro_Ui *ui, const char *markup)
{
   Evas_Coord *gutter = evas_object_data_get(ui->history, "history-gutter");
   Evas_Coord previous_gutter = gutter ? *gutter : 0;
   char *columns = _history_columns(ui->history, markup, EINA_FALSE);

   /* The retained buffer already includes this message. Reflow older rows
    * when a wider nickname moves the message column. */
   if (gutter && *gutter != previous_gutter)
      _history_refresh(ui, EINA_TRUE);
   else elm_entry_entry_append(ui->history, columns);
   free(columns);
}
