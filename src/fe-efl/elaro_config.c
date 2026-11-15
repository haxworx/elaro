#include "elaro_config.h"
#include "elaro_intl.h"
#include "elaro_util.h"
#include <Eet.h>
#include <math.h>

#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

static void
_string_free(const char *text)
{
   free((void *)text);
}

static Eet_Data_Descriptor *
_descriptor_new(Eet_Data_Descriptor *connection)
{
   Eet_Data_Descriptor_Class klass;
   Eet_Data_Descriptor *d;

   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&klass, Elaro_Config);
   klass.func.str_alloc = strdup;
   klass.func.str_free = _string_free;
   d = eet_data_descriptor_stream_new(&klass);
   if (!d) return NULL;
#define FIELD(name, type) EET_DATA_DESCRIPTOR_ADD_BASIC(d, Elaro_Config, #name, name, type)
   FIELD(window_width, EET_T_INT);
   FIELD(window_height, EET_T_INT);

   if (connection)
     {
        FIELD(always_allow_external_links, EET_T_INT);
        FIELD(notify_private_messages, EET_T_INT);
        FIELD(enable_sounds, EET_T_INT);
        FIELD(theme, EET_T_INT);
        FIELD(window_x, EET_T_INT);
        FIELD(window_y, EET_T_INT);
        FIELD(window_position_set, EET_T_INT);
        FIELD(buffer_panel_ratio, EET_T_DOUBLE);
        FIELD(user_panel_ratio, EET_T_DOUBLE);
        FIELD(window_opacity, EET_T_INT);
        FIELD(window_transparent, EET_T_INT);
     }
   FIELD(version, EET_T_INT);
   FIELD(port, EET_T_INT);
   FIELD(tls, EET_T_INT);
   FIELD(sasl, EET_T_INT);
   FIELD(auto_reconnect, EET_T_INT);
   FIELD(host, EET_T_STRING);
   FIELD(nick, EET_T_STRING);
   FIELD(username, EET_T_STRING);
   FIELD(realname, EET_T_STRING);
   FIELD(email, EET_T_STRING);
   FIELD(password, EET_T_STRING);
   FIELD(channels, EET_T_STRING);
   FIELD(sasl_username, EET_T_STRING);
   FIELD(sasl_password, EET_T_STRING);
#undef FIELD

   if (connection)
      EET_DATA_DESCRIPTOR_ADD_LIST(d, Elaro_Config, "connections", connections, connection);

   return d;
}

Elaro_Config *
elaro_config_connection_copy(const Elaro_Config *c)
{
   Elaro_Config *copy;

   copy = calloc(1, sizeof(Elaro_Config));
   if (!copy) return NULL;
   *copy = *c;
   copy->connections = NULL;
   copy->always_allow_external_links = EINA_FALSE;
   copy->notify_private_messages = EINA_FALSE;
   copy->enable_sounds = EINA_FALSE;
#define CLEAR(field) copy->field = NULL
   CLEAR(host);
   CLEAR(nick);
   CLEAR(username);
   CLEAR(realname);
   CLEAR(email);
   CLEAR(password);
   CLEAR(channels);
   CLEAR(sasl_username);
   CLEAR(sasl_password);
#undef CLEAR
#define COPY(field) do { \
   copy->field = c->field ? strdup(c->field) : NULL; \
   if (c->field && !copy->field) goto allocation_failed; \
} while (0)
   COPY(host);
   COPY(nick);
   COPY(username);
   COPY(realname);
   COPY(email);
   COPY(password);
   COPY(channels);
   COPY(sasl_username);
   COPY(sasl_password);
#undef COPY

   return copy;

allocation_failed:
   elaro_config_free(copy);
   return NULL;
}

void
elaro_config_free(Elaro_Config *c)
{
   Elaro_Config *connection;

   if (!c) return;

   EINA_LIST_FREE (c->connections, connection)
      elaro_config_free(connection);
   free(c->host);
   free(c->nick);
   free(c->username);
   free(c->realname);
   free(c->email);
   free(c->password);
   free(c->channels);
   free(c->sasl_username);
   free(c->sasl_password);
   free(c);
}

static Elaro_Config *
_defaults(void)
{
   Elaro_Config *c;

   c = calloc(1, sizeof(Elaro_Config));
   if (!c) return NULL;
   c->version = ELARO_CONFIG_VERSION;
   c->buffer_panel_ratio = 0.20;
   c->user_panel_ratio = 0.18;
   c->window_opacity = ELARO_OPACITY_MAX;
   c->port = 6697;
   c->tls = EINA_TRUE;
   c->host = strdup("irc.libera.chat");
   c->nick = strdup("");
   c->username = elaro_user_name_get();
   c->realname = strdup("elaro user");
   c->email = strdup("");
   c->password = strdup("");
   c->channels = strdup("");
   c->sasl_username = strdup("");
   c->sasl_password = strdup("");

   if (!c->host || !c->nick || !c->username || !c->realname || !c->email
       || !c->password || !c->channels || !c->sasl_username || !c->sasl_password)
      goto allocation_failed;
   return c;

allocation_failed:
   elaro_config_free(c);
   return NULL;
}

static void
_config_error(Eina_Error *error, const char *message)
{
   if (error) *error = eina_error_msg_static_register(message);
}

Elaro_Config *
elaro_config_load(Eina_Error *error)
{
   char *path, *dir;
   Elaro_Config *c, *saved;
   Eet_File *file;
   Eet_Data_Descriptor *connection, *d;
   Eina_List *node, *next;

   if (error) *error = 0;
   dir = elaro_config_dir_get();
   if (!dir) return NULL;
   path = elaro_path_join(dir, "config.eet", NULL);
   free(dir);
   if (!path) return NULL;
   c = NULL;

   if (ecore_file_exists(path))
     {
        file = eet_open(path, EET_FILE_MODE_READ);

        if (file)
          {
             connection = _descriptor_new(NULL);
             d = connection ? _descriptor_new(connection) : NULL;
             if (d) c = eet_data_read(file, d, "connection");
             eet_close(file);
             eet_data_descriptor_free(d);
             eet_data_descriptor_free(connection);
          }

        if (!c || c->version != ELARO_CONFIG_VERSION || c->port < 1
            || c->port > 65535 || !c->host || !c->nick || !c->password || !c->channels
            || !c->sasl_username || !c->sasl_password)
          {
             elaro_config_free(c);
             c = NULL;
             _config_error(error, _("Could not read the saved connection settings."));
          }
     }
   free(path);

   if (!c) c = _defaults();
   if (!c) return NULL;

   if (!c->username) c->username = elaro_user_name_get();
   if (!c->realname) c->realname = strdup("elaro user");
   if (!c->email) c->email = strdup("");

   c->window_opacity = ELARO_CLAMP(c->window_opacity, ELARO_OPACITY_MIN, ELARO_OPACITY_MAX);
   if (!c->username || !c->realname || !c->email) goto allocation_failed;
   if (!isfinite(c->buffer_panel_ratio) || c->buffer_panel_ratio < 0 || c->buffer_panel_ratio > 1)
      c->buffer_panel_ratio = 0.20;
   if (!isfinite(c->user_panel_ratio) || c->user_panel_ratio < 0 || c->user_panel_ratio > 1)
      c->user_panel_ratio = 0.18;
   EINA_LIST_FOREACH (c->connections, node, saved)
     {
        if (!saved->username) saved->username = elaro_user_name_get();
        if (!saved->realname) saved->realname = strdup("elaro user");
        if (!saved->email) saved->email = strdup("");
        if (!saved->username || !saved->realname || !saved->email) goto allocation_failed;
     }
   EINA_LIST_FOREACH_SAFE (c->connections, node, next, saved)

      if (!saved->host || !*saved->host || !saved->nick || !*saved->nick || !saved->password
          || !saved->channels || !saved->sasl_username || !saved->sasl_password || saved->port < 1
          || saved->port > 65535)
        {
           c->connections = eina_list_remove_list(c->connections, node);
           elaro_config_free(saved);
        }

   c->window_size_default = c->window_width < 320 || c->window_height < 240;
   if (c->window_width < 320) c->window_width = 1280;

   if (c->window_height < 240) c->window_height = 900;

   return c;

allocation_failed:
   elaro_config_free(c);
   return NULL;
}

Eina_Bool
elaro_config_save(const Elaro_Config *c, Eina_Error *error)
{
   char *dir, *path = NULL, *temp = NULL, *parent;
   Eina_Bool ok;
   int fd, written;
   Eet_File *file;
   Eet_Data_Descriptor *connection, *d;
   Elaro_Config stored;
   Eet_Error result;

   if (error) *error = 0;
   ok = EINA_FALSE;
   dir = elaro_config_dir_get();
   if (!dir) goto failure;
   path = elaro_path_join(dir, "config.eet", NULL);
   temp = elaro_path_join(dir, ".config.eet.XXXXXX", NULL);
   if (!path || !temp) goto failure;

   parent = ecore_file_dir_get(dir);
   if (!parent) goto failure;
   ok = ecore_file_mkpath(parent);
   free(parent);
   if (!ok) goto failure;
   ok = EINA_FALSE;
   if (mkdir(dir, 0700) != 0 && errno != EEXIST) goto failure;
   fd = mkstemp(temp);

   if (fd < 0) goto failure;
   close(fd);
   file = eet_open(temp, EET_FILE_MODE_WRITE);

   if (!file) goto failure;
   connection = _descriptor_new(NULL);
   d = connection ? _descriptor_new(connection) : NULL;
   if (!d)
     {
        eet_close(file);
        eet_data_descriptor_free(connection);
        goto failure;
     }
   stored = *c;
   stored.version = ELARO_CONFIG_VERSION;
   if (!isfinite(stored.buffer_panel_ratio) || stored.buffer_panel_ratio < 0
       || stored.buffer_panel_ratio > 1)
      stored.buffer_panel_ratio = 0.20;
   if (!isfinite(stored.user_panel_ratio) || stored.user_panel_ratio < 0
       || stored.user_panel_ratio > 1)
      stored.user_panel_ratio = 0.18;
   stored.window_opacity = ELARO_CLAMP(c->window_opacity, ELARO_OPACITY_MIN, ELARO_OPACITY_MAX);
   written = eet_data_write(file, d, "connection", &stored, EET_COMPRESSION_DEFAULT);
   result = eet_close(file);
   eet_data_descriptor_free(d);
   eet_data_descriptor_free(connection);

   if (!written || result != EET_ERROR_NONE) goto failure;

   if (rename(temp, path) != 0) goto failure;
   ok = EINA_TRUE;
   goto done;
failure:
   _config_error(error,
                 _("Could not save connection settings in the Elaro configuration directory."));
   if (temp) unlink(temp);
done:
   free(dir);
   free(path);
   free(temp);

   return ok;
}
