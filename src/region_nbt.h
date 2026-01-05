/*  region_litematic - Region Structure converted from Litematic
    Copyright (C) 2025 Dream Helium
    This file is part of litematica_reader_c.

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

#ifndef DHLRC_REGION_NBT_H
#define DHLRC_REGION_NBT_H

#include "region.h"
#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
  DHLRC_REGION_NBT_ERROR_NO_CHILD,
  DHLRC_REGION_NBT_ERROR_WRONG_TYPE
} DhlrcRegionNbtError;

Region *region_new_from_nbt (NbtNode *root, int i, GError **err,
                             DhProgressFullSet func, void *main_klass);

G_END_DECLS

#endif // DHLRC_REGION_NBT_H
