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
#include "libnbt/nbt.h"
#include "dhlrc_list.h"

G_BEGIN_DECLS

typedef enum DhNbtType { Litematica, NBTStruct, Schematics, Others } 
    DhNbtType;

typedef struct NbtInfo{
    NBT* root;
    GDateTime* time;
    gchar* description;
    GRWLock info_lock;
    DhNbtType type;
} NbtInfo;

void nbt_info_list_init();
void nbt_info_list_free();
gboolean nbt_info_new(NBT* root, GDateTime* time, const gchar* description);
gboolean nbt_info_list_remove_item(gchar* uuid);
NbtInfo* nbt_info_list_get_nbt_info(const gchar* uuid);
/* Block the update and update */
gboolean nbt_info_list_update_nbt(gchar* uuid, NbtInfo* info);
DhList* nbt_info_list_get_uuid_list();

void nbt_info_list_set_uuid(const char* uuid);
const char* nbt_info_list_get_uuid();

G_END_DECLS

#endif /* NBT_INFO_H */