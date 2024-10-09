/*  region_info - region info struct
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

#ifndef REGION_INFO_H
#define REGION_INFO_H

#include <glib.h>
#include "region.h"

G_BEGIN_DECLS

typedef struct RegionInfo{
    Region* root;
    GDateTime* time;
    gchar* description;
    GRWLock info_lock;
} RegionInfo;

void region_info_list_free();
gboolean region_info_new(Region* root, GDateTime* time, const gchar* description);
gboolean region_info_list_remove_item(gchar* uuid);
RegionInfo* region_info_list_get_region_info(const gchar* uuid);
/* Block the update and update */
gboolean region_info_list_update_region(gchar* uuid, RegionInfo* info);
DhList* region_info_list_get_uuid_list();

void region_info_list_set_uuid(const char* uuid);
const char* region_info_list_get_uuid();

G_END_DECLS

#endif /* REGION_INFO_H */