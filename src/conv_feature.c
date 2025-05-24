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

DhlrcMainFunc
dhlrc_conv_enable (GModule *module)
{
    DhlrcMainFunc func = NULL;
    if (!g_module_symbol (module, "start_point", (gpointer *)&func))
        return NULL;
    enabled = TRUE;
    return func;
}

gboolean
dhlrc_conv_enabled ()
{
    return enabled;
}

