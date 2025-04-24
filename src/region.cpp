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
#include "dh_bit.h"
#include "dh_string_util.h"
#include "dhlrc_list.h"
#include "glib.h"
#include "glibconfig.h"
#include "litematica_region.h"
#include "nbt_interface_cpp/nbt_interface.hpp"
#include <time.h>
#include "process_state.h"
#include "../translation.h"

/* TODO and maybe never do, since property can be too much */
const char* property[] = {N_("waterlogged"),
                          N_("facing")};
const char* data[] = {N_("true"), 
                      N_("false"),
                      N_("south"),
                      N_("east"),
                      N_("north"),
                      N_("west")};

static void palette_free(gpointer mem);
static void block_info_free(gpointer mem);
static void base_data_free(gpointer mem);

void region_free(Region* region)
{
    g_free(region->region_size);
    base_data_free(region->data);
    g_ptr_array_unref(region->block_info_array);
    g_ptr_array_unref(region->palette_array);
    g_free(region);
}

static void palette_free(gpointer mem)
{
    Palette* palette = (Palette*)mem;
    g_free(palette->id_name);
    dh_str_array_free(palette->property_name);
    dh_str_array_free(palette->property_data);
    g_free(mem);
}

static DhStrArray* get_property_name_from_nbt_instance_cpp(DhNbtInstance instance)
{
    if(!instance.is_non_null())
        return nullptr;
    else
    {
        DhStrArray* arr = nullptr;
        for(; instance.is_non_null() ; instance.next())
            dh_str_array_add_str(&arr, instance.get_key());
        return arr;
    }
}

static DhStrArray* get_property_data_from_nbt_instance_cpp(DhNbtInstance instance)
{
    if(!instance.is_non_null())
        return nullptr;
    else
    {
        DhStrArray* arr = nullptr;
        for(; instance.is_non_null() ; instance.next())
            dh_str_array_add_str(&arr, instance.get_string());
        return arr;
    }
}

static PaletteArray* get_palette_full_info_from_lr(LiteRegion* lr)
{
    GPtrArray* array = g_ptr_array_new_with_free_func(palette_free);
    DhStrArray* blocks = lite_region_block_name_array(lr);
    for(int i = 0 ; i < blocks->num; i++)
    {
        Palette* palette = g_new0(Palette, 1);
        palette->id_name = g_strdup(blocks->val[i]);

        auto instance = lite_region_get_instance(lr);
        auto instance_dup(instance);
        instance_dup.child("BlockStatePalette");
        instance_dup.child();
        for(int j = 0 ; j < i ; j++)
            instance_dup.next();
        if(instance_dup.child("Properties"))
            instance_dup.child();
        else instance_dup.make_invalid();
        DhStrArray* name = get_property_name_from_nbt_instance_cpp(instance_dup);
        DhStrArray* data = get_property_data_from_nbt_instance_cpp(instance_dup);

        palette->property_name = name;
        palette->property_data = data;

        g_ptr_array_add(array, palette);
    }
    return array;
}



static PaletteArray* get_palette_full_info_from_nbt_instance(DhNbtInstance instance)
{
    GPtrArray* array = g_ptr_array_new_with_free_func(palette_free);
    instance.child("palette");
    instance.child();
    for(; instance.is_non_null() ; instance.next())
    {
        auto palette_instance(instance);
        Palette* palette = g_new0(Palette, 1);
        palette_instance.child("Name");
        palette->id_name = g_strdup(palette_instance.get_string());

        palette_instance.parent();
        if(palette_instance.child("Properties"))
            palette_instance.child();
        else palette_instance.make_invalid();
        DhStrArray* name = get_property_name_from_nbt_instance_cpp(palette_instance);
        DhStrArray* data = get_property_data_from_nbt_instance_cpp(palette_instance);

        palette->property_name = name;
        palette->property_data = data;

        g_ptr_array_add(array, palette);
    }
    return array;
}

static void block_info_free(gpointer mem)
{
    BlockInfo* info = (BlockInfo*)mem;
    g_free(info->pos);
    g_free(info->id_name);
    dh_nbt_instance_cpp_free(info->nbt_instance);
    g_free(mem);
}

static BlockInfoArray* get_block_full_info_from_lr(LiteRegion* lr)
{
    GPtrArray* array = g_ptr_array_new_with_free_func(block_info_free);
    for(int y = 0 ; y < lite_region_size_y(lr) ; y++)
    {
        for(int z = 0 ; z < lite_region_size_z(lr) ; z++)
        {
            for(int x = 0; x < lite_region_size_x(lr) ; x++)
            {
                BlockInfo* block = g_new0(BlockInfo, 1);
                block->pos = g_new0(Pos, 1);
                block->index = lite_region_block_index(lr, x, y, z);
                block->palette = lite_region_block_id(lr, block->index);
                block->pos->x = x;
                block->pos->y = y;
                block->pos->z = z;
                block->nbt_instance = nullptr;
                DhStrArray* blocks = lite_region_block_name_array(lr);
                if(block->palette < blocks->num)
                    block->id_name = g_strdup(blocks->val[block->palette]);
                else
                {
                    g_critical("Palette out of range! will fall back to air!");
                    block->id_name = g_strdup("minecraft:air");
                }
                g_ptr_array_add(array, block);
            }
        }
    }

    auto region_instance(lite_region_get_instance(lr));
    region_instance.child("TileEntities");
    region_instance.child();
    for(; region_instance.is_non_null() ; region_instance.next())
    {
        region_instance.child("x");
        gint64 x = region_instance.get_int();
        region_instance.parent();

        region_instance.child("y");
        gint64 y = region_instance.get_int();
        region_instance.parent();

        region_instance.child("z");
        gint64 z = region_instance.get_int();
        region_instance.parent();

        auto tile_entities= region_instance.dup_current_as_original(false);
        tile_entities.rm_node("x");
        tile_entities.rm_node("y");
        tile_entities.rm_node("z");
        tile_entities.set_key("nbt");
        // NBT* new_tile_entities = nbt_dup(region_instance.get_current_nbt());
        // new_tile_entities = nbt_rm(new_tile_entities, "x");
        // new_tile_entities = nbt_rm(new_tile_entities, "y");
        // new_tile_entities = nbt_rm(new_tile_entities, "z");
        gint64 index = lite_region_block_index(lr, x, y, z);
        // new_tile_entities->key = dh_strdup("nbt");

        ((BlockInfo*)array->pdata[index])->nbt_instance = new DhNbtInstance(tile_entities);
    }
    return array;
}

static BlockInfoArray* get_block_full_info_from_nbt_instance(DhNbtInstance instance, PaletteArray* pa, Pos* pos)
{
    GPtrArray* array = g_ptr_array_new_with_free_func(block_info_free);
    DhNbtInstance blocks(instance);
    blocks.child("blocks");
    blocks.child();
    for( ; blocks.is_non_null() ; blocks.next())
    {
        BlockInfo* block_info = g_new0(BlockInfo, 1);
        block_info->pos = g_new0(Pos, 1);
        // lr->region_size.x * lr->region_size.z * y + lr->region_size.x * z + x
        
        
        auto palette(blocks);
        palette.child("state");
        block_info->palette = palette.get_int();

        auto pos_instance(blocks);
        pos_instance.child("pos");
        pos_instance.child();
        int x = pos_instance.get_int();
        block_info->pos->x = x;
        pos_instance.next();
        int y = pos_instance.get_int();
        block_info->pos->y = y;
        pos_instance.next();
        int z = pos_instance.get_int();
        block_info->pos->z = z;

        block_info->index = pos->x * pos->z * y + pos->x * z + x /* TODO */;
        
        auto nbt(blocks);
        if(nbt.child("nbt"))
        {
            DhNbtInstance ni = nbt.dup_current_as_original(false);
            block_info->nbt_instance = new DhNbtInstance(ni);
        }
        else block_info->nbt_instance = nullptr;

        auto block_palette = (Palette*)(pa->pdata[block_info->palette]);
        block_info->id_name = g_strdup(block_palette->id_name);
        g_ptr_array_add(array, block_info);
    }
    return array;
}

static BaseData* base_data_new_from_lr(LiteRegion* lr)
{
    auto ret = g_new0(BaseData, 1);
    ret->create_time = g_date_time_new_from_unix_utc(lite_region_create_time(lr) / 1000);
    ret->modify_time = g_date_time_new_from_unix_utc(lite_region_modify_time(lr) / 1000);
    ret->author = g_strdup(lite_region_author(lr));
    ret->name = g_strdup(lite_region_name(lr));
    ret->description = g_strdup(lite_region_description(lr));
    
    return ret;
}

Region* region_new_from_lite_region(LiteRegion *lr)
{
    Region* region = g_new0(Region, 1);

    /* Fill DataVersion */
    region->data_version = lite_region_data_version(lr); 
    region->data = base_data_new_from_lr(lr);

    /* Fill RegionSize */
    RegionSize* rs = g_new0(RegionSize, 1);
    rs->x = lite_region_size_x(lr);
    rs->y = lite_region_size_y(lr);
    rs->z = lite_region_size_z(lr);
    region->region_size = rs;

    /* Fill PaletteArray */
    PaletteArray* pa = get_palette_full_info_from_lr(lr);
    region->palette_array = pa;

    /* Fill BlockInfoArray */
    BlockInfoArray* bia = get_block_full_info_from_lr(lr);
    region->block_info_array = bia;

    return region;
}

static BaseData* base_data_new_null()
{
    auto ret = g_new0(BaseData, 1);
    ret->create_time = g_date_time_new_now_local();
    ret->modify_time = g_date_time_new_now_local();
    ret->description = g_strdup("");
    ret->author = g_strdup(g_get_user_name());
    ret->name = g_strdup("Converted");

    return ret;
}

static void base_data_free(gpointer mem)
{
    BaseData* data = (BaseData*)mem;
    if(data)
    {
        g_date_time_unref(data->create_time);
        g_date_time_unref(data->modify_time);
        g_free(data->author);
        g_free(data->description);
        g_free(data->name);
        g_free(data);
    }
}

static Region* region_new_from_nbt_instance(DhNbtInstance instance)
{
    Region* region = g_new0(Region, 1);

    /* Fill RegionSize */
    RegionSize* rs = g_new0(RegionSize, 1);
    auto size_instance(instance);
    size_instance.child("size");
    if(size_instance.is_non_null())
    {
        size_instance.child();
        rs->x = ABS(size_instance.get_int());
        size_instance.next();
        rs->y = ABS(size_instance.get_int());
        size_instance.next();
        rs->z = ABS(size_instance.get_int());
        region->region_size = rs;

        /* Fill PaletteArray */
        PaletteArray* pa = get_palette_full_info_from_nbt_instance(instance);
        region->palette_array = pa;

        /* Fill BlockInfoArray */
        BlockInfoArray* bia = get_block_full_info_from_nbt_instance(instance, pa, rs);
        region->block_info_array = bia;

        /* Fill Data Version */
        auto version_instance(instance);
        version_instance.child("DataVersion");
        region->data_version = version_instance.get_int();
        region->data = base_data_new_null();

        return region;
    }
    else 
    {
        g_free(region);
        g_free(rs);
        return NULL;
    }
}

Region* region_new_from_nbt_file(const char* filepos)
{
    DhNbtInstance instance(filepos);

    return region_new_from_nbt_instance(instance);
}

Region* region_new_from_nbt_instance_ptr(void* instance_ptr)
{
    auto instance = *(DhNbtInstance*)instance_ptr;
    return region_new_from_nbt_instance(instance);
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

static DhNbtInstance size_nbt_instance_new(Pos* pos, const char* key)
{ 
    DhNbtInstance ret(DH_TYPE_List, key, true);

    DhNbtInstance x(pos->x, nullptr, true);
    DhNbtInstance y(pos->y, nullptr, true);
    DhNbtInstance z(pos->z, nullptr, true);
    ret.insert_before({}, x);
    ret.insert_before({}, y);
    ret.insert_before({}, z);

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

static NBT* data_version_nbt_new(gint64 version)
{
    GValue val = {0};
    g_value_init(&val, G_TYPE_INT64);
    g_value_set_int64(&val, version);
    NBT* ret = nbt_new(TAG_Int, &val, 0, "DataVersion");
    return ret;
}

static NBT* block_nbt_new(BlockInfo* info)
{
    // NBT* nbt = nbt_dup(info->nbt);
    NBT* pos = size_nbt_new(info->pos, "pos");
    // pos->prev = nbt;
    // if(nbt)  nbt->next = pos;
    GValue val = {0};
    g_value_init(&val, G_TYPE_INT64);
    g_value_set_int64(&val, info->palette);
    NBT* state = nbt_new(TAG_Int, &val, 0, "state");
    pos->next = state;
    pos->next->prev = pos;
    g_value_unset(&val);

    g_value_init(&val, G_TYPE_POINTER);
    // g_value_set_pointer(&val, nbt? nbt : pos);
    NBT* ret = nbt_new(TAG_Compound, &val, 2, NULL);
    return ret;
}

static DhNbtInstance block_nbt_instance_new(BlockInfo* info)
{
    DhNbtInstance ret(DH_TYPE_Compound, nullptr, true);
    DhNbtInstance nbt{};
    if(info->nbt_instance)
        nbt = ((DhNbtInstance*)(info->nbt_instance))->dup_current_as_original(true);
    DhNbtInstance pos = size_nbt_instance_new(info->pos, "pos");

    ret.prepend(nbt.is_non_null()? nbt : pos);

    if(nbt.is_non_null())
        ret.insert_before({}, pos);
    DhNbtInstance state(info->palette, "state", true);
    ret.insert_before({}, state);

    return ret;
}

static NBT* blocks_nbt_new(BlockInfoArray* array)
{
    NBT* head = NULL;
    NBT* prev = NULL;
    NBT* cur = NULL;
    for(int i = 0 ; i < array->len ; i++)
    {
        cur = block_nbt_new((BlockInfo*)(array->pdata[i]));
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

static DhNbtInstance blocks_nbt_instance_new(BlockInfoArray* array)
{
    DhNbtInstance ret(DH_TYPE_List, "blocks", true);
    for(int i = 0 ; i < array->len ; i++)
    {
        DhNbtInstance cur = block_nbt_instance_new((BlockInfo*)(array->pdata[i])); 
        /* before is too slow */
        ret.insert_after({}, cur);
    }
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

static DhNbtInstance properties_nbt_instance_new(DhStrArray* name, DhStrArray* data)
{
    if(!name)
        return {};
    DhNbtInstance ret(DH_TYPE_Compound, "Properties", true);
    for(int i = 0 ; name && i < name->num ; i++)
    {
        DhNbtInstance cur(data->val[i], name->val[i], true);
        ret.insert_before({}, cur);
    }
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

static DhNbtInstance palette_nbt_instance_new(Palette* palette)
{
    DhNbtInstance ret(DH_TYPE_Compound, nullptr, true);
    DhNbtInstance properties = properties_nbt_instance_new(palette->property_name, palette->property_data);
    DhNbtInstance name(palette->id_name, "Name", true);
    if(properties.is_non_null())
        ret.insert_before({}, properties);
    ret.insert_before({}, name);

    return ret;
}

static NBT* palettes_nbt_new(PaletteArray* array)
{
    NBT* head = NULL;
    NBT* prev = NULL;
    NBT* cur = NULL;
    for(int i = 0 ; i < array->len ; i++)
    {
        cur = palette_nbt_new((Palette*)(array->pdata[i]));
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

static DhNbtInstance palettes_nbt_instance_new(PaletteArray* array)
{
    DhNbtInstance ret(DH_TYPE_List, "palette", true);
    for(int i = 0 ; i < array->len ; i++)
    {
        DhNbtInstance cur = palette_nbt_instance_new((Palette*)(array->pdata[i]));
        ret.insert_before({}, cur);
    }
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
    NBT* data_version_nbt = data_version_nbt_new(region->data_version);
    palette_nbt->next = data_version_nbt;
    data_version_nbt->prev = palette_nbt;

    /* Cover cover */
    GValue val = {0};
    g_value_init(&val, G_TYPE_POINTER);
    g_value_set_pointer(&val, size_nbt);
    NBT* ret = nbt_new(TAG_Compound, &val, 5, NULL);
    return ret;
}

static DhNbtInstance entities_nbt_instance_new()
{
    return DhNbtInstance(DH_TYPE_List, "entities", true);
}

void* nbt_instance_ptr_new_from_region(Region* region, gboolean temp_root)
{
    DhNbtInstance ret(DH_TYPE_Compound, NULL, temp_root);
    /* First is size */
    DhNbtInstance size_nbt = size_nbt_instance_new(region->region_size, "size");
    ret.insert_before({}, size_nbt);
    /* Second is entities */
    DhNbtInstance entities_nbt = entities_nbt_instance_new();
    ret.insert_before({}, entities_nbt);
    /* Third is blocks */
    DhNbtInstance blocks_nbt = blocks_nbt_instance_new(region->block_info_array);
    ret.insert_before({}, blocks_nbt);
    /* Fourth is palette */
    DhNbtInstance palette_nbt = palettes_nbt_instance_new(region->palette_array);
    ret.insert_before({}, palette_nbt);
    /* Fifth is DataVersion */
    DhNbtInstance data_version_nbt(region->data_version, "DataVersion", true);
    ret.insert_before({}, data_version_nbt);
    return new DhNbtInstance(ret);
}

gint64* region_get_palette_num_from_region(Region* region, int* length)
{
    PaletteArray* arr = region->palette_array;
    int num = arr->len;
    BlockInfoArray* info_array = region->block_info_array;
    int block_num = info_array->len;
    int move_bits = g_bit_storage(num) <= 2 ? 2 : g_bit_storage(num);

    DhBit* bit = dh_bit_new();
    gint64* ret = NULL;

    for(int id = 0 ; id < block_num ; id++)
    {
        int palette = 0;
        for(int i = 0 ; i < block_num ; i++)
        {
            BlockInfo* info = (BlockInfo*)(info_array->pdata[i]);
            if(info->index == id)
            {
                palette = info->palette;
                break;
            }
        }
        dh_bit_push_back_val(bit, move_bits, palette);
    }

    ret = dh_bit_dup_array(bit, length);
    dh_bit_free(bit);
    return ret;
}