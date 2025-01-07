#include "cli_util.h"
#include <stdlib.h>
#include <glib.h>
#include "../translation.h"
#include "../dhutil/dh_validator_cpp.hpp"

void dhlrc_clear_screen()
{
#ifdef G_OS_WIN32
    system("cls");
#else
    system("clear");
#endif
}

void dhlrc_add_args_common(void* arg, char c, const char* first, const char* second, const char* description)
{
    DhStrArray* arr = dh_str_array_init(first);
    dh_str_array_add_str(&arr, second);
    char* tr_str = gettext(second);
    if(strcmp(tr_str, second))
        dh_str_array_add_str(&arr, tr_str);
    char** strv = dh_str_array_dup_to_plain(arr);
    dh_arg_add_arg_multi(arg, c, strv, description);
    dh_str_array_free_plain(strv);
}