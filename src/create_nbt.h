/*  create_nbt - Create own NBT sturcture
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

#ifndef CREATE_NBT_H
#define CREATE_NBT_H

#include <glib-object.h>
#include "nbt_interface_cpp/libnbt/nbt.h"

#ifdef __cplusplus
extern "C"{
#endif

NBT* nbt_new(NBT_Tags tag, GValue* value, int len, const char* key);
NBT* nbt_dup(NBT* root);
NBT* nbt_rm(NBT* root, const char* node);

#ifdef __cplusplus
}
#endif

#endif