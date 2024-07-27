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
    
#include "region.h"
#include "dh_string_util.h"
#include "litematica_region.h"

static void palette_free(gpointer mem);
static void block_info_free(gpointer mem);

void region_free(Region* region)
{
    g_free(region->region_size);
    g_ptr_array_unref(region->block_info_array);
    g_ptr_array_unref(region->palette_array);
    g_free(region);
}

static void palette_free(gpointer mem)
{
    Palette* palette = mem;
    g_free(palette->id_name);
    dh_str_array_free(palette->property_name);
    dh_str_array_free(palette->property_data);
    g_free(mem);
}

static DhStrArray* get_property_name_from_nbt(NBT* root)
{
    DhStrArray* array = NULL;
    if(root == NULL)
        return NULL;
    else
    {
        for(; root ; root = root->next)
        {
            dh_str_array_add_str(&array, root->key);
        }
    }
    return array;
}

static DhStrArray* get_property_data_from_nbt(NBT* root)
{
    DhStrArray* array = NULL;
    if(root == NULL)
        return NULL;
    else
    {
        for(; root ; root = root->next)
        {
            dh_str_array_add_str(&array, root->value_a.value);
        }
    }
    return array;
}

static PaletteArray* get_palette_full_info_from_lr(LiteRegion* lr)
{
    GPtrArray* array = g_ptr_array_new_with_free_func(palette_free);
    for(int i = 0 ; i < lr->blocks->num; i++)
    {
        Palette* palette = g_new0(Palette, 1);
        palette->id_name = g_strdup(lr->blocks->val[i]);

        NBT* property = lr->block_properties[i];
        DhStrArray* name = get_property_name_from_nbt(property);
        DhStrArray* data = get_property_data_from_nbt(property);

        palette->property_name = name;
        palette->property_data = data;

        g_ptr_array_add(array, palette);
    }
    return array;
}

static void block_info_free(gpointer mem)
{
    BlockInfo* info = mem;
    g_free(info->pos);
    g_free(info->id_name);
    g_free(mem);
}

static BlockInfoArray* get_block_full_info_from_lr(LiteRegion* lr)
{
    GPtrArray* array = g_ptr_array_new_with_free_func(block_info_free);
    for(int y = 0 ; y < lr->region_size.y ; y++)
    {
        for(int z = 0 ; z < lr->region_size.z ; z++)
        {
            for(int x = 0; x < lr->region_size.x ; x++)
            {
                BlockInfo* block = g_new0(BlockInfo, 1);
                block->pos = g_new0(Pos, 1);
                block->index = lite_region_block_index(lr, x, y, z);
                block->palette = lite_region_block_id(lr, block->index);
                block->pos->x = x;
                block->pos->y = y;
                block->pos->z = z;
                block->id_name = g_strdup(lr->blocks->val[block->palette]);
                g_ptr_array_add(array, block);
            }
        }
    }
    return array;
}

Region* region_new_from_lite_region(LiteRegion *lr)
{
    Region* region = g_new0(Region, 1);

    /* Fill RegionSize */
    RegionSize* rs = g_new0(RegionSize, 1);
    rs->x = lr->region_size.x;
    rs->y = lr->region_size.y;
    rs->z = lr->region_size.z;
    region->region_size = rs;

    /* Fill PaletteArray */
    PaletteArray* pa = get_palette_full_info_from_lr(lr);
    region->palette_array = pa;

    /* Fill BlockInfoArray */
    BlockInfoArray* bia = get_block_full_info_from_lr(lr);
    region->block_info_array = bia;

    return region;
}