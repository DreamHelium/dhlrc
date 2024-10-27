/*  il_info - item list info struct
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

#ifndef IL_INFO_H
#define IL_INFO_H

#include "dhlrc_list.h"

G_BEGIN_DECLS

typedef struct IlInfo {
    ItemList* il;
    GDateTime* time;
    gchar* description;
    /* The lock for this il_info */
    GRWLock info_lock;
    /* Freed sign */
    gboolean freed;
} IlInfo;

void il_info_list_free();
gboolean il_info_new(ItemList* il, GDateTime* time, const gchar* description);
gboolean il_info_list_remove_item(gchar* uuid);
IlInfo* il_info_list_get_il_info(const gchar* uuid);
/* Block the update and update */
gboolean il_info_list_update_il(gchar* uuid, IlInfo* info);
DhList* il_info_list_get_uuid_list();
void il_info_list_init();

void il_info_list_set_uuid(const char* uuid);
const char* il_info_list_get_uuid();


G_END_DECLS

#endif /* IL_INFO_H */