#include "ui/elaro_ui.h"
#include "elaro_music_backend.h"
#include <string.h>

int
main(int argc, char **argv)
{
   if (argc == 2 && !strcmp(argv[1], "--music-backend")) return elaro_music_backend_run(argc, argv);

   return elaro_ui_run(argc, argv);
}
