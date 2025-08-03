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

#include "dh_string_util.h"
#include "dhlrc_list.h"
#include "nbt_interface_cpp/libnbt/nbt.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*************************************************************
     * Region Processing Stuff:
     * Get Region nums and names, processing blocks in the region.
     *************************************************************/

    /** Information of a region */
    typedef struct _LiteRegion LiteRegion;
    typedef struct _Region Region;

    LiteRegion *lite_region_create_from_region (Region *region);
    void lite_region_free (LiteRegion *lr);

    /** Get region numbers in litematica file */
    int lite_region_num_instance (void *instance);
    G_DEPRECATED_FOR (lite_region_name_array)
    char **lite_region_names (NBT *root, int rNum, int *err);
    G_DEPRECATED
    void lite_region_free_names (char **region, int rNum);
    /** Improved version to get region name */
    DhStrArray *lite_region_name_array_instance (void *instance);

    /** Get block numbers in a region */
    DhStrArray *lite_region_block_name_array (LiteRegion *lr);

    int lite_region_data_version (LiteRegion *lr);
    int lite_region_size_x (LiteRegion *lr);
    int lite_region_size_y (LiteRegion *lr);
    int lite_region_size_z (LiteRegion *lr);
    const char *lite_region_name (LiteRegion *lr);
    gint64 lite_region_create_time (LiteRegion *lr);
    gint64 lite_region_modify_time (LiteRegion *lr);
    const char *lite_region_description (LiteRegion *lr);
    const char *lite_region_author (LiteRegion *lr);

    /** Improved version of getting id */
    int lite_region_block_id (LiteRegion *lr, uint64_t index);
    /** A better way to get id */
    int lite_region_block_id_xyz (LiteRegion *lr, int x, int y, int z);

    /** Improved version of getting index */
    uint64_t lite_region_block_index (LiteRegion *lr, int x, int y, int z);
    // ItemList* lite_region_item_list(NBT* root,int r_num);
    /**  Make ItemList but not init item numbers (support extend) */
    ItemList *lite_region_item_list_without_num (LiteRegion *lr,
                                                 ItemList *o_il);
    // ItemList* lite_region_item_list_extend(NBT* root, int r_num, ItemList
    // *oBlock, int print_process); gboolean
    // lite_region_block_properties_equal(LiteRegion* lr, int id, char* key,
    // char* val);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include "nbt_interface_cpp/nbt_interface.hpp"

DhNbtInstance lite_region_get_instance (LiteRegion *lr);
LiteRegion *lite_region_create_from_root_instance_cpp (DhNbtInstance root,
                                                       int r_num);

#endif

#endif // LITEMATICA_REGION_H
