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

static GList* nbt_info_list = NULL;

typedef struct NbtInfo{
    NBT* root;
    GDateTime* time;
    gchar* description;
} NbtInfo;

static void nbt_info_free(gpointer info);

static void nbt_info_free(gpointer data)
{
    NbtInfo* info = data;
    NBT_Free(info->root);
    g_date_time_unref(info->time);
    g_free(info->description);
    g_free(info);
}

void nbt_info_list_clear()
{
    g_list_free_full(nbt_info_list, nbt_info_free);
}

void nbt_info_new(NBT *root, GDateTime *time, const gchar *description)
{
    NbtInfo* info = g_new0(NbtInfo, 1);
    info->root = root;
    info->time = time;
    info->description = g_strdup(description);
    nbt_info_list = g_list_append(nbt_info_list, info);
}

guint nbt_info_list_length()
{
    return g_list_length(nbt_info_list);
}

NBT* nbt_info_get_nbt(guint id)
{
    NbtInfo* info = g_list_nth_data(nbt_info_list, id);
    return info->root;
}

GDateTime* nbt_info_get_time(guint id)
{
    NbtInfo* info = g_list_nth_data(nbt_info_list, id);
    return info->time;
}

gchar* nbt_info_get_description(guint id)
{
    NbtInfo* info = g_list_nth_data(nbt_info_list, id);
    return info->description;
}