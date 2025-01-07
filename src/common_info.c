/*  common_info - common info struct
    Copyright (C) 2025 Dream Helium
    This file is part of litematica_reader_c (aka dhlrc).

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

#include "common_info.h"
#include "dh_mt_table.h"
#include "dh_string_util.h"
#include "dhlrc_list.h"
#include "glib.h"
#include "libnbt/nbt.h"
#include "nbt_interface/nbt_interface.h"
#include "region.h"

/* Include an instance
 * including: table<const char*, info> */
typedef struct CommonInfoSingle{
    gboolean full_free;
    DhMTTable* table;
    GList* uuid_list;
    DhStrArray* uuid_array;
    GRWLock lock;
} CommonInfoSingle;

typedef GPtrArray CommonInfos;

/* Array<CommonInfoSingle> */
static CommonInfos* infos = NULL;

static void common_info_free(CommonInfo* info, GFreeFunc func)
{
    func(info->data);
    g_date_time_unref(info->time);
    g_free(info->description);
    g_rw_lock_clear(&info->info_lock);
    g_free(info);
}

static void nbt_info_free(gpointer mem)
{
    common_info_free(mem, NBT_Free);
}

static void region_info_free(gpointer mem)
{
    common_info_free(mem, region_free);
}

static void item_list_info_free(gpointer mem)
{
    common_info_free(mem, item_list_free);
}

static void nbt_interface_info_free(gpointer mem)
{
    common_info_free(mem, dh_nbt_instance_free);
}

static void common_info_single_free(gpointer mem)
{
    CommonInfoSingle* single = mem;
    single->full_free = TRUE;
    if(single->uuid_list)
    {
        if(single->table) dh_mt_table_destroy(single->table);
        g_list_free_full(single->uuid_list, g_free);
        if(single->uuid_array) dh_str_array_free(single->uuid_array);
    }
    g_free(mem);
}

static gboolean is_same_string(gconstpointer a, gconstpointer b)
{
    if(strcmp(a, b)) return FALSE;
    else return TRUE;
}


void common_infos_init()
{
    DhInfoTypes type_num = N_TYPES;
    infos = g_ptr_array_new_with_free_func(common_info_single_free);
    for(int i = 0 ; i < N_TYPES ; i++)
    {
        CommonInfoSingle* instance = g_new0(CommonInfoSingle, 1);
        g_rw_lock_init(&instance->lock);
        g_rw_lock_writer_lock(&instance->lock);
        if(i == DH_TYPE_NBT)
            instance->table = dh_mt_table_new(g_str_hash, is_same_string, g_free, nbt_info_free);
        else if(i == DH_TYPE_Region)
            instance->table = dh_mt_table_new(g_str_hash, is_same_string, g_free, region_info_free);
        else if(i == DH_TYPE_Item_List)
            instance->table = dh_mt_table_new(g_str_hash, is_same_string, g_free, item_list_info_free);
        else if(i == DH_TYPE_NBT_INTERFACE)
            instance->table = dh_mt_table_new(g_str_hash, is_same_string, g_free, nbt_interface_info_free);
        else instance->table = NULL;
        instance->uuid_list = NULL;
        instance->uuid_array = NULL;
        g_rw_lock_writer_unlock(&instance->lock);
        g_ptr_array_add(infos, instance);
    }
}

void common_infos_free()
{
    g_ptr_array_free(infos, TRUE);
}

gboolean common_info_new(DhInfoTypes type, void* data, GDateTime* time, const gchar* description)
{
    CommonInfo* info = g_new0(CommonInfo, 1);
    gboolean ret = FALSE;
    g_rw_lock_init(&info->info_lock);
    g_rw_lock_writer_lock(&info->info_lock);

    info->data = data;
    info->time = time;
    info->description = g_strdup(description);

    CommonInfoSingle* instance = infos->pdata[type];
    DhMTTable* table = instance->table;
    gchar* uuid = g_uuid_string_random();
    ret = dh_mt_table_insert(table, uuid, info);
    instance->uuid_list = g_list_append(instance->uuid_list, uuid);
    g_rw_lock_writer_unlock(&info->info_lock);
    return ret;
}

gboolean common_info_list_remove_item(DhInfoTypes type, const gchar* uuid)
{
    CommonInfoSingle* instance = infos->pdata[type];
    CommonInfo* info = dh_mt_table_lookup(instance->table, uuid);
    if(g_rw_lock_writer_trylock(&info->info_lock))
    {
        instance->uuid_list = g_list_remove(instance->uuid_list, uuid);
        gboolean ret = dh_mt_table_remove(instance->table, uuid);
        return ret;
    }
    else return FALSE;
}

CommonInfo* common_info_list_get_common_info(DhInfoTypes type, const gchar* uuid)
{
    CommonInfoSingle* instance = infos->pdata[type];
    CommonInfo* info = dh_mt_table_lookup(instance->table, uuid);
    return info;
}

gboolean common_info_list_update_data(DhInfoTypes type, const gchar* uuid, void* data)
{
    CommonInfoSingle* instance = infos->pdata[type];
    CommonInfo* info = dh_mt_table_lookup(instance->table, uuid);
    info->data = data;
    return TRUE;
}

GList* common_info_list_get_uuid_list(DhInfoTypes type)
{
    CommonInfoSingle* instance = infos->pdata[type];
    return instance->uuid_list;
}

void common_info_list_set_uuid(DhInfoTypes type, const char* uuid)
{
    CommonInfoSingle* instance = infos->pdata[type];
    if(instance->uuid_array) dh_str_array_free(instance->uuid_array);
    dh_str_array_add_str(&(instance->uuid_array), uuid);
}

const char* common_info_list_get_uuid(DhInfoTypes type)
{
    CommonInfoSingle* instance = infos->pdata[type];
    if(instance->uuid_array && instance->uuid_array->num == 1)
        return instance->uuid_array->val[0];
    else return NULL;
}

void common_info_list_set_multi_uuid(DhInfoTypes type, const char** arr)
{
    CommonInfoSingle* instance = infos->pdata[type];
    if(instance->uuid_array) dh_str_array_free(instance->uuid_array);
    for(; *arr ; arr++)
        dh_str_array_add_str(&(instance->uuid_array), *arr);
}

char** common_info_list_get_multi_uuid(DhInfoTypes type)
{
    CommonInfoSingle* instance = infos->pdata[type];
    return dh_str_array_dup_to_plain(instance->uuid_array);
}

gboolean common_info_list_writer_trylock(DhInfoTypes type)
{
    CommonInfoSingle* instance = infos->pdata[type];
    return g_rw_lock_writer_trylock(&instance->lock);
}

void     common_info_list_writer_unlock(DhInfoTypes type)
{
    CommonInfoSingle* instance = infos->pdata[type];
    g_rw_lock_writer_unlock(&instance->lock);
}

gboolean common_info_list_reader_trylock(DhInfoTypes type)
{
    CommonInfoSingle* instance = infos->pdata[type];
    return g_rw_lock_reader_trylock(&instance->lock);
}

void     common_info_list_reader_unlock(DhInfoTypes type)
{
    CommonInfoSingle* instance = infos->pdata[type];
    g_rw_lock_reader_unlock(&instance->lock);
}