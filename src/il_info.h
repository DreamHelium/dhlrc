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

#ifndef IL_INFO_H
#define IL_INFO_H

#include "dhlrc_list.h"

G_BEGIN_DECLS

guint il_info_list_get_length();
void il_info_list_set_id(guint id);
guint il_info_list_get_id();
void il_info_list_free();
void il_info_new(ItemList* il, GDateTime* time, const gchar* description);
ItemList* il_info_get_item_list();
GDateTime* il_info_get_time();
gchar* il_info_get_description();
void il_info_update_item_list(ItemList* il);
void il_info_list_remove_item(guint id);

G_END_DECLS

#endif /* IL_INFO_H */