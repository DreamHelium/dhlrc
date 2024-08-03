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
extern "C" {
#endif

typedef struct _Region Region;

typedef struct Pos{
    int x;
    int y;
    int z;
} Pos;

typedef Pos RegionSize;

typedef struct BlockInfo{
    int index;
    Pos* pos;
    char* id_name;
    int palette;
} BlockInfo;

typedef struct Palette{
    char* id_name;
    DhStrArray* property_name;
    DhStrArray* property_data;
} Palette;

/** Just like `GPtrArray<BlockInfo>` */
typedef GPtrArray BlockInfoArray;
/** Just like `GPtrArray<Palette>` */
typedef GPtrArray PaletteArray;
/** Just like `GList<Region>` */
typedef GList RegionInfo;

typedef struct _Region
{
    /** The size of region */
    RegionSize* region_size;
    /** The block info array */
    BlockInfoArray* block_info_array;
    /** The Palette info array*/
    PaletteArray* palette_array;
} Region;

Region* region_new_from_lite_region(LiteRegion* lr);
Region* region_new_from_nbt(NBT* root);
ItemList* item_list_new_from_region(Region* region);
void region_free(Region* region);

#ifdef __cplusplus
}
#endif


#endif /* REGION_H */