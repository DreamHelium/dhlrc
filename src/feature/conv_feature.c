/*  conv_feature - Converter features
Copyright (C) 2025 Dream Helium
    This file is part of dhlrc.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>. */

#include "conv_feature.h"

static gboolean enabled = FALSE;
typedef void *(*ConvFunc) (Region *, gboolean);
static ConvFunc conv_to_nbt = NULL;
static ConvFunc conv_to_lite = NULL;
static ConvFunc conv_to_schema = NULL;

DhlrcMainFunc
dhlrc_conv_enable (GModule *module)
{
    DhlrcMainFunc func = NULL;
    if (!g_module_symbol (module, "start_point", (gpointer *)&func)
        || !g_module_symbol (module, "nbt_instance_ptr_new_from_region",
                             (gpointer *)&conv_to_nbt)
        || !g_module_symbol (module, "lite_instance_ptr_new_from_region",
                             (gpointer *)&conv_to_lite)
        || !g_module_symbol (module, "new_schema_instance_ptr_new_from_region",
                             (gpointer *)&conv_to_schema))
        return NULL;
    enabled = TRUE;
    return func;
}

gboolean
dhlrc_conv_enabled ()
{
    return enabled;
}
void *
dhlrc_conv_region_to_nbt (Region *region, gboolean temp_root)
{
    if (enabled)
        {
            return conv_to_nbt (region, temp_root);
        }
    return NULL;
}

void *
dhlrc_conv_region_to_lite_nbt (Region *region, gboolean temp_root)
{
    if (enabled)
        {
            return conv_to_lite (region, temp_root);
        }
    return NULL;
}

void *
dhlrc_conv_region_to_schema_nbt (Region *region, gboolean temp_root)
{
    if (enabled)
        {
            return conv_to_schema (region, temp_root);
        }
    return NULL;
}
