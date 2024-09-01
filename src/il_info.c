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

static GList* il_info_list = NULL;
/* On the last stage we unlock all the locks and free full */
static gboolean full_free = FALSE;
static guint set_id = 0;


static void ilinfo_free(gpointer data)
{
    IlInfo* info = data;
    /* Although item list info can be freed by outer function
     * We believe the programmer would not do this */
    if(full_free) g_rw_lock_writer_lock(&(info->info_lock));

    item_list_free(info->il);
    g_date_time_unref(info->time);
    g_free(info->description);
    g_free(info->uuid);
    info->freed = TRUE; /* Should never use this info */
    g_rw_lock_writer_unlock(&(info->info_lock));
    g_rw_lock_clear(&(info->info_lock));
    g_free(info);
}

/* a is the IlInfo */
static int illist_compare(gconstpointer a, gconstpointer b)
{
    const IlInfo* info = a;
    return ((info->il) - (ItemList*)b);
}

static int uuid_compare(gconstpointer a, gconstpointer b)
{
    const IlInfo* info = a;
    return strcmp(info->uuid, b);
}

void il_info_list_free()
{
    full_free = TRUE;
    g_list_free_full(il_info_list, ilinfo_free);
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
    info->uuid = g_uuid_string_random();

    il_info_list = g_list_append(il_info_list, info);
    /* Unlock the writer lock */
    g_rw_lock_writer_unlock(&(info->info_lock));
}

guint il_info_list_get_length()
{
    return g_list_length(il_info_list);
}

gboolean il_info_list_remove_item(guint id)
{
    IlInfo* info = g_list_nth_data(il_info_list, id);
    if(g_rw_lock_writer_trylock(&(info->info_lock)))
    {
        ilinfo_free(info);
        il_info_list = g_list_remove(il_info_list, info);
        return TRUE;
    }
    else return FALSE; /* Some function is using the il */
}

IlInfo* il_info_list_get_il_info(guint id)
{
    IlInfo* info = g_list_nth_data(il_info_list, id);
    if(info->freed) return NULL;
    else return info;
}

void il_info_list_update_il(IlInfo* info)
{
    /* We believe this function is used in a lock function
     * If not we will not lock */
    GList* old_info = g_list_find_custom(il_info_list, info->uuid, uuid_compare);
    old_info->data = info;
}

void il_info_list_set_id(guint id)
{
    set_id = id;
}

guint il_info_list_get_id()
{
    return set_id;
}