/*  nbt_litereader - nbt lite reader
    Copyright (C) 2022 Dream Helium
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

#ifndef NBT_LITEREADER_H
#define NBT_LITEREADER_H
#ifdef __cplusplus
extern "C"{
#endif

#include "libnbt/nbt.h"
#include <dh/dh_string_util.h>
#include "nbt_pos.h"



/** Just use this to start a reader instance.
 *  Please pass NULL in "parent" to start. */
int         nbtlr_start(NBT* root);

int         nbtlr_start_pos(NbtPos* pos);
int         nbtlr_list(NBT* given_nbt, int read_next);
int         nbtlr_list_item(NBT* given_nbt);
NBT*        nbtlr_to_next_nbt(NBT* root, int n);

dh_LineOut *nbtlr_modifier_start(NBT* root, int modify_list);


#ifdef __cplusplus
}
#endif

#endif // NBT_LITEREADER_H
