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
#include "../translation.h"
#include "common_info.h"
#include "dh_bit.h"
#include "dh_string_util.h"
#include "dhlrc_list.h"
#include "glib.h"
#include "glibconfig.h"
#include "litematica_region.h"
#include "nbt_interface_cpp/nbt_interface.hpp"
#include "process_state.h"

#include <iostream>
#include <ostream>

/* TODO: Region convertion progress */

static void palette_free (gpointer mem);
static void base_data_free (gpointer mem);

void
region_free (void *data)
{
    auto region = static_cast<Region *> (data);
    g_free (region->region_size);
    base_data_free (region->data);
    g_ptr_array_unref (region->palette_array);
    g_ptr_array_unref (region->block_entity_array);
    g_free (region->block_array);
    g_free (region);
}

static void
palette_free (gpointer mem)
{
    Palette *palette = (Palette *)mem;
    g_free (palette->id_name);
    dh_str_array_free (palette->property_name);
    dh_str_array_free (palette->property_data);
    g_free (mem);
}

static DhStrArray *
get_property_name_from_nbt_instance_cpp (DhNbtInstance instance)
{
    if (!instance.is_non_null ())
        return nullptr;
    else
        {
            DhStrArray *arr = nullptr;
            for (; instance.is_non_null (); instance.next ())
                dh_str_array_add_str (&arr, instance.get_key ());
            return arr;
        }
}

static DhStrArray *
get_property_data_from_nbt_instance_cpp (DhNbtInstance instance)
{
    if (!instance.is_non_null ())
        return nullptr;
    else
        {
            DhStrArray *arr = nullptr;
            for (; instance.is_non_null (); instance.next ())
                dh_str_array_add_str (&arr, instance.get_string ());
            return arr;
        }
}

static PaletteArray *
get_palette_full_info_from_lr (LiteRegion *lr)
{
    GPtrArray *array = g_ptr_array_new_with_free_func (palette_free);
    DhStrArray *blocks = lite_region_block_name_array (lr);
    for (int i = 0; i < blocks->num; i++)
        {
            Palette *palette = g_new0 (Palette, 1);
            palette->id_name = g_strdup (blocks->val[i]);

            auto instance = lite_region_get_instance (lr);
            auto instance_dup (instance);
            instance_dup.child ("BlockStatePalette");
            instance_dup.child ();
            for (int j = 0; j < i; j++)
                instance_dup.next ();
            if (instance_dup.child ("Properties"))
                instance_dup.child ();
            else
                instance_dup.make_invalid ();
            DhStrArray *name
                = get_property_name_from_nbt_instance_cpp (instance_dup);
            DhStrArray *data
                = get_property_data_from_nbt_instance_cpp (instance_dup);

            palette->property_name = name;
            palette->property_data = data;

            g_ptr_array_add (array, palette);
        }
    return array;
}

static PaletteArray *
get_palette_full_info_from_nbt_instance (Region *region,
                                         DhNbtInstance instance)
{
    GPtrArray *array = g_ptr_array_new_with_free_func (palette_free);
    instance.child ("palette");
    instance.child ();
    bool find_air = false;
    int air_palette = -1;
    for (; instance.is_non_null (); instance.next ())
        {
            auto palette_instance (instance);
            Palette *palette = g_new0 (Palette, 1);
            palette_instance.child ("Name");
            palette->id_name = g_strdup (palette_instance.get_string ());
            const char *id_name = palette_instance.get_string ();

            palette_instance.parent ();
            if (palette_instance.child ("Properties"))
                palette_instance.child ();
            else
                palette_instance.make_invalid ();
            DhStrArray *name
                = get_property_name_from_nbt_instance_cpp (palette_instance);
            DhStrArray *data
                = get_property_data_from_nbt_instance_cpp (palette_instance);

            palette->property_name = name;
            palette->property_data = data;

            g_ptr_array_add (array, palette);
            if (g_str_equal (id_name, "minecraft:air") && !find_air)
                {
                    find_air = true;
                    air_palette = array->len - 1;
                }
        }
    if (!find_air)
        {
            Palette *palette = g_new0 (Palette, 1);
            palette->id_name = g_strdup ("minecraft:air");
            palette->property_name = nullptr;
            palette->property_data = nullptr;
            g_ptr_array_add (array, palette);
            air_palette = array->len - 1;
        }
    if (air_palette != 0)
        {
            Palette *temp_non_air = (Palette *)array->pdata[0];
            Palette *air = (Palette *)array->pdata[air_palette];
            array->pdata[0] = air;
            array->pdata[air_palette] = temp_non_air;
        }
    region->air_palette = air_palette;

    return array;
}

static void
block_entity_free (gpointer mem)
{
    BlockEntity *entity = (BlockEntity *)mem;
    dh_nbt_instance_cpp_free (entity->nbt_instance);
    g_free (entity->pos);
    g_free (entity);
}

static BaseData *
base_data_new_from_lr (LiteRegion *lr)
{
    auto ret = g_new0 (BaseData, 1);
    ret->create_time
        = g_date_time_new_from_unix_utc (lite_region_create_time (lr) / 1000);
    ret->modify_time
        = g_date_time_new_from_unix_utc (lite_region_modify_time (lr) / 1000);
    ret->author = g_strdup (lite_region_author (lr));
    ret->name = g_strdup (lite_region_name (lr));
    ret->description = g_strdup (lite_region_description (lr));

    return ret;
}

gboolean
file_is_new_schem (void *instance_ptr)
{
    DhNbtInstance *instance = (DhNbtInstance *)instance_ptr;
    if (instance->get_key ()
        && g_str_equal ("Schematic", instance->get_key ()))
        return true;
    else
        return false;
}

void
new_schem_get_size (Region *region, DhNbtInstance instance)
{
    Pos *pos = g_new0 (Pos, 1);
    instance.child ("Width");
    int x = instance.get_short ();
    instance.parent ();
    instance.child ("Length");
    int z = instance.get_short ();
    instance.parent ();
    instance.child ("Height");
    int y = instance.get_short ();
    instance.parent ();
    pos->x = x;
    pos->y = y;
    pos->z = z;
    region->region_size = pos;
}

static BaseData *
base_data_new_null ()
{
    auto ret = g_new0 (BaseData, 1);
    ret->create_time = g_date_time_new_now_local ();
    ret->modify_time = g_date_time_new_now_local ();
    ret->description = g_strdup ("");
    ret->author = g_strdup (g_get_user_name ());
    ret->name = g_strdup ("Converted");

    return ret;
}

static Palette *
new_schem_get_palette_from_key (const char *key)
{
    Palette *palette = g_new0 (Palette, 1);
    if (strchr (key, '['))
        {
            char *o_key_dup = g_strdup (key);
            char *pos = strchr (o_key_dup, '['); /* Point to [ */
            char *new_pos = pos + 1;             /* Point to key after [ */
            char *elements = g_strdup (new_pos);
            *pos = 0;
            palette->id_name = g_strdup (o_key_dup);
            g_free (o_key_dup);
            /* Analyse every element */
            int len = strlen (elements);
            elements[len - 1] = 0;
            auto strarray = g_strsplit (elements, ",", -1);
            g_free (elements);
            DhStrArray *name = nullptr;
            DhStrArray *data = nullptr;
            auto strarray_p = strarray;
            for (; *strarray && **strarray; strarray++)
                {
                    auto str = *strarray;
                    char *equal_pos = strchr (str, '=');
                    char *data_str = g_strdup (equal_pos + 1);
                    *equal_pos = 0;
                    dh_str_array_add_str (&name, str);
                    dh_str_array_add_str (&data, data_str);
                    g_free (data_str);
                }
            g_strfreev (strarray_p);
            palette->property_name = name;
            palette->property_data = data;
        }
    else
        {
            /* No element need to analyse */
            palette->id_name = g_strdup (key);
            palette->property_name = nullptr;
            palette->property_data = nullptr;
        }
    return palette;
}

static void
new_schem_get_palette (Region *region, DhNbtInstance instance)
{
    /* Get length first */
    instance.child ("PaletteMax");
    int len = instance.get_int ();
    instance.parent ();
    GPtrArray *array = g_ptr_array_new_with_free_func (palette_free);
    g_ptr_array_set_size (array, len);

    instance.child ("Palette");
    if (instance.child ())
        {
            /* Then analyse everyone */
            for (; instance.is_non_null (); instance.next ())
                {
                    int pos = instance.get_int ();
                    Palette *palette
                        = new_schem_get_palette_from_key (instance.get_key ());
                    if (g_str_equal (palette->id_name, "minecraft:air"))
                        region->air_palette = pos;
                    array->pdata[pos] = palette;
                }
        }
    if (region->air_palette != 0)
        {
            Palette *temp_non_air = (Palette *)array->pdata[0];
            Palette *air = (Palette *)array->pdata[region->air_palette];
            array->pdata[0] = air;
            array->pdata[region->air_palette] = temp_non_air;
        }
    region->palette_array = array;
}

static void
new_schem_get_block_array (Region *region, const DhNbtInstance &instance)
{
    /* Get array first */
    int len = 0;
    DhNbtInstance instance_dup (instance);
    instance_dup.child ("BlockData");
    auto arr = instance_dup.get_byte_array (len);
    instance_dup.parent ();

    int move_bits = g_bit_storage (region->palette_array->len - 1);
    move_bits = move_bits <= 2 ? 2 : move_bits;
    DhBit *bits = dh_bit_new ();
    for (int i = 0; i < len; i++)
        {
            int palette = arr[i];
            if (palette == 0)
                palette = region->air_palette;
            else if (palette == region->air_palette)
                palette = 0;

            dh_bit_push_back_val (bits, move_bits, palette);
        }

    region->block_array = dh_bit_dup_array (bits, &region->block_array_len);
    dh_bit_free (bits);
}

static void
new_schem_get_block_entity_array (Region *region,
                                  const DhNbtInstance &instance_dup)
{
    region->block_entity_array
        = g_ptr_array_new_with_free_func (block_entity_free);
    DhNbtInstance instance (instance_dup);
    instance.child ("BlockEntities");
    if (instance.child ())
        {
            for (; instance (); instance.next ())
                {
                    DhNbtInstance entity
                        = instance.dup_current_as_original (false);
                    entity.child ("Pos");
                    int pos_len = 0;
                    auto pos = entity.get_int_array (pos_len);
                    entity.parent ();

                    BlockEntity *entity_block = g_new0 (BlockEntity, 1);
                    entity_block->pos = g_new0 (Pos, 1);
                    entity_block->pos->x = pos[0];
                    entity_block->pos->y = pos[1];
                    entity_block->pos->z = pos[2];

                    entity.set_key ("nbt");
                    DhNbtInstance id (entity);
                    id.child ("Id");
                    id.set_key ("id");
                    entity.rm_node ("Pos");
                    entity_block->nbt_instance = new DhNbtInstance (entity);
                    g_ptr_array_add (region->block_entity_array, entity_block);
                }
        }
}

Region *
region_new_from_new_schem (void *instance_ptr)
{
    if (!file_is_new_schem (instance_ptr))
        return nullptr;
    else
        {
            Region *region = g_new0 (Region, 1);
            DhNbtInstance instance = *(DhNbtInstance *)instance_ptr;
            DhNbtInstance instance_dup (instance);
            /* Fill DataVersion */
            instance_dup.child ("DataVersion");
            region->data_version = instance_dup.get_int ();
            instance_dup.parent ();
            /* Fill BaseData */
            auto base_data = base_data_new_null ();
            region->data = base_data;
            /* Fill Pos */
            new_schem_get_size (region, instance_dup);
            /* Fill Palette */
            new_schem_get_palette (region, instance_dup);
            /* Fill BlockArray */
            new_schem_get_block_array (region, instance_dup);
            /* Fill BlockEntityArray */
            new_schem_get_block_entity_array (region, instance_dup);
            return region;
        }
}

static BlockEntityArray *
get_block_entities_from_lr (LiteRegion *lr, Region *region)
{
    auto ret = g_ptr_array_new_with_free_func (block_entity_free);
    auto region_instance (lite_region_get_instance (lr));
    region_instance.child ("TileEntities");
    region_instance.child ();
    for (; region_instance (); region_instance.next ())
        {
            BlockEntity *entity = g_new (BlockEntity, 1);

            region_instance.child ("x");
            gint64 x = region_instance.get_int ();
            region_instance.parent ();

            region_instance.child ("y");
            gint64 y = region_instance.get_int ();
            region_instance.parent ();

            region_instance.child ("z");
            gint64 z = region_instance.get_int ();
            region_instance.parent ();

            entity->pos = g_new (Pos, 1);
            entity->pos->x = x;
            entity->pos->y = y;
            entity->pos->z = z;

            auto tile_entities
                = region_instance.dup_current_as_original (false);
            tile_entities.rm_node ("x");
            tile_entities.rm_node ("y");
            tile_entities.rm_node ("z");
            tile_entities.set_key ("nbt");

            gint64 index = lite_region_block_index (lr, x, y, z);

            /* It seems that some version lacks id, we temporarily add one. */
            DhNbtInstance decide_has_id (tile_entities);
            if (!decide_has_id.child ("id"))
                {
                    auto palette_num
                        = region_get_block_palette (region, index);
                    auto palette = region_get_palette (region, palette_num);
                    auto id_name = palette->id_name;
                    DhNbtInstance id (id_name, "id", true);
                    tile_entities.insert_before ({}, id);
                }

            entity->nbt_instance = new DhNbtInstance (tile_entities);
            g_ptr_array_add (ret, entity);
        }
    return ret;
}

Region *
region_new_from_lite_region (LiteRegion *lr)
{
    Region *region = g_new0 (Region, 1);

    /* Fill DataVersion */
    region->data_version = lite_region_data_version (lr);
    region->data = base_data_new_from_lr (lr);

    /* Fill RegionSize */
    RegionSize *rs = g_new0 (RegionSize, 1);
    rs->x = lite_region_size_x (lr);
    rs->y = lite_region_size_y (lr);
    rs->z = lite_region_size_z (lr);
    region->region_size = rs;

    /* Fill PaletteArray */
    PaletteArray *pa = get_palette_full_info_from_lr (lr);
    region->palette_array = pa;

    /* Fill BlockArray */
    const gint64 *states = lite_region_state (lr, &region->block_array_len);
    region->block_array = g_new0 (gint64, region->block_array_len);
    memcpy (region->block_array, states,
            sizeof (gint64) * region->block_array_len);

    /* Fill BlockEntityArray */
    BlockEntityArray *bea = get_block_entities_from_lr (lr, region);
    region->block_entity_array = bea;

    return region;
}

static void
base_data_free (gpointer mem)
{
    BaseData *data = (BaseData *)mem;
    if (data)
        {
            g_date_time_unref (data->create_time);
            g_date_time_unref (data->modify_time);
            g_free (data->author);
            g_free (data->description);
            g_free (data->name);
            g_free (data);
        }
}

static gint64 *
get_block_array_from_nbt_instance (const DhNbtInstance &instance,
                                   Region *region, int *len)
{
    DhNbtInstance nbt_instance (instance);
    nbt_instance.child ("blocks");
    nbt_instance.child ();
    int move_bits = g_bit_storage (region->palette_array->len - 1);
    move_bits = move_bits <= 2 ? 2 : move_bits;
    int region_size = region->region_size->x * region->region_size->y
                      * region->region_size->z;
    DhBit *bits = dh_bit_new ();
    for (int i = 0; i < region_size; i++)
        {
            DhNbtInstance block (nbt_instance);
            bool added = false;
            for (; block (); block.next ())
                {
                    auto pos_instance (block);
                    pos_instance.child ("pos");
                    pos_instance.child ();
                    int x = pos_instance.get_int ();
                    pos_instance.next ();
                    int y = pos_instance.get_int ();
                    pos_instance.next ();
                    int z = pos_instance.get_int ();
                    int index = region_get_index (region, x, y, z);
                    if (i == index)
                        {
                            added = true;
                            auto palette (block);
                            palette.child ("state");
                            int palette_num = palette.get_int ();
                            /* Switch the air palette with the air-palette */
                            if (palette_num == region->air_palette)
                                palette_num = 0;
                            else if (palette_num == 0)
                                palette_num = region->air_palette;
                            dh_bit_push_back_val (bits, move_bits,
                                                  palette_num);
                            break;
                        }
                }
            if (!added)
                {
                    dh_bit_push_back_val (bits, move_bits, 0);
                }
        }
    gint64 *ret = dh_bit_dup_array (bits, len);
    dh_bit_free (bits);
    return ret;
}

static BlockEntityArray *
get_block_entity_array_from_nbt_instance (const DhNbtInstance &instance,
                                          Region *region)
{
    auto ret = g_ptr_array_new_with_free_func (block_entity_free);
    DhNbtInstance nbt_instance (instance);
    nbt_instance.child ("blocks");
    nbt_instance.child ();
    for (; nbt_instance (); nbt_instance.next ())
        {
            auto pos_instance (nbt_instance);
            pos_instance.child ("pos");
            pos_instance.child ();
            int x = pos_instance.get_int ();
            pos_instance.next ();
            int y = pos_instance.get_int ();
            pos_instance.next ();
            int z = pos_instance.get_int ();

            auto nbt (nbt_instance);
            if (nbt.child ("nbt"))
                {
                    BlockEntity *be = g_new0 (BlockEntity, 1);
                    DhNbtInstance ni = nbt.dup_current_as_original (false);
                    be->nbt_instance = new DhNbtInstance (ni);
                    be->pos = g_new0 (Pos, 1);
                    be->pos->x = x;
                    be->pos->y = y;
                    be->pos->z = z;
                    g_ptr_array_add (ret, be);
                }
        }
    return ret;
}

static Region *
region_new_from_nbt_instance (const DhNbtInstance &instance)
{
    Region *region = g_new0 (Region, 1);

    /* Fill RegionSize */
    RegionSize *rs = g_new0 (RegionSize, 1);
    auto size_instance (instance);
    if (size_instance.child ("size"))
        {
            size_instance.child ();
            rs->x = ABS (size_instance.get_int ());
            size_instance.next ();
            rs->y = ABS (size_instance.get_int ());
            size_instance.next ();
            rs->z = ABS (size_instance.get_int ());
            region->region_size = rs;

            /* Fill PaletteArray */
            PaletteArray *pa
                = get_palette_full_info_from_nbt_instance (region, instance);
            region->palette_array = pa;

            /* Fill BlockArray */
            gint64 *ba = get_block_array_from_nbt_instance (
                instance, region, &region->block_array_len);
            region->block_array = ba;

            /* Fill BlockEntityArray */
            BlockEntityArray *bl
                = get_block_entity_array_from_nbt_instance (instance, region);
            region->block_entity_array = bl;

            /* Fill Data Version */
            auto version_instance (instance);
            version_instance.child ("DataVersion");
            region->data_version = version_instance.get_int ();
            region->data = base_data_new_null ();

            return region;
        }
    else
        {
            g_free (region);
            g_free (rs);
            return NULL;
        }
}

Region *
region_new_from_nbt_file (const char *filepos)
{
    DhNbtInstance instance (filepos, true);
    Region *ret = region_new_from_nbt_instance (instance);
    instance.self_free ();
    return ret;
}

Region *
region_new_from_nbt_instance_ptr (void *instance_ptr)
{
    auto instance = *(DhNbtInstance *)instance_ptr;
    if (file_is_new_schem (instance_ptr))
        return region_new_from_new_schem (instance_ptr);
    else
        return region_new_from_nbt_instance (instance);
}

ItemList *
item_list_new_from_multi_region (const char **region_uuid_arr)
{
    ItemList *ret = NULL;
    for (; *region_uuid_arr; region_uuid_arr++)
        {
            Region *region = (Region *)dh_info_get_data (DH_TYPE_REGION,
                                                         *region_uuid_arr);
            ItemList *i_il = item_list_new_from_region (region);
            item_list_combine (&ret, i_il);
            item_list_free (i_il);
        }
    return ret;
}

gboolean
palette_is_same (gconstpointer a_data_p, gconstpointer b_data_p)
{
    auto a = static_cast<const Palette *> (a_data_p);
    auto b = static_cast<const Palette *> (b_data_p);

    DhStrArray *a_name = a->property_name;
    DhStrArray *b_name = b->property_name;

    DhStrArray *a_data = a->property_data;
    DhStrArray *b_data = b->property_data;

    if (g_str_equal (a->id_name, b->id_name))
        {
            if (!a_name && !b_name) /* No property */
                return TRUE;
            if (!a_name || !b_name) /* ?? */
                return FALSE;
            if (a_name->num != b_name->num) /* ?? */
                return FALSE;
            DhStrArray *a_palette
                = dh_str_array_cat_str_array (a_name, a_data, "=");
            DhStrArray *b_palette
                = dh_str_array_cat_str_array (b_name, b_data, "=");
            gboolean ret = FALSE;
            if (dh_str_array_equal (a_palette, b_palette, TRUE))
                ret = TRUE;
            dh_str_array_free (a_palette);
            dh_str_array_free (b_palette);
            return ret;
        }
    return FALSE;
}

Palette *
region_get_palette (Region *region, int val)
{
    return static_cast<Palette *> (region->palette_array->pdata[val]);
}

char *
region_get_id_name (Region *region, int index)
{
    auto palette_num = region_get_block_palette (region, index);
    auto palette = region_get_palette (region, palette_num);
    return palette->id_name;
}

// void
// region_modify_property (Region *region, BlockInfo *info, gboolean
// all_modify,
//                         DhStrArray *new_data)
// {
//     auto new_palette = g_new0 (Palette, 1);
//     auto old_palette = region_get_palette (region, info->palette);
//     new_palette->id_name = g_strdup (old_palette->id_name);
//     new_palette->property_name = dh_str_array_dup
//     (old_palette->property_name); new_palette->property_data =
//     dh_str_array_dup (new_data);
//
//     if (all_modify)
//         {
//             /* First replace the old palette */
//             palette_free (old_palette);
//             region->palette_array->pdata[info->palette] = new_palette;
//             /* Try to get the same palette */
//             for (int i = 0; i < region->palette_array->len; i++)
//                 {
//                     auto current_palette = region_get_palette (region, i);
//                     if (i != info->palette
//                         && palette_is_same (new_palette, current_palette))
//                         {
//                             g_ptr_array_remove_index (region->palette_array,
//                                                       i);
//                             palette_minus_one (region, i, info->palette);
//                             return;
//                         }
//                 }
//             /* If not same palette found, do nothing. */
//         }
//     else
//         {
//             for (int i = 0; i < region->palette_array->len; i++)
//                 {
//                     auto current_palette = region_get_palette (region, i);
//                     if (i != info->palette
//                         && palette_is_same (new_palette, current_palette))
//                         {
//                             info->palette = i;
//                             palette_free (new_palette);
//                             return;
//                         }
//                 }
//             g_ptr_array_add (region->palette_array, new_palette);
//             info->palette = region->palette_array->len - 1;
//         }
// }

int
region_get_index (Region *region, int x, int y, int z)
{
    if (x >= region->region_size->x && y >= region->region_size->y
        && z >= region->region_size->z)
        return -1;
    return region->region_size->x * region->region_size->z * y
           + region->region_size->x * z + x;
}

gboolean
region_add_palette (Region *region, const char *id_name,
                    DhStrArray *palette_name, DhStrArray *palette_data)
{
    Palette tmp_palette{ (char *)id_name, palette_name, palette_data };
    if (g_ptr_array_find_with_equal_func (region->palette_array, &tmp_palette,
                                          palette_is_same, nullptr))
        return FALSE;
    auto palette = g_new0 (Palette, 1);
    palette->id_name = g_strdup (palette->id_name);
    palette->property_name = dh_str_array_dup (palette->property_name);
    palette->property_data = dh_str_array_dup (palette->property_data);
    g_ptr_array_add (region->palette_array, palette);
    return TRUE;
}

gboolean
region_add_palette_using_palette (Region *region, Palette *palette)
{
    if (g_ptr_array_find_with_equal_func (region->palette_array, palette,
                                          palette_is_same, nullptr))
        return FALSE;
    g_ptr_array_add (region->palette_array, palette);
    return TRUE;
}

int
region_get_block_palette (Region *region, int index)
{
    const int64_t *state = region->block_array;
    int bits = g_bit_storage (region->palette_array->len - 1);
    bits = bits <= 2 ? 2 : bits;
    uint64_t start_bit = static_cast<uint64_t> (index) * bits;
    uint64_t start_state = start_bit / 64;
    uint64_t and_num = (1 << bits) - 1;
    uint64_t move_num = start_bit & 63;
    uint64_t end_num = start_bit % 64 + bits;
    int id = 0;
    if (end_num <= 64)
        id = (uint64_t)(state[start_state]) >> move_num & and_num;
    else
        {
            int move_num_2 = 64 - move_num;
            if (start_state + 1 >= region->block_array_len)
                g_error ("Out of range!");
            id = ((uint64_t)state[start_state] >> move_num
                  | (uint64_t)state[start_state + 1] << move_num_2)
                 & and_num;
        }
    return id;
}

BlockEntity *
region_get_block_entity (Region *region, int x, int y, int z)
{
    for (int i = 0; i < region->block_entity_array->len; i++)
        {
            BlockEntity *be
                = (BlockEntity *)region->block_entity_array->pdata[i];
            if (be->pos->x == x && be->pos->y == y && be->pos->z == z)
                return be;
        }
    return NULL;
}