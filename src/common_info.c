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
#include "nbt_interface_cpp/nbt_interface.hpp"
#include "region.h"

/* Include an instance
 * including: table<const char*, info> */
typedef struct CommonInfoSingle
{
    gboolean full_free;
    DhMTTable *table;
    GList *uuid_list;
    DhStrArray *uuid_array;
    GPtrArray *update_notifiers;
} CommonInfoSingle;

typedef struct CommonInfo{
    void* data;
    GDateTime* time;
    gchar* description;
    GRWLock info_lock;
} CommonInfo;

typedef GPtrArray CommonInfos;

static CommonInfo* common_info_list_get_common_info(DhInfoTypes type, const gchar* uuid);

/* Array<CommonInfoSingle> */
static CommonInfos *infos = NULL;

static void
update (CommonInfoSingle *instance)
{
    for (int i = 0; i < instance->update_notifiers->len; i++)
        {
            UpdateNotifier *un = instance->update_notifiers->pdata[i];
            un->func (un->main_class);
        }
}

static void
common_info_free (CommonInfo *info, GFreeFunc func)
{
    func (info->data);
    g_date_time_unref (info->time);
    g_free (info->description);
    g_rw_lock_clear (&info->info_lock);
    g_free (info);
}

static void
region_info_free (gpointer mem)
{
    common_info_free (mem, (GFreeFunc)region_free);
}

static void
item_list_info_free (gpointer mem)
{
    common_info_free (mem, (GFreeFunc)item_list_free);
}

static void
nbt_interface_cpp_info_free (gpointer mem)
{
    common_info_free (mem, dh_nbt_instance_cpp_free);
}

static void
common_info_single_free (gpointer mem)
{
    CommonInfoSingle *single = mem;
    single->full_free = TRUE;
    // if(single->uuid_list)
    // {
    if (single->table)
        dh_mt_table_destroy (single->table);
    g_list_free (single->uuid_list);
    if (single->uuid_array)
        dh_str_array_free (single->uuid_array);
    g_ptr_array_free (single->update_notifiers, TRUE);
    // }
    g_free (mem);
}

static gboolean
is_same_string (gconstpointer a, gconstpointer b)
{
    if (strcmp (a, b))
        return FALSE;
    else
        return TRUE;
}

void
common_infos_init ()
{
    DhInfoTypes type_num = N_TYPES;
    infos = g_ptr_array_new_with_free_func (common_info_single_free);
    for (int i = 0; i < N_TYPES; i++)
        {
            CommonInfoSingle *instance = g_new0 (CommonInfoSingle, 1);
            if (i == DH_TYPE_Region)
                instance->table = dh_mt_table_new (g_str_hash, is_same_string,
                                                   g_free, region_info_free);
            else if (i == DH_TYPE_Item_List)
                instance->table = dh_mt_table_new (
                    g_str_hash, is_same_string, g_free, item_list_info_free);
            else if (i == DH_TYPE_NBT_INTERFACE_CPP)
                instance->table
                    = dh_mt_table_new (g_str_hash, is_same_string, g_free,
                                       nbt_interface_cpp_info_free);
            else
                instance->table = NULL;
            instance->uuid_list = NULL;
            instance->uuid_array = NULL;
            instance->update_notifiers
                = g_ptr_array_new_with_free_func (g_free);
            g_ptr_array_add (infos, instance);
        }
}

void
common_infos_free ()
{
    g_ptr_array_free (infos, TRUE);
}

gboolean
common_info_new (DhInfoTypes type, void *data, GDateTime *time,
                 const gchar *description)
{
    CommonInfo *info = g_new0 (CommonInfo, 1);
    gboolean ret = FALSE;
    g_rw_lock_init (&info->info_lock);
    g_rw_lock_writer_lock (&info->info_lock);

    info->data = data;
    info->time = time;
    info->description = g_strdup (description);

    CommonInfoSingle *instance = infos->pdata[type];
    DhMTTable *table = instance->table;
    gchar *uuid = g_uuid_string_random ();
    ret = dh_mt_table_insert (table, uuid, info);
    instance->uuid_list = g_list_append (instance->uuid_list, uuid);
    g_rw_lock_writer_unlock (&info->info_lock);
    update (instance);
    return ret;
}

gboolean
common_info_list_remove_item (DhInfoTypes type, const gchar *uuid)
{
    CommonInfoSingle *instance = infos->pdata[type];
    CommonInfo *info = dh_mt_table_lookup (instance->table, uuid);
    if (g_rw_lock_writer_trylock (&info->info_lock))
        {
            instance->uuid_list = g_list_remove (instance->uuid_list, uuid);
            gboolean ret = dh_mt_table_remove (instance->table, uuid);
            return ret;
        }
    else
        return FALSE;
}

static CommonInfo *
common_info_list_get_common_info (DhInfoTypes type, const gchar *uuid)
{
    CommonInfoSingle *instance = infos->pdata[type];
    CommonInfo *info = dh_mt_table_lookup (instance->table, uuid);
    return info;
}

void *
common_info_get_data (DhInfoTypes type, const gchar *uuid)
{
    CommonInfo *info = common_info_list_get_common_info (type, uuid);
    return info->data;
}

GDateTime *
common_info_get_time (DhInfoTypes type, const gchar *uuid)
{
    CommonInfo *info = common_info_list_get_common_info (type, uuid);
    return info->time;
}

gchar *
common_info_get_description (DhInfoTypes type, const gchar *uuid)
{
    CommonInfo *info = common_info_list_get_common_info (type, uuid);
    return info->description;
}

void
common_info_reset_description (DhInfoTypes type, const gchar *uuid,
                               const gchar *description)
{
    CommonInfo *info = common_info_list_get_common_info (type, uuid);
    g_free (info->description);
    info->description = g_strdup (description);
    update (infos->pdata[type]);
}

gboolean
common_info_reader_trylock (DhInfoTypes type, const gchar *uuid)
{
    CommonInfo *info = common_info_list_get_common_info (type, uuid);
    return g_rw_lock_reader_trylock (&info->info_lock);
}

void
common_info_reader_unlock (DhInfoTypes type, const gchar *uuid)
{
    CommonInfo *info = common_info_list_get_common_info (type, uuid);
    g_rw_lock_reader_unlock (&info->info_lock);
}

gboolean
common_info_writer_trylock (DhInfoTypes type, const gchar *uuid)
{
    CommonInfo *info = common_info_list_get_common_info (type, uuid);
    return g_rw_lock_writer_trylock (&info->info_lock);
}

void
common_info_writer_unlock (DhInfoTypes type, const gchar *uuid)
{
    CommonInfo *info = common_info_list_get_common_info (type, uuid);
    g_rw_lock_writer_unlock (&info->info_lock);
}

gboolean
common_info_update_data (DhInfoTypes type, const gchar *uuid, void *data)
{
    CommonInfoSingle *instance = infos->pdata[type];
    CommonInfo *info = dh_mt_table_lookup (instance->table, uuid);
    info->data = data;

    update (instance);
    return TRUE;
}

const GList *
common_info_list_get_uuid_list (DhInfoTypes type)
{
    CommonInfoSingle *instance = infos->pdata[type];
    return instance->uuid_list;
}

void
common_info_list_set_uuid (DhInfoTypes type, const char *uuid)
{
    CommonInfoSingle *instance = infos->pdata[type];
    if (instance->uuid_array)
        {
            dh_str_array_free (instance->uuid_array);
            instance->uuid_array = NULL;
        }
    dh_str_array_add_str (&(instance->uuid_array), uuid);
}

const char *
common_info_list_get_uuid (DhInfoTypes type)
{
    CommonInfoSingle *instance = infos->pdata[type];
    if (instance->uuid_array && instance->uuid_array->num == 1)
        return instance->uuid_array->val[0];
    else
        return NULL;
}

void
common_info_list_set_multi_uuid (DhInfoTypes type, const char **arr)
{
    CommonInfoSingle *instance = infos->pdata[type];
    if (instance->uuid_array)
        {
            dh_str_array_free (instance->uuid_array);
            instance->uuid_array = NULL;
        }
    for (; arr && *arr; arr++)
        dh_str_array_add_str (&(instance->uuid_array), *arr);
}

char **
common_info_list_get_multi_uuid (DhInfoTypes type)
{
    CommonInfoSingle *instance = infos->pdata[type];
    return dh_str_array_dup_to_plain (instance->uuid_array);
}

void
common_info_list_add_update_notifier (DhInfoTypes type, void *main_class,
                                      DhUpdateFunc func)
{
    UpdateNotifier *un = g_new0 (UpdateNotifier, 1);
    un->main_class = main_class;
    un->func = func;
    CommonInfoSingle *instance = infos->pdata[type];
    g_ptr_array_add (instance->update_notifiers, un);
}

void
common_info_list_remove_update_notifier (DhInfoTypes type, void *main_class)
{
    CommonInfoSingle *instance = infos->pdata[type];
    for (int i = 0; i < instance->update_notifiers->len; i++)
        {
            UpdateNotifier *un = instance->update_notifiers->pdata[i];
            if (un->main_class == main_class)
                {
                    g_ptr_array_remove (instance->update_notifiers, un);
                    break;
                }
        }
}