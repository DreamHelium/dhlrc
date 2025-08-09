/*  region - Region Structure
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

#ifndef REGION_H
#define REGION_H

#include "litematica_region.h"
#include <glib.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct _Region Region;

    typedef struct Pos
    {
        int x;
        int y;
        int z;
    } Pos;

    typedef Pos RegionSize;

    typedef struct Palette
    {
        char *id_name;
        DhStrArray *property_name;
        DhStrArray *property_data;
    } Palette;

    typedef struct BlockInfo
    {
        int index;
        Pos *pos;
        int palette;
        void *nbt_instance;
    } BlockInfo;

    /** Just like `GPtrArray<BlockInfo>` */
    typedef GPtrArray BlockInfoArray;
    /** Just like `GPtrArray<Palette>` */
    typedef GPtrArray PaletteArray;

    typedef struct BaseData
    {
        /* Default: time of generated */
        GDateTime *create_time;
        /* Default: time of generated */
        GDateTime *modify_time;
        /* Default: "" */
        char *description;
        /* Default: username */
        char *author;
        /* Default: Converted */
        char *name;
    } BaseData;

    typedef struct _Region
    {
        /** The base information */
        int data_version;
        BaseData *data;
        /** The size of region */
        RegionSize *region_size;
        /** The block info array */
        BlockInfoArray *block_info_array;
        /** The Palette info array*/
        PaletteArray *palette_array;
        int air_palette;
    } Region;

    int region_get_index (Region *region, int x, int y, int z);
    gboolean file_is_new_schem (void *instance_ptr);
    Region *region_new_from_new_schem (void *instance_ptr);
    Region *region_new_from_lite_region (LiteRegion *lr);
    Region *region_new_from_nbt_file (const char *filepos);
    Region *region_new_from_nbt_instance_ptr (void *instance_ptr);
    gboolean palette_is_same (gconstpointer a, gconstpointer b);
    char *block_info_get_id_name (Region *region, BlockInfo *info);
    void region_modify_property (Region *region, BlockInfo *info,
                                 gboolean all_modify, DhStrArray *new_data);
    gboolean region_add_palette (Region *region, const char *id_name,
                                 DhStrArray *palette_name,
                                 DhStrArray *palette_data);
    gboolean region_add_palette_using_palette(Region* region, Palette* palette);
    Palette *region_get_palette (Region *region, int val);
    ItemList *item_list_new_from_multi_region (const char **region_uuid_arr);
    void region_free (void *region);

#ifdef __cplusplus
}
#endif

#endif /* REGION_H */