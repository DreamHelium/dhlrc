/*  nbt_pos - NBT_Pos struct
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


#ifndef NBT_POS_H
#define NBT_POS_H

#include "nbt_interface/libnbt/nbt.h"

typedef struct NbtPos
{
    /** The level of the pos in the NBT */
    int level;
    /** The pos of child in the NBT, len: level */
    int* child;
    /** NBT tree, len: level + 1 (include root as the first when level is 0) */
    NBT** tree;
    /** Current NBT */
    NBT* current;
    /** Represent the item in the latest tree */
    int item;
}
NbtPos;

NbtPos* nbt_pos_init(NBT* root);
int     nbt_pos_add_to_tree(NbtPos* pos, int n);
int     nbt_pos_delete_last(NbtPos* pos);
/* It might be slightly different from libnbt's get child */
int     nbt_pos_get_child(NbtPos* pos, const char* key);
int     nbt_pos_get_child_deep(NbtPos* pos, ...);
NbtPos* nbt_pos_copy(NbtPos* pos);
void    nbt_pos_free(NbtPos* pos);
NBT*    nbt_pos_get_item_nbt(NbtPos* pos, const char* key);

#endif // NBT_POS_H
