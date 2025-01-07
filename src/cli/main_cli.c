#include "main_cli.h"
#include "../translation.h"
#include "dh_string_util.h"
#include "dh_validator_cpp.hpp"
#include "manage_nbt.h"
#define DH_EDITLINE_USED

static void add_args_common(void* arg, char c, const char* first, const char* second, const char* description)
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

static void add_args(void* arg)
{
    add_args_common(arg, 'n', "mnbt" ,N_("manage_nbt"), _("Manage NBT"));
    add_args_common(arg, 'r', "mregion", N_("manage_region"), _("Manage Region"));
    add_args_common(arg, 'i', "mitem", N_("manage_item_list"),_("Manage item list"));
    add_args_common(arg, 'c', "config", N_("config_settings"), _("Config settings"));
}

extern int
start_point (int argc, char **argv, const char* prpath)
{
    printf(_("The functions are listed below:\n"));
    printf("[0] %s\n", _("Manage NBT"));
    printf("[1] %s\n", _("Manage Region"));
    printf("[2] %s\n", _("Manage item list"));
    printf("[3] %s\n", _("Config settings"));

    void* arg = dh_arg_new();
    add_args(arg);

    #ifdef DH_DEBUG_IN_IDE
    dh_arg_add_arg(arg, 'd', "debug", _("Debug"));
    #endif
    
    GValue val = {0};
    char ret_val_c = 0;
    dh_get_output_arg(arg, N_("Please enter a number or an option"), TRUE, &val);

    if(G_VALUE_HOLDS_UCHAR(&val)) 
    {
        ret_val_c = g_value_get_uchar(&val);
        if(ret_val_c == 'n') return manage_nbt_instance();
    }
    else return -1;

    g_message("%c", ret_val_c);

    #ifdef DH_DEBUG_IN_IDE
    if(ret_val_c == 'd')
        debug();
    #endif
    
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