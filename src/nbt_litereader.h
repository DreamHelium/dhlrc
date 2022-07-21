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
#include "dh_string_util.h"

typedef struct NBT_Pos
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
NBT_Pos;

NBT_Pos* NBT_Pos_init(NBT* root);
int NBT_Pos_AddToTree(NBT_Pos* pos, int n);
int NBT_Pos_DeleteLast(NBT_Pos* pos);
/* It might be slightly different from libnbt's get child */
int NBT_Pos_GetChild(NBT_Pos* pos, const char* key);
int NBT_Pos_GetChild_Deep(NBT_Pos* pos, ...);
NBT_Pos* NBT_Pos_Copy(NBT_Pos* pos);
void NBT_Pos_Free(NBT_Pos* pos);
NBT* NBT_Pos_GetItem_NBT(NBT_Pos* pos, const char* key);

/** Just use this to start a reader instance.
 *  Please pass NULL in "parent" to start. */
int nbtlr_Start(NBT* root);

int nbtlr_Start_Pos(NBT_Pos* pos);
int nbtlr_List(NBT* given_nbt, int read_next);
int nbtlr_ListItem(NBT* given_nbt);
NBT* nbtlr_ToNextNBT(NBT* root, int n);

dh_LineOut *nbtlr_Modifier_Start(NBT* root, int modify_list);


#ifdef __cplusplus
}
#endif

#endif // NBT_LITEREADER_H
