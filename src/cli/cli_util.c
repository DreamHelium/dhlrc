#include "cli_util.h"
#include <stdlib.h>
#include <glib.h>

void dhlrc_clear_screen()
{
#ifdef G_OS_WIN32
    system("cls");
#else
    system("clear");
#endif
}