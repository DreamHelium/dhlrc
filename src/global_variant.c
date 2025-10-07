#include "global_variant.h"

static gboolean ignore_extension_name = FALSE;
static GPtrArray *ignore_air_list = NULL;

void
dhlrc_global_variant_init ()
{
    ignore_air_list = g_ptr_array_new_with_free_func (g_free);
    g_ptr_array_add (ignore_air_list, g_strdup ("minecraft:air"));
    g_ptr_array_add (ignore_air_list, g_strdup ("minecraft:cave_air"));
    g_ptr_array_add (ignore_air_list, g_strdup ("minecraft:void_air"));
}

void
dhlrc_global_variant_clear ()
{
    g_ptr_array_free (ignore_air_list, TRUE);
}

void
dhlrc_set_ignore_extension_name (gboolean ignore_extension)
{
    ignore_extension_name = ignore_extension;
}

gboolean
dhlrc_get_ignore_extension_name ()
{
    return ignore_extension_name;
}

void
dhlrc_set_ignore_air_list (GPtrArray *air_list)
{
    g_ptr_array_free (ignore_air_list, TRUE);
    ignore_air_list = air_list;
}

const GPtrArray *
dhlrc_get_ignore_air_list ()
{
    return ignore_air_list;
}
