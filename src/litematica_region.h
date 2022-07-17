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
#include "dh_string_util.h"

// litematica processing stuff

/*************************************************************
 * Region Processing Stuff:
 * Get Region nums and names, processing blocks in the region.
 *************************************************************/

/** Infomation of a region */
typedef struct LiteRegion{

    /** Region name */
    char* name;

    /** Number of the region */
    int region_num;

    /** Block names and nums */
    dh_StrArray* blocks;

    /** Replaced name of blocks */
    dh_StrArray* replaced_blocks;

    /** Region NBT */
    NBT* region_nbt;

    /** Block states */
    int64_t* states;

    /** numbers of BlockStates */
    int states_num;

    struct{
        int x;
        int y;
        int z;
    } region_size;

    /** In many cases you don't need it, it's used to get block id. */
    int move_bits;

} LiteRegion;

LiteRegion* LiteRegion_Create(NBT* root, int r_num);
void LiteRegion_Free(LiteRegion* lr);

/** Get region numbers in litematica file */
int lite_region_Num(NBT* root);
char** lite_region_Name(NBT* root, int rNum, int* err);
/** Improved version to get region name */
dh_StrArray* lite_region_Name_StrArray(NBT* root);


void lite_region_FreeNameArray(char** region, int rNum);
NBT* lite_region_RegionNBT(NBT* root,int r_num);

/** Get block numbers in a region */
int lite_region_BlockNum(NBT* root, int r_num);
char** lite_region_BlockNameArray(NBT* root, int r_num ,int bNum);
/** Improved version to get block name */
dh_StrArray* lite_region_BlockName_StrArray(NBT* root, int r_num);

/** Directly into the BlockStatePalette (Do not need to go to child) */
NBT* lite_region_BlockStatePalette(NBT* root, int r_num);

uint64_t* lite_region_BlockStatesArray(NBT* root, int r_num, int* len);

/** Get Block Position in array */
int lite_region_BlockArrayPos(NBT* root, int r_num, uint64_t index);
/** Improved version of getting blockarraypos */
int lite_region_BlockArrayPos_lr(LiteRegion* lr, uint64_t index);
/** A better way to get blockarraypos */
int lite_region_BlockArrayPos_ByCoordination(LiteRegion* lr, int x, int y, int z);


int* lite_region_SizeArray(NBT* root,int r_num);

/** Get block index */
uint64_t lite_region_BlockIndex(NBT* root, int r_num,int x, int y, int z);
/** Improved version of getting index */
uint64_t lite_region_BlockIndex_lr(LiteRegion* lr, int x, int y, int z);

NBT* lite_region_SpecificBlockStatePalette(NBT* root,int r_num,int id);
char* lite_region_BlockType(NBT* root,int r_num, int id);
ItemList* lite_region_ItemList(NBT* root,int r_num);
/**  Make ItemList but not init item numbers (support extend) */
ItemList* lite_region_ItemList_WithoutNum(LiteRegion* lr, ItemList *o_il);
ItemList* lite_region_ItemListExtend(NBT* root, int r_num, ItemList *oBlock);
int lite_region_IsBlockWaterlogged(NBT* root,int r_num,int id);
int lite_region_BlockLevel(NBT* root,int r_num,int id);
char* lite_region_DoorHalf(NBT* root,int r_num,int id);


#ifdef __cplusplus
}
#endif

#endif // LITEMATICA_REGION_H
