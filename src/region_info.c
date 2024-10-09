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
#include <dh_mt_table.h>

static gboolean full_free = FALSE;
static DhMTTable* table = NULL;
static const char* uuid = NULL;

static DhList* uuid_list = NULL;

static void region_info_free(gpointer data)
{
    RegionInfo* info = data;
    /* Although item list info can be freed by outer function
     * We believe the programmer would not do this */
    if(full_free) g_rw_lock_writer_lock(&(info->info_lock));

    region_free(info->root);
    g_date_time_unref(info->time);
    g_free(info->description);
    g_rw_lock_writer_unlock(&(info->info_lock));
    g_rw_lock_clear(&(info->info_lock));
    g_free(info);
}


static gboolean is_same_string(gconstpointer a, gconstpointer b)
{
    if(strcmp(a, b)) return FALSE;
    else return TRUE;
}

void region_info_list_free()
{
    full_free = TRUE;
    if(uuid_list)
    {
        dh_mt_table_destroy(table);
        dh_list_free(uuid_list);
    }
}

gboolean region_info_new(Region* root, GDateTime* time, const gchar* description)
{
    RegionInfo* info = g_new0(RegionInfo, 1);
    gboolean ret = FALSE;
    /* Init the item info lock and ready to lock it */
    g_rw_lock_init(&(info->info_lock));
    g_rw_lock_writer_lock(&(info->info_lock));

    info->root = root;
    info->time = time;
    info->description = g_strdup(description);

    if(!table)
    {
        table = dh_mt_table_new(g_str_hash, is_same_string, g_free, region_info_free);
        uuid_list = dh_list_new();
    }
    g_rw_lock_writer_lock(&uuid_list->lock);
    gchar* uuid = g_uuid_string_random();
    ret = dh_mt_table_insert(table, uuid, info);
    uuid_list->list = g_list_append(uuid_list->list, uuid);
    g_rw_lock_writer_unlock(&uuid_list->lock);
    /* Unlock the writer lock */
    g_rw_lock_writer_unlock(&(info->info_lock));
    return ret;
}

gboolean region_info_list_remove_item(gchar* uuid)
{
    RegionInfo* info = dh_mt_table_lookup(table, uuid);
    if(g_rw_lock_writer_trylock(&(info->info_lock)))
    {
        g_rw_lock_writer_lock(&uuid_list->lock);
        uuid_list->list = g_list_remove(uuid_list->list, uuid);
        gboolean ret = dh_mt_table_remove(table, uuid);
        g_rw_lock_writer_unlock(&uuid_list->lock);
        return ret;
    }
    else return FALSE; /* Some function is using the il */
}

RegionInfo* region_info_list_get_region_info(const gchar* uuid)
{
    RegionInfo* info = dh_mt_table_lookup(table, uuid);
    return info;
}

/* Block the update and update */
gboolean region_info_list_update_region(gchar* uuid, RegionInfo* info)
{
    /* We believe this function is used in a lock function
     * If not we will not lock */
    return dh_mt_table_replace(table, uuid, info);
}


DhList* region_info_list_get_uuid_list()
{
    return uuid_list;
}

void region_info_list_set_uuid(const char* auuid)
{
    uuid = auuid;
}

const char* region_info_list_get_uuid()
{
    return uuid;
}