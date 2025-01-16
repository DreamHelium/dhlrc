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
#include "common_info.h"
#include "create_nbt.h"
#include "dh_string_util.h"
#include "dhlrc_list.h"
#include "glibconfig.h"
#include "libnbt/nbt.h"
#include "litematica_region.h"
#include "nbt_interface/nbt_interface.h"
#include <time.h>

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
        if(property) property = property->child;
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
    if(info->nbt) NBT_Free(info->nbt);
    dh_nbt_instance_free_only_instance(info->instance);
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
                block->nbt = NULL;
                /* WARNING: This is temporary */
                block->instance = dh_nbt_instance_new_from_real_nbt((RealNbt*)block->nbt);
                block->id_name = g_strdup(lr->blocks->val[block->palette]);
                g_ptr_array_add(array, block);
            }
        }
    }

    NBT* tile_entities = NBT_GetChild(lr->region_nbt, "TileEntities")->child;
    for(; tile_entities ; tile_entities = tile_entities->next)
    {
        gint64 x = NBT_GetChild(tile_entities, "x")->value_i;
        gint64 y = NBT_GetChild(tile_entities, "y")->value_i;
        gint64 z = NBT_GetChild(tile_entities, "z")->value_i;
        NBT* new_tile_entities = nbt_dup(tile_entities);
        new_tile_entities = nbt_rm(new_tile_entities, "x");
        new_tile_entities = nbt_rm(new_tile_entities, "y");
        new_tile_entities = nbt_rm(new_tile_entities, "z");
        gint64 index = lite_region_block_index(lr, x, y, z);
        ((BlockInfo*)array->pdata[index])->nbt = new_tile_entities;
        new_tile_entities->key = dh_strdup("nbt");
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
        block->nbt = nbt_dup(NBT_GetChild(blocks_nbt, "nbt"));
        /* WARNING: This is a temporary thing */
        block->instance = dh_nbt_instance_new_from_real_nbt((RealNbt*)block->nbt);
        Palette* block_palette = pa->pdata[block->palette];
        block->id_name = g_strdup(block_palette->id_name);
        g_ptr_array_add(array, block);
    }
    return array;
}

Region* region_new_from_lite_region(LiteRegion *lr)
{
    Region* region = g_new0(Region, 1);

    /* Fill DataVersion */
    region->data_version = lr->data_version; 

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
    if(size_nbt)
    {
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

        /* Fill Data Version */
        NBT* data_version = NBT_GetChild(root, "DataVersion");
        region->data_version = data_version->value_i;
        
        return region;
    }
    else 
    {
        g_free(region);
        g_free(rs);
        return NULL;
    }
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

ItemList* item_list_new_from_multi_region(const char** region_uuid_arr)
{
    ItemList* ret = NULL;
    for( ; *region_uuid_arr ; region_uuid_arr++)
    {
        Region* region = (Region*)common_info_get_data(DH_TYPE_Region, *region_uuid_arr);
        ItemList* i_il = item_list_new_from_region(region);
        item_list_combine(&ret, i_il);
        item_list_free(i_il);
    }
    return ret;
}

static NBT* size_nbt_new(Pos* pos, const char* key)
{
    GValue val = {0};
    g_value_init(&val, G_TYPE_INT64);
    g_value_set_int64(&val, pos->x);
    NBT* x = nbt_new(TAG_Int, &val, 0, NULL);
    g_value_set_int64(&val, pos->y);
    NBT* y = nbt_new(TAG_Int, &val, 0, NULL);
    g_value_set_int64(&val, pos->z);
    NBT* z = nbt_new(TAG_Int, &val, 0, NULL);
    g_value_unset(&val);

    x->next = y;
    y->prev = x;
    y->next = z;
    z->prev = y;

    g_value_init(&val, G_TYPE_POINTER);
    g_value_set_pointer(&val, x);
    NBT* ret = nbt_new(TAG_List, &val, 3, key);
    return ret;
}

static NbtInstance* size_nbt_instance_new(Pos* pos, const char* key)
{
    NbtInstance* x = dh_nbt_instance_new_int(pos->x, NULL);
    NbtInstance* y = dh_nbt_instance_new_int(pos->y, NULL);
    NbtInstance* z = dh_nbt_instance_new_int(pos->z, NULL);
    dh_nbt_instance_fill_next(x, y);
    dh_nbt_instance_fill_prev(y, x);
    dh_nbt_instance_fill_next(y, z);
    dh_nbt_instance_fill_prev(z, y);

    NbtInstance* ret = dh_nbt_instance_new_list(key);
    dh_nbt_instance_fill_child(ret, x);

    dh_nbt_instance_free_only_instance(x);
    dh_nbt_instance_free_only_instance(y);
    dh_nbt_instance_free_only_instance(z);

    return ret;
}

static NBT* entities_nbt_new()
{
    GValue val = {0};
    g_value_init(&val, G_TYPE_POINTER);
    g_value_set_pointer(&val, NULL);
    NBT* ret = nbt_new(TAG_List, &val, 0, "entities");
    return ret;
}

static NbtInstance* entities_nbt_instance_new()
{
    return dh_nbt_instance_new_list("entities");
}

static NBT* data_version_nbt_new(gint64 version)
{
    GValue val = {0};
    g_value_init(&val, G_TYPE_INT64);
    g_value_set_int64(&val, version);
    NBT* ret = nbt_new(TAG_Int, &val, 0, "DataVersion");
    return ret;
}

static NbtInstance* data_version_nbt_instance_new(gint64 version)
{
    return dh_nbt_instance_new_int(version, "DataVersion");
}

static NBT* block_nbt_new(BlockInfo* info)
{
    NBT* nbt = nbt_dup(info->nbt);
    NBT* pos = size_nbt_new(info->pos, "pos");
    pos->prev = nbt;
    if(nbt)  nbt->next = pos;
    GValue val = {0};
    g_value_init(&val, G_TYPE_INT64);
    g_value_set_int64(&val, info->palette);
    NBT* state = nbt_new(TAG_Int, &val, 0, "state");
    pos->next = state;
    pos->next->prev = pos;
    g_value_unset(&val);

    g_value_init(&val, G_TYPE_POINTER);
    g_value_set_pointer(&val, nbt? nbt : pos);
    NBT* ret = nbt_new(TAG_Compound, &val, 2, NULL);
    return ret;
}

static NbtInstance* block_nbt_instance_new(BlockInfo* info)
{
    NbtInstance* nbt = dh_nbt_instance_dup(info->instance);
    NbtInstance* pos = size_nbt_instance_new(info->pos, "pos");
    dh_nbt_instance_fill_prev(pos, nbt);
    if(dh_nbt_instance_is_non_null(nbt))
        dh_nbt_instance_fill_next(nbt, pos);
    NbtInstance* state = dh_nbt_instance_new_int(info->palette, "state");
    dh_nbt_instance_fill_next(pos, state);
    dh_nbt_instance_fill_prev(state, pos);
    NbtInstance* ret = dh_nbt_instance_new_compound(NULL);
    dh_nbt_instance_fill_child(ret, dh_nbt_instance_is_non_null(nbt)? nbt : pos);

    dh_nbt_instance_free_only_instance(nbt);
    dh_nbt_instance_free_only_instance(pos);
    dh_nbt_instance_free_only_instance(state);

    return ret;
}

static NBT* blocks_nbt_new(BlockInfoArray* array)
{
    NBT* head = NULL;
    NBT* prev = NULL;
    NBT* cur = NULL;
    for(int i = 0 ; i < array->len ; i++)
    {
        cur = block_nbt_new(array->pdata[i]);
        cur->prev = prev;
        if(prev) prev->next = cur;
        if(i == 0) head = cur;
        prev = cur;
    }

    GValue val = {0};
    g_value_init(&val, G_TYPE_POINTER);
    g_value_set_pointer(&val, head);
    NBT* ret = nbt_new(TAG_List, &val, 0, "blocks");
    return ret;
}

static NbtInstance* blocks_nbt_instance_new(BlockInfoArray* array)
{
    NbtInstance* head = dh_nbt_instance_new_from_real_nbt(NULL);
    NbtInstance* prev = dh_nbt_instance_new_from_real_nbt(NULL);
    NbtInstance* cur = NULL;
    for(int i = 0 ; i < array->len ; i++)
    {
        cur = block_nbt_instance_new(array->pdata[i]);
        /* Set head */
        if(i == 0)
        {
            dh_nbt_instance_free(head);
            head = cur;
        } 
        dh_nbt_instance_fill_prev(cur, prev);
        if(dh_nbt_instance_is_non_null(prev))
            dh_nbt_instance_fill_next(prev, cur);
        /* Old prev is of no use */
        if(head != prev) dh_nbt_instance_free_only_instance(prev);
        prev = cur;
    }

    NbtInstance* ret = dh_nbt_instance_new_list("blocks");
    dh_nbt_instance_fill_child(ret, head);
    if(prev != head) dh_nbt_instance_free_only_instance(head);
    dh_nbt_instance_free_only_instance(prev);
    return ret;
}

static NBT* properties_nbt_new(DhStrArray* name, DhStrArray* data)
{
    NBT* head = NULL;
    NBT* prev = NULL;
    NBT* cur = NULL;
    if(!name) return NULL;
    for(int i = 0 ; i < name->num ; i++)
    {
        GValue val = {0};
        g_value_init(&val, G_TYPE_STRING);
        g_value_set_string(&val, data->val[i]);
        cur = nbt_new(TAG_String, &val, 0, name->val[i]);
        cur->prev = prev;
        if(prev) prev->next = cur;
        if(i == 0) head = cur;
        prev = cur;
        g_value_unset(&val);
    }

    if(!head) return NULL;
    GValue val = {0};
    g_value_init(&val, G_TYPE_POINTER);
    g_value_set_pointer(&val, head);
    NBT* ret = nbt_new(TAG_Compound, &val, 0, "Properties");
    return ret;
}

static NbtInstance* properties_nbt_instance_new(DhStrArray* name, DhStrArray* data)
{
    NbtInstance* head = dh_nbt_instance_new_from_real_nbt(NULL);
    NbtInstance* prev = dh_nbt_instance_new_from_real_nbt(NULL);
    NbtInstance* cur = NULL;

    for(int i = 0 ; name && i < name->num ; i++)
    {
        cur = dh_nbt_instance_new_string(data->val[i], name->val[i]);
        if(i == 0)
        {
            dh_nbt_instance_free(head);
            head = cur;
        }
        dh_nbt_instance_fill_prev(cur, prev);
        if(dh_nbt_instance_is_non_null(prev))
            dh_nbt_instance_fill_next(prev, cur);
        /* Old prev is of no use */
        if(prev != head) dh_nbt_instance_free_only_instance(prev);
        prev = NULL;
        prev = cur;
    }

    if(!name) 
    {
        dh_nbt_instance_free(prev);
        return head;
    }
    NbtInstance* ret = dh_nbt_instance_new_compound("Properties");
    dh_nbt_instance_fill_child(ret, head);
    if(prev != head) dh_nbt_instance_free_only_instance(head);
    dh_nbt_instance_free_only_instance(prev);
    return ret;
}

static NBT* palette_nbt_new(Palette* palette)
{
    NBT* properties = properties_nbt_new(palette->property_name, palette->property_data);
    GValue val = {0};
    g_value_init(&val, G_TYPE_STRING);
    g_value_set_string(&val, palette->id_name);
    NBT* name = nbt_new(TAG_String, &val, 0, "Name");
    
    if(properties) properties->next = name;
    name->prev = properties;

    g_value_unset(&val);

    GValue val2 = {0};
    g_value_init(&val2, G_TYPE_POINTER);
    g_value_set_pointer(&val2, properties ? properties : name);
    NBT* ret = nbt_new(TAG_Compound, &val2, 0, NULL);
    return ret;
}

static NbtInstance* palette_nbt_instance_new(Palette* palette)
{
    NbtInstance* properties = properties_nbt_instance_new(palette->property_name, palette->property_data);
    NbtInstance* name = dh_nbt_instance_new_string(palette->id_name, "Name");
    if(dh_nbt_instance_is_non_null(properties))
        dh_nbt_instance_fill_next(properties, name);
    dh_nbt_instance_fill_prev(name, properties);

    NbtInstance* ret = dh_nbt_instance_new_compound(NULL);
    dh_nbt_instance_fill_child(ret, dh_nbt_instance_is_non_null(properties)? properties : name);
    dh_nbt_instance_free_only_instance(properties);
    dh_nbt_instance_free_only_instance(name);
    return ret;
}

static NBT* palettes_nbt_new(PaletteArray* array)
{
    NBT* head = NULL;
    NBT* prev = NULL;
    NBT* cur = NULL;
    for(int i = 0 ; i < array->len ; i++)
    {
        cur = palette_nbt_new(array->pdata[i]);
        cur->prev = prev;
        if(prev) prev->next = cur;
        if(i == 0) head = cur;
        prev = cur;
    }

    GValue val = {0};
    g_value_init(&val, G_TYPE_POINTER);
    g_value_set_pointer(&val, head);
    NBT* ret = nbt_new(TAG_List, &val, 0, "palette");
    return ret;
}

static NbtInstance* palettes_nbt_instance_new(PaletteArray* array)
{
    NbtInstance* head = dh_nbt_instance_new_from_real_nbt(NULL);
    NbtInstance* prev = dh_nbt_instance_new_from_real_nbt(NULL);
    NbtInstance* cur = NULL;
    for(int i = 0 ; i < array->len ; i++)
    {
        cur = palette_nbt_instance_new(array->pdata[i]);
        /* Set head */
        if(i == 0)
        {
            dh_nbt_instance_free(head);
            head = cur;
        } 
        dh_nbt_instance_fill_prev(cur, prev);
        if(dh_nbt_instance_is_non_null(prev))
            dh_nbt_instance_fill_next(prev, cur);
        /* Old prev is of no use */
        if(prev != head) dh_nbt_instance_free_only_instance(prev);
        prev = cur;
    }

    NbtInstance* ret = dh_nbt_instance_new_list("palette");
    dh_nbt_instance_fill_child(ret, head);
    if(prev != head) dh_nbt_instance_free_only_instance(head);
    dh_nbt_instance_free_only_instance(prev);
    return ret;
}

NBT* nbt_new_from_region(Region* region)
{
    /* First is size */
    NBT* size_nbt = size_nbt_new(region->region_size, "size");
    /* Second is entities */
    NBT* entities_nbt = entities_nbt_new();
    size_nbt->next = entities_nbt;
    entities_nbt->prev = size_nbt;
    /* Third is blocks */
    NBT* blocks_nbt = blocks_nbt_new(region->block_info_array);
    entities_nbt->next = blocks_nbt;
    blocks_nbt->prev = entities_nbt;
    /* Fourth is palette */
    NBT* palette_nbt = palettes_nbt_new(region->palette_array);
    blocks_nbt->next = palette_nbt;
    palette_nbt->prev = blocks_nbt;
    /* Fifth is DataVersion */
    NBT* data_version_nbt = data_version_nbt_new(2230);
    palette_nbt->next = data_version_nbt;
    data_version_nbt->prev = palette_nbt;

    /* Cover cover */
    GValue val = {0};
    g_value_init(&val, G_TYPE_POINTER);
    g_value_set_pointer(&val, size_nbt);
    NBT* ret = nbt_new(TAG_Compound, &val, 5, NULL);
    return ret;
}

NbtInstance* nbt_instance_new_from_region(Region* region)
{
    /* First is size */
    NbtInstance* size_nbt = size_nbt_instance_new(region->region_size, "size");
    /* Second is entities */
    NbtInstance* entities_nbt = entities_nbt_instance_new();
    dh_nbt_instance_fill_next(size_nbt, entities_nbt);
    dh_nbt_instance_fill_prev(entities_nbt, size_nbt);
    /* Third is blocks */
    NbtInstance* blocks_nbt = blocks_nbt_instance_new(region->block_info_array);
    dh_nbt_instance_fill_next(entities_nbt, blocks_nbt);
    dh_nbt_instance_fill_prev(blocks_nbt, entities_nbt);
    /* Fourth is palette */
    NbtInstance* palette_nbt = palettes_nbt_instance_new(region->palette_array);
    dh_nbt_instance_fill_next(blocks_nbt, palette_nbt);
    dh_nbt_instance_fill_prev(palette_nbt, blocks_nbt);
    /* Fifth is DataVersion */
    NbtInstance* data_version_nbt = data_version_nbt_instance_new(2230);
    dh_nbt_instance_fill_next(palette_nbt, data_version_nbt);
    dh_nbt_instance_fill_prev(data_version_nbt, palette_nbt);

    /* Cover cover */
    NbtInstance* ret = dh_nbt_instance_new_compound(NULL);
    dh_nbt_instance_fill_child(ret, entities_nbt);

    dh_nbt_instance_free_only_instance(size_nbt);
    dh_nbt_instance_free_only_instance(entities_nbt);
    dh_nbt_instance_free_only_instance(blocks_nbt);
    dh_nbt_instance_free_only_instance(palette_nbt);
    dh_nbt_instance_free_only_instance(data_version_nbt);
    return ret;
}