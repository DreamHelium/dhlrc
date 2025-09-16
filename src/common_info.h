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

#include "dh_type.h"
#include <glib.h>

G_BEGIN_DECLS

#define DH_TYPE_REGION dh_region_get_type ()
#define DH_TYPE_ITEM_LIST dh_item_list_get_type ()
#define DH_TYPE_NBT_INTERFACE_CPP dh_nbt_interface_cpp_get_type ()
#define DH_TYPE_MODULE dh_module_get_type ()

DH_GET_TYPE (dh, region)
DH_GET_TYPE (dh, item_list)
DH_GET_TYPE (dh, nbt_interface_cpp)
DH_GET_TYPE (dh, module)

G_END_DECLS

#endif /* COMMON_INFO_H */