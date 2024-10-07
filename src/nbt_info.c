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

#include "nbt_info.h"
#include "dh_mt_table.h"
#include "dhlrc_list.h"
#include "region.h"

static gboolean full_free = FALSE;
static DhMTTable* table = NULL;
static const char* uuid = NULL;

static DhList* uuid_list = NULL;

static void nbtinfo_free(gpointer data)
{
    NbtInfo* info = data;
    /* Although item list info can be freed by outer function
     * We believe the programmer would not do this */
    if(full_free) g_rw_lock_writer_lock(&(info->info_lock));

    NBT_Free(info->root);
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

void nbt_info_list_free()
{
    full_free = TRUE;
    if(uuid_list)
    {
        dh_mt_table_destroy(table);
        dh_list_free(uuid_list);
    }
}

gboolean nbt_info_new(NBT* root, GDateTime* time, const gchar* description)
{
    NbtInfo* info = g_new0(NbtInfo, 1);
    gboolean ret = FALSE;
    /* Init the item info lock and ready to lock it */
    g_rw_lock_init(&(info->info_lock));
    g_rw_lock_writer_lock(&(info->info_lock));

    info->root = root;
    info->time = time;
    info->description = g_strdup(description);

    if(lite_region_num(root))
        info->type = Litematica;
    else
    {
        Region* region = region_new_from_nbt(root);
        if(region)
        {
            info->type = NBTStruct;
            region_free(region);
        }
        else info->type = Others;
    }

    if(!table)
    {
        table = dh_mt_table_new(g_str_hash, is_same_string, g_free, nbtinfo_free);
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

gboolean nbt_info_list_remove_item(gchar* uuid)
{
    NbtInfo* info = dh_mt_table_lookup(table, uuid);
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

NbtInfo* nbt_info_list_get_nbt_info(gchar* uuid)
{
    NbtInfo* info = dh_mt_table_lookup(table, uuid);
    return info;
}
/* Block the update and update */
gboolean nbt_info_list_update_nbt(gchar* uuid, NbtInfo* info)
{
    /* We believe this function is used in a lock function
     * If not we will not lock */
    return dh_mt_table_replace(table, uuid, info);
}


DhList* nbt_info_list_get_uuid_list()
{
    return uuid_list;
}

void nbt_info_list_set_uuid(const char* auuid)
{
    uuid = auuid;
}

const char* nbt_info_list_get_uuid()
{
    return uuid;
}