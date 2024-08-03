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
#include "libnbt/nbt.h"
#include "litematica_region.h"

/* TODO and maybe never do, since property can be too much */
const char* property[] = {"", ""};
const char* data[] = {"", ""};

typedef struct TmpItem{
    gchar* name;
    guint total;
} TmpItem;

typedef GList TmpItemList;

static int tmpitem_strcmp(gconstpointer a, gconstpointer b)
{
    return strcmp( ((TmpItem*)a)->name , b );
}

static void tmp_item_list_add_num(TmpItemList** til, char* item_name)
{
    TmpItemList* il = g_list_find_custom(*til, item_name, tmpitem_strcmp);
    if(il)
    {
        TmpItem* ti = il->data;
        ti->total++;
    }
    else
    {
        TmpItem* ti = g_new0(TmpItem, 1);
        ti->name = g_strdup(item_name);
        ti->total = 1;
        *til = g_list_prepend(*til, ti);
    }
} 

static void tmpitem_free(gpointer ti)
{
    TmpItem* item = ti;
    g_free(item->name);
    g_free(item);
}

static void tmpitem_list_free(TmpItemList* til)
{
    g_list_free_full(til, tmpitem_free);
}

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

static PaletteArray* get_palette_full_info_from_nbt(NBT* root)
{
    GPtrArray* array = g_ptr_array_new_with_free_func(palette_free);
    NBT* palette_parent = NBT_GetChild(root, "palette");
    NBT* palette_nbt = palette_parent->child;
    for(; palette_nbt; palette_nbt = palette_nbt->next)
    {
        Palette* palette = g_new0(Palette, 1);
        palette->id_name = g_strdup(NBT_GetChild(palette_nbt, "Name")->value_a.value);

        NBT* property = NBT_GetChild(palette_nbt, "Properties");
        if(property) property = property->next;
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

static BlockInfoArray* get_block_full_info_from_nbt(NBT* root, PaletteArray* pa)
{
    GPtrArray* array = g_ptr_array_new_with_free_func(block_info_free);
    NBT* blocks_nbt = NBT_GetChild(root, "blocks")->child;
    for(; blocks_nbt ; blocks_nbt = blocks_nbt->next)
    {
        BlockInfo* block = g_new0(BlockInfo, 1);
        block->pos = g_new0(Pos, 1);
        block->index = 0; /* Temporary it's 0 */
        block->palette = NBT_GetChild(blocks_nbt, "state")->value_i;
        block->pos->x = NBT_GetChild(blocks_nbt, "pos")->child->value_i;
        block->pos->y = NBT_GetChild(blocks_nbt, "pos")->child->next->value_i;
        block->pos->z = NBT_GetChild(blocks_nbt, "pos")->child->next->next->value_i;
        Palette* block_palette = pa->pdata[block->palette];
        block->id_name = g_strdup(block_palette->id_name);
        g_ptr_array_add(array, block);
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

Region* region_new_from_nbt(NBT* root)
{
    Region* region = g_new0(Region, 1);

    /* Fill RegionSize */
    RegionSize* rs = g_new0(RegionSize, 1);
    NBT* size_nbt = NBT_GetChild(root, "size");
    size_nbt = size_nbt->child;
    rs->x = size_nbt->value_i;
    rs->y = size_nbt->next->value_i;
    rs->z = size_nbt->next->next->value_i;
    region->region_size = rs;

    /* Fill PaletteArray */
    PaletteArray* pa = get_palette_full_info_from_nbt(root);
    region->palette_array = pa;

    /* Fill BlockInfoArray */
    BlockInfoArray* bia = get_block_full_info_from_nbt(root, pa);
    region->block_info_array = bia;
    
    return region;
}

ItemList* item_list_new_from_region(Region* region)
{
    TmpItemList* til = NULL;
    for(int i = 0 ; i < region->block_info_array->len ; i++)
    {
        BlockInfo* bi = region->block_info_array->pdata[i];
        tmp_item_list_add_num(&til, bi->id_name);
    }

    gchar* description = "Add items from Region.";
    ItemList* oblock = NULL;

    /* Copy items to the ItemList */
    for(TmpItemList* tild = til; tild ; tild = tild->next)
    {
        TmpItem* data = tild->data;
        oblock = item_list_add_item(&oblock, data->total, data->name, description);
    }

    tmpitem_list_free(til);
    return oblock;
}