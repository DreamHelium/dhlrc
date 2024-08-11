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

#include "region_info.h"

static GList* region_info_list = NULL;

typedef struct RegionInfo{
    Region* root;
    GDateTime* time;
    gchar* description;
} RegionInfo;

static void region_info_free(gpointer info);

static void region_info_free(gpointer data)
{
    RegionInfo* info = data;
    region_free(info->root);
    g_date_time_unref(info->time);
    g_free(info->description);
    g_free(info);
}

void region_info_list_clear()
{
    g_list_free_full(region_info_list, region_info_free);
}

void region_info_new(Region* region, GDateTime *time, const gchar *description)
{
    RegionInfo* info = g_new0(RegionInfo, 1);
    info->root = region;
    info->time = time;
    info->description = g_strdup(description);
    region_info_list = g_list_append(region_info_list, info);
}

guint region_info_list_length()
{
    return g_list_length(region_info_list);
}

Region* region_info_get_region(guint id)
{
    RegionInfo* info = g_list_nth_data(region_info_list, id);
    return info->root;
}

GDateTime* region_info_get_time(guint id)
{
    RegionInfo* info = g_list_nth_data(region_info_list, id);
    return info->time;
}

gchar* region_info_get_description(guint id)
{
    RegionInfo* info = g_list_nth_data(region_info_list, id);
    return info->description;
}