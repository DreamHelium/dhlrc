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

G_BEGIN_DECLS

typedef struct Pos
{
  int x;
  int y;
  int z;
} Pos;

typedef struct BlockEntity
{
  Pos *pos;
  void *nbt_instance;
  /* Will use */
  NbtNode *nbt_node;
} BlockEntity;

typedef Pos RegionSize;

typedef struct Palette
{
  char *id_name;
  DhStrArray *property_name;
  DhStrArray *property_data;
} Palette;

/** Just like `GPtrArray<Palette>` */
typedef GPtrArray PaletteArray;
/** Just like `GPtrArray<BlockEntity>` */
typedef GPtrArray BlockEntityArray;

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

typedef struct Region
{
  /** The base information */
  int data_version;
  BaseData *data;
  /** The size of region */
  RegionSize *region_size;
  /** The block info array */
  gint64 *block_array;
  int block_array_len;
  /** Block Entity Array */
  BlockEntityArray *block_entity_array;
  /** The Palette info array*/
  PaletteArray *palette_array;
  int air_palette;
} Region;

int region_get_index (Region *region, int x, int y, int z);
gboolean file_is_new_schem (void *instance_ptr);
gboolean region_node_is_new_schem (const NbtNode *root);
Region *region_new_from_new_schem (void *instance_ptr);
Region *region_new_from_lite_region (LiteRegion *lr);
Region *region_new_from_nbt_node (NbtNode *root, DhProgressFullSet func,
                                  void *main_klass, GCancellable *cancellable);
Region *region_new_from_nbt_file (const char *filepos);
Region *region_new_from_nbt_instance_ptr (void *instance_ptr);
Region *region_new_from_nbt_instance_ptr_full (void *instance_ptr,
                                               DhProgressFullSet func,
                                               void *main_klass,
                                               GCancellable *cancellable);
gboolean palette_is_same (gconstpointer a, gconstpointer b);
char *region_get_id_name (Region *region, int index);
gboolean region_add_palette (Region *region, const char *id_name,
                             DhStrArray *palette_name,
                             DhStrArray *palette_data);
gboolean region_add_palette_using_palette (Region *region, Palette *palette);
Palette *region_get_palette (Region *region, int val);
ItemList *item_list_new_from_multi_region (const char **region_uuid_arr);
void region_free (void *region);
int region_get_block_palette (Region *region, int index);
BlockEntity *region_get_block_entity (Region *region, int x, int y, int z);

G_END_DECLS

#endif /* REGION_H */