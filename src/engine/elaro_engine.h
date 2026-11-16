/* Thread boundary: copied strings and values, never Irssi or Evas objects. */
#ifndef ELARO_ENGINE_H
#define ELARO_ENGINE_H
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
   ELARO_ENGINE_CONNECT,
   ELARO_ENGINE_INPUT,
   ELARO_ENGINE_SELECT,
   ELARO_ENGINE_QUERY,
   ELARO_ENGINE_CLOSE,
   ELARO_ENGINE_TRANSFER_SEND,
   ELARO_ENGINE_TRANSFER_ACCEPT,
   ELARO_ENGINE_TRANSFER_CANCEL,
   ELARO_ENGINE_USER_INFO,
   ELARO_ENGINE_BANS,
   ELARO_ENGINE_UNBAN,
   ELARO_ENGINE_LIST,
   ELARO_ENGINE_LIST_CANCEL,
   ELARO_ENGINE_STOP
} Elaro_Engine_Command_Type;

typedef struct
{
   Elaro_Engine_Command_Type type;
   char *server, *target, *text;
   unsigned int transfer_id, request_id;
   char *host, *nick, *username, *realname, *email, *password, *channels;
   char *sasl_username, *sasl_password;
   bool sasl;
   int port;
   bool tls, auto_reconnect;
} Elaro_Engine_Command;

typedef enum
{
   ELARO_ENGINE_LINE,
   ELARO_ENGINE_CONNECTION,
   ELARO_ENGINE_NICKS,
   ELARO_ENGINE_JOINED,
   ELARO_ENGINE_QUERY_OPENED,
   ELARO_ENGINE_BUFFER_CLOSED,
   ELARO_ENGINE_TOPIC,
   ELARO_ENGINE_USER_DETAILS,
   ELARO_ENGINE_USER_PRESENCE,
   /* Invalidate one nick, or the whole network when target is empty. */
   ELARO_ENGINE_PRESENCE_RESET,
   ELARO_ENGINE_TRANSFER_OFFER,
   ELARO_ENGINE_TRANSFER_UPDATE,
   ELARO_ENGINE_TRANSFER_END,
   ELARO_ENGINE_TRANSFER_ERROR,
   ELARO_ENGINE_BAN_ENTRY,
   ELARO_ENGINE_BAN_END,
   ELARO_ENGINE_BAN_ERROR,
   ELARO_ENGINE_LIST_OPEN,
   ELARO_ENGINE_LIST_BATCH,
   ELARO_ENGINE_LIST_END,
   ELARO_ENGINE_LIST_ERROR
} Elaro_Engine_Event_Type;

/* Owned channel-list payload, sorted naturally by name on the worker. */
typedef struct
{
   char *name, *topic;
   unsigned int users;
} Elaro_Engine_Channel;

/* Copied nick state from Irssi, owned by a NICKS event. */
typedef struct
{
   char *nick;
   char prefix;
   bool away;
} Elaro_Engine_User;

typedef struct
{
   Elaro_Engine_Event_Type type;
   /* NICKS payload; text remains a readable summary. */
   Elaro_Engine_User *users;
   unsigned int user_count;
   bool away;
   Elaro_Engine_Channel *channels;
   unsigned int channel_count;
   /* CONNECTION payload identifies the original saved profile, even after /nick. */
   char *host, *nick;
   int port;
   bool connected;
   bool channel_message; /* Channel chat or action, excluding protocol/status events. */
   bool private_message;
   bool mention; /* Incoming channel message mentioning our current nickname. */
   unsigned int transfer_id, request_id;
   uint64_t transferred, size;
   char *server, *target, *text;
} Elaro_Engine_Event;
typedef struct _Elaro_Engine Elaro_Engine;
/* Returns NULL if wakeup descriptors cannot be created. */
Elaro_Engine *
elaro_engine_start(const char *binary);
/* Free an unsubmitted command, including any fields already allocated. */
void
elaro_engine_command_free(Elaro_Engine_Command *command);

/* Takes ownership, including every string. */
void
elaro_engine_submit(Elaro_Engine *engine, Elaro_Engine_Command *command);
/* Borrowed descriptor: readable while events remain. Watch it, but never read
 * or close it. Remove the watch before stopping the engine. */
int
elaro_engine_event_fd(const Elaro_Engine *engine);
Elaro_Engine_Event *
elaro_engine_poll(Elaro_Engine *engine);
void
elaro_engine_event_free(Elaro_Engine_Event *event);
void
elaro_engine_stop(Elaro_Engine *engine);
#endif
