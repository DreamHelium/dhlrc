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
static guint list_id = 0;

typedef struct IlInfo {
    ItemList* il;
    GDateTime* time;
    gchar* description;
} IlInfo;

static void ilinfo_free(gpointer data)
{
    IlInfo* info = data;
    item_list_free(info->il);
    g_date_time_unref(info->time);
    g_free(info->description);
    g_free(info);
}

void il_info_list_free()
{
    g_list_free_full(il_info_list, ilinfo_free);
}

void il_info_new(ItemList *il, GDateTime *time, const gchar *description)
{
    IlInfo* info = g_new0(IlInfo, 1);
    info->il = il;
    info->time = time;
    info->description = g_strdup(description);
    il_info_list = g_list_append(il_info_list, info);
}

guint il_info_list_get_length()
{
    return g_list_length(il_info_list);
}

void il_info_list_set_id(guint id)
{
    guint len = il_info_list_get_length();
    if(id >= 0 && id < len) list_id = id;
}

ItemList* il_info_get_item_list()
{
    IlInfo* info = g_list_nth_data(il_info_list, list_id);
    return info->il;
}

void il_info_update_item_list(ItemList *il)
{
    IlInfo* info = g_list_nth_data(il_info_list, list_id);
    info->il = il;
}

GDateTime* il_info_get_time()
{
    IlInfo* info = g_list_nth_data(il_info_list, list_id);
    return info->time;
}

gchar* il_info_get_description()
{
    IlInfo* info = g_list_nth_data(il_info_list, list_id);
    return info->description;
}

void il_info_list_remove_item(guint id)
{
    IlInfo* info = g_list_nth_data(il_info_list, id);
    ilinfo_free(info);
    il_info_list = g_list_remove(il_info_list, info);
}

guint il_info_list_get_id()
{
    return list_id;
}