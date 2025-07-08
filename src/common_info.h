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

#ifndef COMMON_INFO_H
#define COMMON_INFO_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum DhInfoTypes{
    DH_TYPE_Region,
    DH_TYPE_Item_List,
    DH_TYPE_NBT_INTERFACE_CPP,
    N_TYPES
} DhInfoTypes;

typedef void (*DhUpdateFunc)(void*);

typedef struct UpdateNotifier{
    void* main_class;
    DhUpdateFunc func;
} UpdateNotifier;

gboolean common_info_new(DhInfoTypes type, void* data, GDateTime* time, const gchar* description);
void     common_infos_init();
void     common_infos_free();
gboolean common_info_list_remove_item(DhInfoTypes type, const gchar* uuid);
void*    common_info_get_data(DhInfoTypes type, const gchar* uuid);
GDateTime* common_info_get_time(DhInfoTypes type, const gchar* uuid);
gchar*     common_info_get_description(DhInfoTypes type, const gchar* uuid);
void     common_info_reset_description(DhInfoTypes type, const gchar* uuid, const gchar* description);
gboolean common_info_reader_trylock(DhInfoTypes type, const gchar* uuid);
void     common_info_reader_unlock(DhInfoTypes type, const gchar* uuid);
gboolean common_info_writer_trylock(DhInfoTypes type, const gchar* uuid);
void     common_info_writer_unlock(DhInfoTypes type, const gchar *uuid);
gboolean common_info_update_data(DhInfoTypes type, const gchar* uuid, void* data);
const GList* common_info_list_get_uuid_list(DhInfoTypes type);

void     common_info_list_set_uuid(DhInfoTypes type, const char* uuid);
const char* common_info_list_get_uuid(DhInfoTypes type);
void     common_info_list_set_multi_uuid(DhInfoTypes type, const char** arr);
char** common_info_list_get_multi_uuid(DhInfoTypes type);

void common_info_list_add_update_notifier(DhInfoTypes type, void* main_class, DhUpdateFunc func);
void common_info_list_remove_update_notifier(DhInfoTypes type, void* main_class);

G_END_DECLS

#endif /* COMMON_INFO_H */