#ifndef ELARO_CORE_CORE_H
#define ELARO_CORE_CORE_H

#include <src/common.h>

/* for determining what GUI is currently in use: */
#define ELARO_GUI_EFL    6
#define ELARO_GUI_NONE	0
#define ELARO_GUI_TEXT	1
#define ELARO_GUI_GTK	2
#define ELARO_GUI_GNOME	3
#define ELARO_GUI_QT   	4
#define ELARO_GUI_KDE  	5

extern int elaro_gui;
extern int elaro_init_finished; /* TRUE after "elaro init finished" signal is sent */
extern int sighup_received; /* TRUE after received SIGHUP. */
extern time_t client_start_time;

void core_preinit(const char *path);

void core_register_options(void);
void core_init(void);
void core_deinit(void);

#endif
