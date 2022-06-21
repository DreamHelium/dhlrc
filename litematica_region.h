/*  litematica_region - the region utilities for litematic file
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

#ifndef LITEMATICA_REGION_H
#define LITEMATICA_REGION_H
#ifdef __cplusplus
extern "C"{
#endif

#include "libnbt/nbt.h"
#include "dhlrc_list.h"

// litematica processing stuff

/*************************************************************
 * Region Processing Stuff:
 * Get Region nums and names, processing blocks in the region.
 *************************************************************/

/** Get region numbers in litematica file */
int lite_region_Num(NBT* root);
char** lite_region_Name(NBT* root, int rNum, int* err);
void lite_region_FreeNameArray(char** region, int rNum);
NBT* lite_region_RegionNBT(NBT* root,int r_num);



NBT* lite_region_BlockStatePalette(NBT* root, int r_num);
int lite_region_BlockNum(NBT* root, int r_num);
char** lite_region_BlockNameArray(NBT* root, int r_num ,int bNum);
uint64_t* lite_region_BlockStatesArray(NBT* root, int r_num, int* len);
int lite_region_BlockArrayPos(NBT* root, int r_num, uint64_t index);
int* lite_region_SizeArray(NBT* root,int r_num);
uint64_t lite_region_BlockIndex(NBT* root, int r_num,int x, int y, int z);
NBT* lite_region_SpecificBlockStatePalette(NBT* root,int r_num,int id);
char* lite_region_BlockType(NBT* root,int r_num, int id);
ItemList* lite_region_ItemList(NBT* root,int r_num);
/** \todo Make ItemList but not init item numbers (support extend) */
ItemList* lite_region_ItemList_WithoutNum(NBT* root, int r_num, ItemList *o_il);
ItemList* lite_region_ItemListExtend(NBT* root, int r_num, ItemList *oBlock);
int lite_region_IsBlockWaterlogged(NBT* root,int r_num,int id);
int lite_region_BlockLevel(NBT* root,int r_num,int id);
char* lite_region_DoorHalf(NBT* root,int r_num,int id);


#ifdef __cplusplus
}
#endif

#endif // LITEMATICA_REGION_H
