#include "main_cli.h"
#include "../translation.h"
#include "dh_string_util.h"
#include <ncursesw/ncurses.h>
#include "dh_validator.h"
#include "dh_nc_rl.h"

extern int
start_point (int argc, char **argv)
{
    initscr();
    printw(_("The functions are listed below:\n"));
    printw("[0] %s\n", _("Manage NBT"));
    printw("[1] %s\n", _("Manage Region"));
    printw("[2] %s\n", _("Manage item list"));
    printw("[3] %s\n", _("Config settings"));
    refresh();
    DhOut* out = dh_out_new();
    DhArgInfo* arg = dh_arg_info_new();
    dh_out_set_show_opt(out, TRUE);
    dh_arg_info_add_arg(arg, 'n', "mnbt", N_("Manage NBT"));
    dh_arg_info_add_arg(arg, 'r', "mregion", N_("Manage Region"));
    dh_arg_info_add_arg(arg, 'i', "mitem", N_("Manage item list"));
    dh_arg_info_add_arg(arg, 'c', "config", N_("Config settings"));
    dh_arg_info_add_arg(arg, 'q', "quit", N_("Quit application"));
    #ifdef DH_DEBUG_IN_IDE
    dh_arg_info_add_arg(arg, 'd', "debug", N_("Debug"));
    #endif
    dh_arg_info_change_default_arg(arg, 'q');
    DhIntValidator* validator = dh_int_validator_new(0, 3);
    
    GValue val = {0};
    int ret_val = -1;
    char ret_val_c = 0;
    // dh_out_read_and_output(out, N_("Please enter a number or an option"), "dhlrc", arg, DH_VALIDATOR(validator), FALSE, &val);

    dh_nc_rl_get_arg(NULL);
    endwin();

    if(G_VALUE_HOLDS_INT64(&val)) ret_val = g_value_get_int64(&val);
    else if(G_VALUE_HOLDS_CHAR(&val)) ret_val_c = g_value_get_schar(&val);
    else return -1;

    g_message("%d", ret_val);
    g_message("%d", ret_val_c);

    #ifdef DH_DEBUG_IN_IDE
    if(ret_val_c == 'd')
        debug();
    #endif

    g_object_unref(out);
    g_object_unref(arg);
    g_object_unref(validator);
    
    return 0;
}

extern const char* module_name()
{
    return NULL;
}

extern DhStrArray* module_name_array()
{
    DhStrArray* arr = NULL;
    dh_str_array_add_str(&arr, "cli");
    dh_str_array_add_str(&arr, "tui");
    return arr;
}

extern const char* module_description()
{
    return _("The CLI/TUI interface of dhlrc");
}

extern const char* help_description()
{
    return NULL;
}