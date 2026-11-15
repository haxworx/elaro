#ifndef ELARO_UI_PRIVATE_H
#define ELARO_UI_PRIVATE_H

#include "elaro_ui.h"
#include "../elaro_intl.h"
#include "../../engine/elaro_engine.h"
#include "../elaro_config.h"
#include "elaro_ui_buffers.h"
#include "elaro_ui_history.h"
#include "elaro-config.h"
#include "elaro_ui_bans.h"
#include "elaro_ui_list.h"

struct _Elaro_Ui_Buffer
{
   Elaro_Ui *app;
   char *server, *target, *topic;
   /* Server buffers retain copied connection identity for saved-profile selection. */
   char *connection_host, *connection_nick;
   int connection_port;
   Eina_Bool connected;
   Eina_Bool channel, unread, mention, user_count_known, away;
   unsigned int user_count;
   unsigned int user_scroll_index; /* First visible row, one-based. */
   Eina_Strbuf *history;
   Eina_List *input_history, *input_position;
   char *input_draft;
   Elm_Object_Item *item; /* NULL while the parent server is collapsed. */
   Elaro_Ui_Buffer *parent;
   Elaro_Ui_Buffer *last_selected; /* Server's most recently selected child. */
};

/* Main-loop state only. The worker sees copied commands and events. */
struct _Elaro_Ui
{
   Elaro_Config *config;
   char *config_error, *binary;
   Elaro_Engine *engine;
   Elaro_Ui_Bans *bans;
   Elaro_Ui_List *channel_list;
   unsigned int list_request_id;
   unsigned int ban_request_id;
   Elm_Genlist_Item_Class *buffer_class, *user_class;
   Eina_Hash *users, *avatars;

   struct
   {
      Eina_Hash *windows;
      unsigned int next_send;
   } dcc;

   char *theme_path;
   Evas_Object *resize_grips[8];
   Evas_Object *frame, *app_menu, *theme_buttons[4], *theme_status;
   Evas_Object *background, *content;
   Evas_Object *buffer_panes, *user_panes;
   Ecore_Animator *user_panel_animator;
   double user_panel_animation_from;
   double user_panel_saved_ratio;
   Eina_Bool user_panel_collapsed;
   Evas_Object *win, *buffers, *history, *nicks, *input, *title, *title_button;
   Evas_Object *user_menu, *buffer_menu, *topic, *about, *preview, *music;

   struct
   {
      Evas_Object *popup, *host, *port, *nick, *username, *realname, *email, *password, *channels,
          *tls, *error;
      Evas_Object *auto_reconnect, *sasl, *sasl_username, *sasl_password;
      Evas_Object *saved, *connect, *remove;
      Evas_Object *view_box, *connection_view, *options_view, *allow_links, *options_error, *apply,
          *opacity, *transparent, *notify_private, *enable_sounds;
      Eina_Bool options_visible, dirty;
      Elaro_Config *selected;
   } connection;

   struct
   {
      Eina_List *matches, *next;
      char *before, *after, *text;
      int cursor;
   } completion;

   Eina_List *views, *theme_icons;
   Elaro_Ui_Buffer *current;
   Eina_Bool user_scroll_restore;
};

#include "elaro_ui_util.h"
#include "elaro_ui_users.h"
#include "elaro_ui_avatar.h"
#include "elaro_ui_connection.h"
#include "elaro_ui_options.h"
#include "elaro_ui_dcc.h"
#include "elaro_ui_about.h"
#include "elaro_ui_text.h"
#include "elaro_ui_image.h"
#include "elaro_ui_music.h"
#include "elaro_ui_links.h"
#include "elaro_ui_notify.h"

#endif
