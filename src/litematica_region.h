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

#include "libnbt/nbt.h"
#include "dhlrc_list.h"
#include <dhutil.h>
#include "nbt_litereader.h"

#ifdef __cplusplus
extern "C"{
#endif



/*************************************************************
 * Region Processing Stuff:
 * Get Region nums and names, processing blocks in the region.
 *************************************************************/

/** Infomation of a region */
typedef struct LiteRegion{

    /** Data Version */
    int data_version;

    /** Region name */
    char* name;

    /** Number of the region */
    int region_num;

    /** Block names and nums */
    DhStrArray* blocks;

    /** Replaced name of blocks */
    DhStrArray* replaced_blocks;

    /** Region NBT */
    NBT* region_nbt;

    /** Block Properties */
    NBT** block_properties;

    /** Region NBT Pos variable */
    NbtPos* region_pos;

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

LiteRegion* lite_region_create(NBT* root, int r_num);
void        lite_region_free(LiteRegion* lr);

/** Get region numbers in litematica file */
int          lite_region_num(NBT* root);
G_DEPRECATED_FOR(lite_region_name_array)
char**       lite_region_names(NBT* root, int rNum, int* err);
G_DEPRECATED
void         lite_region_free_names(char** region, int rNum);
/** Improved version to get region name */
DhStrArray* lite_region_name_array(NBT* root);

NBT*         lite_region_nbt_region(NBT* root,int r_num);
NBT*         lite_region_nbt_block_state_palette(NBT* root, int r_num);
NBT*         lite_region_nbt_specific_block_state_palette(NBT* root,int r_num,int id);
NBT*         lite_region_nbt_block_properties(LiteRegion* lr, int id);

/** Get block numbers in a region */
int           lite_region_block_num(NBT* root, int r_num);
G_DEPRECATED_FOR(lite_region_block_name_array)
char**        lite_region_block_names(NBT* root, int r_num ,int bNum);
/** Improved version to get block name */
DhStrArray*  lite_region_block_name_array(NBT* root, int r_num);

/** Directly into the BlockStatePalette (Do not need to go to child) */

uint64_t* lite_region_block_states_array(NBT* root, int r_num, int* len);

/** Improved version of getting id */
int  lite_region_block_id(LiteRegion* lr, uint64_t index);
/** A better way to get id */
int  lite_region_block_id_xyz(LiteRegion* lr, int x, int y, int z);
int* lite_region_size_array(NBT* root,int r_num);

/** Improved version of getting index */
uint64_t  lite_region_block_index(LiteRegion* lr, int x, int y, int z);
char*     lite_region_block_type(NBT* root,int r_num, int id);
ItemList* lite_region_item_list(NBT* root,int r_num);
/**  Make ItemList but not init item numbers (support extend) */
ItemList* lite_region_item_list_without_num(LiteRegion* lr, ItemList *o_il);
ItemList* lite_region_item_list_extend(NBT* root, int r_num, ItemList *oBlock, int print_process);
gboolean  lite_region_block_properties_equal(LiteRegion* lr, int id, char* key, char* val);

#ifdef __cplusplus
}
#endif

#endif // LITEMATICA_REGION_H
