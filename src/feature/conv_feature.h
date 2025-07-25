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

#ifndef CONV_FEATURE_H
#define CONV_FEATURE_H

#include "../region.h"
#include <glib.h>
#include <gmodule.h>

G_BEGIN_DECLS

typedef int (*DhlrcMainFunc) (int argc, char **argv, const char *);

DhlrcMainFunc dhlrc_conv_enable (GModule *module);
gboolean dhlrc_conv_enabled ();
void *dhlrc_conv_region_to_nbt (Region *region, gboolean temp_root);
void *dhlrc_conv_region_to_lite_nbt (Region *region, gboolean temp_root);
void *dhlrc_conv_region_to_schema_nbt (Region *region, gboolean temp_root);

G_END_DECLS

#endif // CONV_FEATURE_H
