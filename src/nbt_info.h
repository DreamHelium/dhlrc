/*  nbt_info - nbt info struct
    Copyright (C) 2024 Dream Helium
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

#ifndef NBT_INFO_H
#define NBT_INFO_H

#include <glib.h>
#include <openssl/dh.h>
#include "libnbt/nbt.h"

G_BEGIN_DECLS

typedef enum DhNbtType { Litematica, NBTStruct, Schematics, Others } 
    DhNbtType;

void  nbt_info_new(NBT* root, GDateTime* time, const gchar* description);
void  nbt_info_list_clear();
guint nbt_info_list_length();
NBT*  nbt_info_get_nbt(guint id);
GDateTime* nbt_info_get_time(guint id);
/* Don't free the string! */
gchar* nbt_info_get_description(guint id);
DhNbtType nbt_info_get_type(guint id);

G_END_DECLS

#endif /* NBT_INFO_H */