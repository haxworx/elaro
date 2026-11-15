#ifndef ELARO_CONFIG_H
#define ELARO_CONFIG_H
#include <Eina.h>
#define ELARO_CONFIG_VERSION 12
#define ELARO_OPACITY_MIN 25
#define ELARO_OPACITY_MAX 100

typedef struct _Elaro_Config
{
   Eina_List *connections;
   int version, port, tls, sasl, auto_reconnect;
   int window_width, window_height;
   /* Runtime only: default dimensions are logical; saved dimensions are pixels. */
   Eina_Bool window_size_default;
   int window_x, window_y, window_position_set;
   double buffer_panel_ratio, user_panel_ratio;
   int always_allow_external_links;
   int notify_private_messages;
   int enable_sounds;
   int theme;
   int window_opacity, window_transparent;
   char *host, *nick, *username, *realname, *email, *password, *channels, *sasl_username,
       *sasl_password;
} Elaro_Config;

Elaro_Config *
elaro_config_load(Eina_Error *error);
Eina_Bool
elaro_config_save(const Elaro_Config *config, Eina_Error *error);
Elaro_Config *
elaro_config_connection_copy(const Elaro_Config *config);
void
elaro_config_free(Elaro_Config *config);
#endif
