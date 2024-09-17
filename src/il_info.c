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

#include "il_info.h"
#include "dh_mt_table.h"

static GList* il_info_list = NULL;
/* On the last stage we unlock all the locks and free full */
static gboolean full_free = FALSE;
static guint set_id = 0;

static DhMTTable* table = NULL;

static void ilinfo_free(gpointer data)
{
    IlInfo* info = data;
    /* Although item list info can be freed by outer function
     * We believe the programmer would not do this */
    if(full_free) g_rw_lock_writer_lock(&(info->info_lock));

    item_list_free(info->il);
    g_date_time_unref(info->time);
    g_free(info->description);
    info->freed = TRUE; /* Should never use this info */
    g_rw_lock_writer_unlock(&(info->info_lock));
    g_rw_lock_clear(&(info->info_lock));
    g_free(info);
}

static gboolean is_same_string(gconstpointer a, gconstpointer b)
{
    if(strcmp(a, b)) return FALSE;
    else return TRUE;
}

/* a is the IlInfo */
static int illist_compare(gconstpointer a, gconstpointer b)
{
    const IlInfo* info = a;
    return ((info->il) - (ItemList*)b);
}

void il_info_list_free()
{
    full_free = TRUE;
    dh_mt_table_destroy(table);
}

void il_info_new(ItemList *il, GDateTime *time, const gchar *description)
{
    IlInfo* info = g_new0(IlInfo, 1);
    /* Init the item info lock and ready to lock it */
    g_rw_lock_init(&(info->info_lock));
    g_rw_lock_writer_lock(&(info->info_lock));

    info->freed = FALSE;
    info->il = il;
    info->time = time;
    info->description = g_strdup(description);

    if(!table)
        table = dh_mt_table_new(g_str_hash, is_same_string, g_free, ilinfo_free);
    dh_mt_table_insert(table, g_uuid_string_random, info);
    /* Unlock the writer lock */
    g_rw_lock_writer_unlock(&(info->info_lock));
}

gboolean il_info_list_remove_item(gchar* uuid)
{
    IlInfo* info = dh_mt_table_lookup(table, uuid);
    if(g_rw_lock_writer_trylock(&(info->info_lock)))
    {
        gboolean ret = dh_mt_table_remove(table, uuid);
        return ret;
    }
    else return FALSE; /* Some function is using the il */
}

IlInfo* il_info_list_get_il_info(gchar* uuid)
{
    IlInfo* info = dh_mt_table_lookup(table, uuid);
    if(info && info->freed) return NULL;
    else return info;
}

void il_info_list_update_il(gchar* uuid, IlInfo* info)
{
    /* We believe this function is used in a lock function
     * If not we will not lock */
    dh_mt_table_replace(table, uuid, info);
}

void il_info_list_set_id(guint id)
{
    set_id = id;
}

guint il_info_list_get_id()
{
    return set_id;
}

GList* il_info_list_get_uuid_list()
{
    return dh_mt_table_get_keys(table);
}