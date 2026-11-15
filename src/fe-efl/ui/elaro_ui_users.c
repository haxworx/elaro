#include "elaro_ui_private.h"
#include <ctype.h>

typedef struct
{
   char *nick, *realname, *email;
   Elaro_Ui *app;
   Evas_Object *preview;
   Eina_Bool lookup_requested;
   Ecore_Timer *lookup_timer;
   char status;
   Eina_Bool seen, away;
   Elm_Object_Item *item;
} User;

typedef struct
{
   Elaro_Ui *app;
   char *server, *nick, *channel;
} User_Menu;

typedef struct
{
   const char *label, *command;
   Eina_Bool private_reply;
   Eina_Bool channel_action, send_file;
} User_Action;

static const User_Action user_actions[] = {
   /* Conversation and file transfer. NULL labels separate action groups. */
   { N_("Private conversation"), NULL, EINA_TRUE },
   { N_("Send file…"), NULL, EINA_TRUE, EINA_FALSE, EINA_TRUE },
   { NULL },
   /* Channel permissions. */
   { N_("Op"), "/op %s", EINA_FALSE, EINA_TRUE },
   { N_("Deop"), "/deop %s", EINA_FALSE, EINA_TRUE },
   { N_("Voice"), "/voice %s", EINA_FALSE, EINA_TRUE },
   { N_("Devoice"), "/devoice %s", EINA_FALSE, EINA_TRUE },
   { NULL },
   /* Channel moderation. */
   { N_("Kick"), "/kick %s", EINA_FALSE, EINA_TRUE },
   { N_("Ban"), "/ban %s", EINA_FALSE, EINA_TRUE },
   { N_("Kick and ban"), "/kickban %s", EINA_FALSE, EINA_TRUE },
   { NULL },
   /* Server queries about the user. */
   { N_("WHOIS"), "/whois %s", EINA_FALSE },
   { N_("WHOWAS"), "/whowas %s", EINA_FALSE },
   { N_("WHO"), "/who %s", EINA_FALSE },
   { N_("USERHOST"), "/userhost %s", EINA_FALSE },
   { NULL },
   /* Client-to-client queries. */
   { N_("CTCP VERSION"), "/ctcp %s VERSION", EINA_TRUE },
   { N_("CTCP TIME"), "/ctcp %s TIME", EINA_TRUE },
   { N_("CTCP PING"), "/ping %s", EINA_TRUE },
   { N_("CTCP CLIENTINFO"), "/ctcp %s CLIENTINFO", EINA_TRUE },
   { N_("CTCP FINGER"), "/ctcp %s FINGER", EINA_TRUE },
};
static void
_user_selected_cb(void *data, Evas_Object *obj, void *info);

static void
_request_query(Elaro_Ui *ui, const char *server, const char *nick)
{
   Elaro_Engine_Command *c;

   c = calloc(1, sizeof(Elaro_Engine_Command));
   if (!c) goto allocation_failed;
   c->type = ELARO_ENGINE_QUERY;
   c->server = strdup(server);
   c->target = strdup(nick);
   if (!c->server || !c->target) goto allocation_failed;

   elaro_engine_submit(ui->engine, c);
   return;

allocation_failed:
   elaro_engine_command_free(c);
}

static char *
_user_text(void *data, Evas_Object *obj, const char *part)
{
   User *u;

   u = data;

   return elm_entry_utf8_to_markup(u->nick);
}

static Evas_Object *
_user_icon(void *data, Evas_Object *obj, const char *part)
{
   User *u;
   const char *name, *role;
   Evas_Object *icon;

   u = data;

   if (!strcmp(part, "elm.swallow.icon"))
     {
        name = u->status == '@'   ? "emblem-important"
               : u->status == '+' ? "audio-input-microphone"
                                  : "user-available";
        role = u->status == '@'   ? _("Channel operator")
               : u->status == '+' ? _("Voice")
                                  : _("User");
     }
   else if (!strcmp(part, "elm.swallow.end") && u->away)
     {
        name = "user-idle";
        role = _("Away");
     }
   else return NULL;

   icon = elm_icon_add(obj);

   if (!elaro_ui_icon_set(u->app, icon, name))
     {
        evas_object_del(icon);

        icon = elm_label_add(obj);
        elm_object_text_set(icon, !strcmp(part, "elm.swallow.end") ? "Zz"
                                  : u->status == '@'               ? "@"
                                  : u->status == '+'               ? "+"
                                                                   : "•");
     }
   elaro_ui_size_min_set(icon, 20, 20);
   elaro_ui_size_max_set(icon, 20, 20);
   elm_object_tooltip_text_set(icon, role);

   return icon;
}

static void
_user_free(void *data, Evas_Object *obj)
{
   User *u;

   u = data;
   if (u->preview) evas_object_del(u->preview);
   if (u->lookup_timer) ecore_timer_del(u->lookup_timer);
   free(u->nick);
   free(u->realname);
   free(u->email);
   free(u);
}

static int
_user_compare(const void *left, const void *right)
{
   const User *first, *second;
   int a_rank, b_rank, order;

   first = elm_object_item_data_get(left);
   second = elm_object_item_data_get(right);
   a_rank = first->status == '@' ? 0 : first->status == '+' ? 1 : 2;
   b_rank = second->status == '@' ? 0 : second->status == '+' ? 1 : 2;

   if (a_rank != b_rank) return a_rank - b_rank;

   order = strcasecmp(first->nick, second->nick);

   return order ? order : strcmp(first->nick, second->nick);
}

static void
_completion_reset(Elaro_Ui *ui)
{
   char *nick;

   EINA_LIST_FREE (ui->completion.matches, nick)
      free(nick);
   free(ui->completion.before);
   free(ui->completion.after);
   free(ui->completion.text);
   memset(&ui->completion, 0, sizeof(ui->completion));
}

static int
_completion_compare(const void *a, const void *b)
{
   int order;

   order = strcasecmp(a, b);
   return order ? order : strcmp(a, b);
}

Eina_Bool
elaro_ui_users_complete(void *data, Evas_Object *obj, Evas_Event_Key_Down *ev)
{
   Elaro_Ui *ui;
   Eina_Iterator *iter;
   User *u;
   char *text, *end, *start, *prefix, *markup, *result;
   const char *nick, *separator;
   int cursor, offset, i;

   ui = data;
   if (strcmp(ev->key, "Tab") || evas_key_modifier_is_set(ev->modifiers, "Shift")
       || evas_key_modifier_is_set(ev->modifiers, "Control")
       || evas_key_modifier_is_set(ev->modifiers, "Alt"))
     {
        _completion_reset(ui);
        return EINA_FALSE;
     }
   if (!ui->current || !ui->current->channel) return EINA_FALSE;

   /* Consume Tab even without a match, keeping focus in the channel entry. */
   if (elm_entry_selection_get(obj)) return EINA_TRUE;
   text = elm_entry_markup_to_utf8(elm_entry_entry_get(obj));
   if (!text) return EINA_TRUE;
   cursor = elm_entry_cursor_pos_get(obj);
   if (!ui->completion.text || strcmp(text, ui->completion.text) || cursor != ui->completion.cursor)
     {
        _completion_reset(ui);
        /* Elementary cursor positions count Unicode characters, not bytes. */
        offset = 0;
        for (i = 0; i < cursor && text[offset]; i++)
           eina_unicode_utf8_next_get(text, &offset);
        end = text + offset;
        start = end;
        while (start > text && !isspace((unsigned char)start[-1]))
           start--;
        if (start == end)
          {
             free(text);
             return EINA_TRUE;
          }
        prefix = strndup(start, end - start);
        ui->completion.before = strndup(text, start - text);
        ui->completion.after = strdup(end);
        if (!prefix || !ui->completion.before || !ui->completion.after) goto allocation_failed;
        iter = eina_hash_iterator_data_new(ui->users);
        EINA_ITERATOR_FOREACH (iter, u)
           if (!strncasecmp(u->nick, prefix, strlen(prefix)))
             {
                char *match = strdup(u->nick);
                if (match)
                   ui->completion.matches = eina_list_sorted_insert(
                       ui->completion.matches, _completion_compare, match);
             }
        eina_iterator_free(iter);
        free(prefix);
        ui->completion.next = ui->completion.matches;
     }
   free(text);
   if (!ui->completion.next) return EINA_TRUE;
   nick = eina_list_data_get(ui->completion.next);
   separator = *ui->completion.before ? "" : ": ";
   result = elaro_string_printf("%s%s%s%s", ui->completion.before, nick, separator,
                                ui->completion.after);
   if (!result) return EINA_TRUE;
   markup = elm_entry_utf8_to_markup(result);
   elm_entry_entry_set(obj, markup);
   ui->completion.cursor = eina_unicode_utf8_get_len(ui->completion.before)
                           + eina_unicode_utf8_get_len(nick) + strlen(separator);
   elm_entry_cursor_pos_set(obj, ui->completion.cursor);
   free(markup);
   free(ui->completion.text);
   ui->completion.text = result;
   ui->completion.next = eina_list_next(ui->completion.next);
   if (!ui->completion.next) ui->completion.next = ui->completion.matches;
   return EINA_TRUE;

allocation_failed:
   free(prefix);
   free(text);
   _completion_reset(ui);
   return EINA_TRUE;
}

static void
_preview_deleted(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   User *u;

   u = data;
   u->preview = NULL;
}

static void
_preview_update(User *u)
{
   Evas_Object *label, *image;
   char *safe, *text;

   if (!u->preview) return;
   label = evas_object_data_get(u->preview, "label");
   image = evas_object_data_get(u->preview, "image");
   safe = elm_entry_utf8_to_markup(u->realname && *u->realname ? u->realname : u->nick);
   text = elaro_string_printf("<b>%s</b><br/>%s", safe,
                              !u->realname ? _("Looking up public profile…")
                              : !u->email  ? _("No public email shared")
                                           : _("Gravatar"));
   elm_object_text_set(label, text);
   free(text);
   free(safe);
   if (u->email) elaro_ui_avatar_load(u->app, u->email, image);
}

static Eina_Bool
_lookup_timeout(void *data)
{
   User *u;

   u = data;
   u->lookup_timer = NULL;
   free(u->realname);
   u->realname = strdup("");
   _preview_update(u);
   return ECORE_CALLBACK_CANCEL;
}

static Evas_Object *
_user_tooltip(void *data, Evas_Object *obj, Evas_Object *tooltip, void *item)
{
   User *u;
   Evas_Object *box, *image, *label;
   Elaro_Engine_Command *c;

   u = data;
   if (u->preview)
     {
        image = evas_object_data_get(u->preview, "image");
        return evas_object_data_get(image, "avatar-loaded") ? u->preview : NULL;
     }
   /* Keep the pending lookup outside the tooltip's lifetime. Returning NULL
    * suppresses the popup until a real avatar has successfully loaded. */
   box = elm_box_add(obj);
   u->preview = box;
   evas_object_event_callback_add(box, EVAS_CALLBACK_DEL, _preview_deleted, u);
   elaro_ui_box_padding_set(box, 0, 6);
   image = elm_icon_add(box);
   elaro_ui_avatar_placeholder_set(image);
   elaro_ui_size_min_set(image, 96, 96);
   elm_box_pack_end(box, image);
   evas_object_show(image);
   label = elm_label_add(box);
   elm_label_line_wrap_set(label, ELM_WRAP_WORD);
   elaro_ui_wrap_width_set(label, 240);
   elm_box_pack_end(box, label);
   evas_object_show(label);
   evas_object_data_set(box, "label", label);
   evas_object_data_set(box, "image", image);
   if (!u->lookup_requested && !u->realname && u->app->current)
     {
        c = calloc(1, sizeof(Elaro_Engine_Command));
        if (!c) goto allocation_failed;
        c->type = ELARO_ENGINE_USER_INFO;
        c->server = strdup(u->app->current->server);
        c->target = strdup(u->nick);
        if (!c->server || !c->target) goto allocation_failed;

        u->lookup_requested = EINA_TRUE;
        elaro_engine_submit(u->app->engine, c);
        u->lookup_timer = ecore_timer_add(10, _lookup_timeout, u);
     }
   _preview_update(u);
   return NULL;

allocation_failed:
   elaro_engine_command_free(c);
   return NULL;
}

void
elaro_ui_users_details(Elaro_Ui *ui, const Elaro_Engine_Event *event)
{
   User *u;
   Eina_Iterator *iter;

   if (!ui->current || strcasecmp(ui->current->server, event->server)) return;
   iter = eina_hash_iterator_data_new(ui->users);
   EINA_ITERATOR_FOREACH (iter, u)
      if (!strcasecmp(u->nick, event->target))
        {
           if (u->lookup_timer) ecore_timer_del(u->lookup_timer);
           u->lookup_timer = NULL;
           free(u->realname);
           free(u->email);
           u->realname = strdup(event->text);
           u->email = elaro_email_from_realname(event->text);
           _preview_update(u);
           break;
        }
   eina_iterator_free(iter);
}

void
elaro_ui_users_save(Elaro_Ui *ui)
{
   Elm_Object_Item *item;
   Evas_Coord x, y, w;

   /* A switch before the snapshot arrives must retain the saved position. */
   if (!ui->current || ui->user_scroll_restore) return;
   evas_object_geometry_get(ui->nicks, &x, &y, &w, NULL);
   item = elm_genlist_at_xy_item_get(ui->nicks, x + w / 2, y + 1, NULL);
   if (item) ui->current->user_scroll_index = elm_genlist_item_index_get(item);
}

void
elaro_ui_users_clear(Elaro_Ui *ui)
{
   _completion_reset(ui);
   if (ui->user_menu) evas_object_del(ui->user_menu);
   /* The genlist owns User records; the hash only indexes them. */
   eina_hash_free_buckets(ui->users);
   elm_genlist_clear(ui->nicks);
}

void
elaro_ui_users_presence(Elaro_Ui *ui, const Elaro_Engine_Event *event)
{
   Eina_Iterator *iter;
   User *u;

   if (!ui->current || strcasecmp(ui->current->server, event->server)) return;
   iter = eina_hash_iterator_data_new(ui->users);
   EINA_ITERATOR_FOREACH (iter, u)
      if (!*event->target || !strcasecmp(u->nick, event->target))
        {
           Eina_Bool away = event->type == ELARO_ENGINE_USER_PRESENCE && event->away;
           if (u->away != away)
             {
                u->away = away;
                elm_genlist_item_update(u->item);
             }
        }
   eina_iterator_free(iter);
}

void
elaro_ui_users_update(Elaro_Ui *ui, const Elaro_Engine_User *users, unsigned int count)
{
   User *u;
   Eina_Iterator *iter;
   Eina_List *removed = NULL;
   const char *nick;
   char status;

   _completion_reset(ui);
   iter = eina_hash_iterator_data_new(ui->users);
   EINA_ITERATOR_FOREACH (iter, u)
      u->seen = EINA_FALSE;
   eina_iterator_free(iter);
   for (unsigned int i = 0; i < count; i++)
     {

        nick = users[i].nick;
        status = users[i].prefix;

        if (!*nick) continue;

        u = eina_hash_find(ui->users, nick);

        if (u && u->status != status)
          {
             /* Reinsert a changed role in its new sort group. */
             eina_hash_del_by_key(ui->users, nick);
             elm_object_item_del(u->item);
             u = NULL;
          }

        if (!u)
          {
             u = calloc(1, sizeof(User));
             if (!u) return;
             u->nick = strdup(nick);
             if (!u->nick) goto allocation_failed;
             u->app = ui;
             u->status = status;
             u->away = users[i].away;
             u->item = elm_genlist_item_sorted_insert(ui->nicks, ui->user_class, u, NULL,
                                                      ELM_GENLIST_ITEM_NONE, _user_compare,
                                                      _user_selected_cb, ui);
             eina_hash_add(ui->users, u->nick, u);
             elm_genlist_item_tooltip_content_cb_set(u->item, _user_tooltip, u, NULL);
          }
        if (u->away != users[i].away)
          {
             u->away = users[i].away;
             elm_genlist_item_update(u->item);
          }
        u->seen = EINA_TRUE;
     }

   /* Collect removals before modifying the hash underneath its iterator. */
   iter = eina_hash_iterator_data_new(ui->users);
   EINA_ITERATOR_FOREACH (iter, u)
      if (!u->seen) removed = eina_list_append(removed, u);
   eina_iterator_free(iter);
   EINA_LIST_FREE (removed, u)
     {
        eina_hash_del_by_key(ui->users, u->nick);
        elm_object_item_del(u->item);
     }
   if (ui->current && ui->current->channel)
     {
        ui->current->user_count = eina_hash_population(ui->users);
        ui->current->user_count_known = EINA_TRUE;
        elaro_ui_buffer_title_update(ui);
     }
   if (ui->user_scroll_restore)
     {
        Elm_Object_Item *item = elm_genlist_first_item_get(ui->nicks);
        unsigned int index = ui->current ? ui->current->user_scroll_index : 0;

        /* Item-show defers scrolling until genlist has laid out the new rows.
         * Clamp a saved position when the channel has become smaller. */
        for (unsigned int i = 1; item && i < index; i++)
          {
             Elm_Object_Item *next = elm_genlist_item_next_get(item);
             if (!next) break;
             item = next;
          }
        if (item) elm_genlist_item_show(item, ELM_GENLIST_ITEM_SCROLLTO_TOP);
        ui->user_scroll_restore = EINA_FALSE;
     }
   return;

allocation_failed:
   free(u);
}

static void
_user_selected_cb(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui;
   const User *u;

   ui = data;
   u = elm_object_item_data_get(info);

   if (ui->current && u) _request_query(ui, ui->current->server, u->nick);
}

static void
_user_menu_deleted_cb(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   User_Menu *menu;

   menu = data;

   if (menu->app->user_menu == obj) menu->app->user_menu = NULL;
   free(menu->server);
   free(menu->nick);
   free(menu->channel);
   free(menu);
}

static void
_user_menu_dismissed_cb(void *data, Evas_Object *obj, void *info)
{
   evas_object_del(obj);
}

static void
_user_action_clicked_cb(void *data, Evas_Object *obj, void *info)
{
   const User_Action *action;
   User_Menu *menu;
   Elaro_Ui *ui;
   char *server, *target;
   Elaro_Engine_Command *c = NULL;
   Elaro_Ui_Buffer *b;

   action = data;
   menu = evas_object_data_get(obj, "user-menu");
   ui = menu->app;
   /* Copy the destination before changing buffers, which closes the menu. */
   server = strdup(menu->server);
   target
       = strdup(action->channel_action ? menu->channel : (action->private_reply ? menu->nick : ""));

   if (!server || !target) goto allocation_failed;

   if (action->send_file)
     {
        evas_object_del(obj);
        elaro_ui_dcc_send_show(ui, server, target);
        free(server);
        free(target);

        return;
     }

   if (action->command)
     {
        c = calloc(1, sizeof(Elaro_Engine_Command));
        if (!c) goto allocation_failed;
        c->type = ELARO_ENGINE_INPUT;
        c->server = strdup(server);
        c->target = strdup(target);
        c->text = elaro_string_printf(action->command, menu->nick);
        if (!c->server || !c->target || !c->text) goto allocation_failed;

        elaro_engine_submit(ui->engine, c);
     }
   evas_object_del(obj);

   if (!action->command)
     {
        _request_query(ui, server, target);
        free(server);
        free(target);

        return;
     }
   b = elaro_ui_buffer_get(ui, server, target);
   if (b) elaro_ui_buffer_select(b);
   elm_object_focus_set(ui->input, EINA_TRUE);
   free(server);
   free(target);
   return;

allocation_failed:
   elaro_engine_command_free(c);
   free(server);
   free(target);
}

static void
_user_right_clicked_cb(void *data, Evas_Object *obj, void *info)
{
   Elaro_Ui *ui;
   const User *u;
   User_Menu *menu;
   unsigned int i;
   int x, y;

   ui = data;

   if (!info || !ui->current) return;

   u = elm_object_item_data_get(info);

   if (!u) return;

   if (ui->user_menu) evas_object_del(ui->user_menu);
   menu = calloc(1, sizeof(User_Menu));
   if (!menu) return;
   menu->app = ui;
   menu->server = strdup(ui->current->server);
   menu->nick = strdup(u->nick);
   menu->channel = strdup(ui->current->target);

   if (!menu->server || !menu->nick || !menu->channel) goto allocation_failed;
   ui->user_menu = elm_menu_add(ui->win);
   evas_object_data_set(ui->user_menu, "user-menu", menu);
   evas_object_event_callback_add(ui->user_menu, EVAS_CALLBACK_DEL, _user_menu_deleted_cb, menu);
   evas_object_smart_callback_add(ui->user_menu, "dismissed", _user_menu_dismissed_cb, NULL);

   for (i = 0; i < EINA_C_ARRAY_LENGTH(user_actions); i++)
     {
        if (!user_actions[i].label) elm_menu_item_separator_add(ui->user_menu, NULL);
        else
           elm_menu_item_add(ui->user_menu, NULL, NULL, _(user_actions[i].label),
                             _user_action_clicked_cb, &user_actions[i]);
     }

   evas_pointer_canvas_xy_get(evas_object_evas_get(obj), &x, &y);
   elm_menu_move(ui->user_menu, x, y);
   evas_object_show(ui->user_menu);
   return;

allocation_failed:
   free(menu->server);
   free(menu->nick);
   free(menu->channel);
   free(menu);
}

void
elaro_ui_users_init(Elaro_Ui *ui)
{
   /* Nick rows have one line and a fixed icon size. Keep their geometry
    * stable while a deferred snapshot restores the channel viewport. */
   elm_genlist_homogeneous_set(ui->nicks, EINA_TRUE);
   ui->users = eina_hash_string_superfast_new(NULL);
   ui->user_class = elm_genlist_item_class_new();
   ui->user_class->item_style = "default_style";
   ui->user_class->func.text_get = _user_text;
   ui->user_class->func.content_get = _user_icon;
   ui->user_class->func.del = _user_free;
   elm_genlist_select_mode_set(ui->nicks, ELM_OBJECT_SELECT_MODE_ALWAYS);
   evas_object_smart_callback_add(ui->nicks, "clicked,right", _user_right_clicked_cb, ui);
}

void
elaro_ui_users_shutdown(Elaro_Ui *ui)
{
   elaro_ui_users_clear(ui);
   eina_hash_free(ui->users);
   elm_genlist_item_class_free(ui->user_class);
}
