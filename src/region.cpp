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
#include <time.h>

/* TODO and maybe never do, since property can be too much */
const char *property[] = { N_ ("waterlogged"), N_ ("facing") };
const char *data[] = { N_ ("true"), N_ ("false"), N_ ("south"),
                       N_ ("east"), N_ ("north"), N_ ("west") };

static void palette_free (gpointer mem);
static void block_info_free (gpointer mem);
static void base_data_free (gpointer mem);
static gboolean find_palette (gconstpointer a, gconstpointer b);
static int compare_block_info (gconstpointer a, gconstpointer b);

void
region_free (void *data)
{
    auto region = static_cast<Region *> (data);
    g_free (region->region_size);
    base_data_free (region->data);
    g_ptr_array_unref (region->block_info_array);
    g_ptr_array_unref (region->palette_array);
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
    if (find_air && air_palette != 0)
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
block_info_free (gpointer mem)
{
    BlockInfo *info = (BlockInfo *)mem;
    g_free (info->pos);
    dh_nbt_instance_cpp_free (info->nbt_instance);
    g_free (mem);
}

static BlockInfoArray *
get_block_full_info_from_lr (LiteRegion *lr, Region *region)
{
    GPtrArray *array = g_ptr_array_new_with_free_func (block_info_free);
    for (int y = 0; y < lite_region_size_y (lr); y++)
        {
            for (int z = 0; z < lite_region_size_z (lr); z++)
                {
                    for (int x = 0; x < lite_region_size_x (lr); x++)
                        {
                            BlockInfo *block = g_new0 (BlockInfo, 1);
                            block->pos = g_new0 (Pos, 1);
                            block->index
                                = lite_region_block_index (lr, x, y, z);
                            block->palette
                                = lite_region_block_id (lr, block->index);
                            block->pos->x = x;
                            block->pos->y = y;
                            block->pos->z = z;
                            block->nbt_instance = nullptr;
                            DhStrArray *blocks
                                = lite_region_block_name_array (lr);
                            if (block->palette < blocks->num)
                                {
                                }
                            else
                                g_critical ("Palette out of range!");
                            g_ptr_array_add (array, block);
                        }
                }
        }

    auto region_instance (lite_region_get_instance (lr));
    region_instance.child ("TileEntities");
    region_instance.child ();
    for (; region_instance.is_non_null (); region_instance.next ())
        {
            region_instance.child ("x");
            gint64 x = region_instance.get_int ();
            region_instance.parent ();

            region_instance.child ("y");
            gint64 y = region_instance.get_int ();
            region_instance.parent ();

            region_instance.child ("z");
            gint64 z = region_instance.get_int ();
            region_instance.parent ();

            auto tile_entities
                = region_instance.dup_current_as_original (false);
            tile_entities.rm_node ("x");
            tile_entities.rm_node ("y");
            tile_entities.rm_node ("z");
            tile_entities.set_key ("nbt");

            gint64 index = lite_region_block_index (lr, x, y, z);
            BlockInfo *info = (BlockInfo *)array->pdata[index];

            /* It seems that some version lacks id, we temporarily add one. */
            DhNbtInstance decide_has_id (tile_entities);
            if (!decide_has_id.child ("id"))
                {
                    auto id_name = block_info_get_id_name (region, info);
                    DhNbtInstance id (id_name, "id", true);
                    tile_entities.insert_before ({}, id);
                }

            info->nbt_instance = new DhNbtInstance (tile_entities);
        }
    return array;
}

static int
compare_block_info (gconstpointer a, gconstpointer b)
{
    BlockInfo *info_a = (BlockInfo *)a;
    BlockInfo *info_b = (BlockInfo *)b;
    return info_a->index - info_b->index;
}

static BlockInfoArray *
get_block_full_info_from_nbt_instance (const DhNbtInstance &instance,
                                       PaletteArray *pa, Pos *pos,
                                       Region *region)
{
    GPtrArray *array = g_ptr_array_new_with_free_func (block_info_free);
    DhNbtInstance blocks (instance);
    blocks.child ("blocks");
    blocks.child ();
    for (; blocks.is_non_null (); blocks.next ())
        {
            BlockInfo *block_info = g_new0 (BlockInfo, 1);
            block_info->pos = g_new0 (Pos, 1);
            // lr->region_size.x * lr->region_size.z * y + lr->region_size.x *
            // z + x

            auto palette (blocks);
            palette.child ("state");
            block_info->palette = palette.get_int ();
            if (block_info->palette == region->air_palette)
                block_info->palette = 0;
            else if (block_info->palette == 0)
                block_info->palette = region->air_palette;

            auto pos_instance (blocks);
            pos_instance.child ("pos");
            pos_instance.child ();
            int x = pos_instance.get_int ();
            block_info->pos->x = x;
            pos_instance.next ();
            int y = pos_instance.get_int ();
            block_info->pos->y = y;
            pos_instance.next ();
            int z = pos_instance.get_int ();
            block_info->pos->z = z;

            block_info->index = pos->x * pos->z * y + pos->x * z + x;

            auto nbt (blocks);
            if (nbt.child ("nbt"))
                {
                    DhNbtInstance ni = nbt.dup_current_as_original (false);
                    block_info->nbt_instance = new DhNbtInstance (ni);
                }
            else
                block_info->nbt_instance = nullptr;

            auto block_palette = (Palette *)(pa->pdata[block_info->palette]);
            g_ptr_array_add (array, block_info);
        }
    g_ptr_array_sort_values (array, compare_block_info);
    return array;
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
    if (g_str_equal ("Schematic", instance->get_key ()))
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
    region->palette_array = array;
}

static void
new_schem_get_block_info (Region *region, DhNbtInstance instance)
{
    /* Get array first */
    int len = 0;
    instance.child ("BlockData");
    auto arr = instance.get_byte_array (len);
    instance.parent ();

    auto array = g_ptr_array_new_with_free_func (block_info_free);
    int x = -1;
    int y = 0;
    int z = 0;
    int region_x = region->region_size->x;
    int region_y = region->region_size->y;
    int region_z = region->region_size->z;
    for (int i = 0; i < len; i++)
        {
            BlockInfo *info = g_new0 (BlockInfo, 1);
            info->index = i;
            info->palette = arr[i];
            /* Add x first */
            if (x < region_x - 1)
                x++;
            else if (z < region_z - 1)
                {
                    x = 0;
                    z++;
                }
            else if (y < region_y - 1)
                {
                    x = 0;
                    z = 0;
                    y++;
                }
            Pos *pos = g_new0 (Pos, 1);
            pos->x = x;
            pos->y = y;
            pos->z = z;
            info->pos = pos;
            Palette *palette = (Palette *)region->palette_array->pdata[arr[i]];
            g_ptr_array_add (array, info);
        }

    instance.child ("BlockEntities");
    if (instance.child ())
        {
            for (; instance.is_non_null (); instance.next ())
                {
                    DhNbtInstance entity
                        = instance.dup_current_as_original (false);
                    entity.child ("Pos");
                    int pos_len = 0;
                    auto pos = entity.get_int_array (pos_len);
                    entity.parent ();

                    /* block_info->index = pos->x * pos->z * y + pos->x * z + x
                     */
                    int index = region_x * region_z * pos[1]
                                + region_x * pos[2] + pos[0];
                    BlockInfo *info = (BlockInfo *)array->pdata[index];
                    entity.set_key ("nbt");
                    DhNbtInstance id (entity);
                    id.child ("Id");
                    id.set_key ("id");
                    entity.rm_node ("Pos");
                    info->nbt_instance = new DhNbtInstance (entity);
                }
        }
    region->block_info_array = array;
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
            /* Fill BlockInfo */
            new_schem_get_block_info (region, instance_dup);
            return region;
        }
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

    /* Fill BlockInfoArray */
    BlockInfoArray *bia = get_block_full_info_from_lr (lr, region);
    region->block_info_array = bia;

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

static Region *
region_new_from_nbt_instance (DhNbtInstance instance)
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

            /* Fill BlockInfoArray */
            BlockInfoArray *bia = get_block_full_info_from_nbt_instance (
                instance, pa, rs, region);
            region->block_info_array = bia;

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
palette_is_same (Palette *a, Palette *b)
{
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
            for (int i = 0; i < a_name->num; i++)
                {
                    for (int j = 0; j < b_name->num; j++)
                        if (g_str_equal (a_name->val[i], b_name->val[j]))
                            if (!g_str_equal (a_data->val[i], b_data->val[j]))
                                return FALSE;
                }
            return TRUE;
        }
    return FALSE;
}

Palette *
region_get_palette (Region *region, int val)
{
    return static_cast<Palette *> (region->palette_array->pdata[val]);
}

char *
block_info_get_id_name (Region *region, BlockInfo *info)
{
    auto palette = region_get_palette (region, info->palette);
    return palette->id_name;
}

static void
palette_minus_one (Region *region, int startFrom, int replaceNum)
{
    for (int i = 0; i < region->block_info_array->len; i++)
        {
            auto info = static_cast<BlockInfo *> (
                region->block_info_array->pdata[i]);
            if (info->palette > startFrom)
                info->palette--;
            if (info->palette == startFrom)
                info->palette = replaceNum;
        }
}

void
region_modify_property (Region *region, BlockInfo *info, gboolean all_modify,
                        DhStrArray *new_data)
{
    auto new_palette = g_new0 (Palette, 1);
    auto old_palette = region_get_palette (region, info->palette);
    new_palette->id_name = g_strdup (old_palette->id_name);
    new_palette->property_name = dh_str_array_dup (old_palette->property_name);
    new_palette->property_data = dh_str_array_dup (new_data);

    if (all_modify)
        {
            /* First replace the old palette */
            palette_free (old_palette);
            region->palette_array->pdata[info->palette] = new_palette;
            /* Try to get the same palette */
            for (int i = 0; i < region->palette_array->len; i++)
                {
                    auto current_palette = region_get_palette (region, i);
                    if (i != info->palette
                        && palette_is_same (new_palette, current_palette))
                        {
                            g_ptr_array_remove_index (region->palette_array,
                                                      i);
                            palette_minus_one (region, i, info->palette);
                            return;
                        }
                }
            /* If not same palette found, do nothing. */
        }
    else
        {
            for (int i = 0; i < region->palette_array->len; i++)
                {
                    auto current_palette = region_get_palette (region, i);
                    if (i != info->palette
                        && palette_is_same (new_palette, current_palette))
                        {
                            info->palette = i;
                            palette_free (new_palette);
                            return;
                        }
                }
            g_ptr_array_add (region->palette_array, new_palette);
            info->palette = region->palette_array->len - 1;
        }
}
void
region_modify_block (Region *region, BlockInfo *info, gboolean all_modify,
                     gboolean safe_mode, DhStrArray *property_name,
                     DhStrArray *property_data)
{
}

int
region_get_index (Region *region, int x, int y, int z)
{
    if (x >= region->region_size->x && y >= region->region_size->y
        && z >= region->region_size->z)
        return -1;
    return region->region_size->x * region->region_size->z * y
           + region->region_size->x * z + x;
}