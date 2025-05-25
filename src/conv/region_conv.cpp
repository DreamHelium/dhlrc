#include "../dh_bit.h"
#include "../nbt_interface_cpp/nbt_interface.hpp"
#include "../region.h"

static gint64 *
region_get_palette_num_from_region (Region *region, int *length)
{
    PaletteArray *arr = region->palette_array;
    int num = arr->len;
    BlockInfoArray *info_array = region->block_info_array;
    int block_num = info_array->len;
    int move_bits = g_bit_storage (num - 1) <= 2 ? 2 : g_bit_storage (num - 1);
    int len = move_bits * block_num / 64 + 1;

    DhBit *bit = dh_bit_new_with_len (len);
    gint64 *ret = NULL;
    int before = -1;

    for (int id = 0; id < block_num; id++)
        {
            guint palette = region->air_palette;
            BlockInfo *info = (BlockInfo *)region->block_info_array->pdata[id];
            if (before + 1 != info->index) /* Before and index has air */
                {
                    for (int i = before + 1; i < info->index; i++)
                        dh_bit_push_back_val (bit, move_bits, palette);
                    /* Fill air */
                }
            before = info->index;
            palette = info->palette;
            dh_bit_push_back_val (bit, move_bits, palette);
        }

    ret = dh_bit_dup_array (bit, length);
    dh_bit_free (bit);
    return ret;
}

static DhNbtInstance
size_nbt_instance_new (Pos *pos, const char *key)
{
    DhNbtInstance ret (DH_TYPE_List, key, true);

    DhNbtInstance x (pos->x, nullptr, true);
    DhNbtInstance y (pos->y, nullptr, true);
    DhNbtInstance z (pos->z, nullptr, true);
    ret.insert_before ({}, x);
    ret.insert_before ({}, y);
    ret.insert_before ({}, z);

    return ret;
}

static DhNbtInstance
block_nbt_instance_new (BlockInfo *info)
{
    DhNbtInstance ret (DH_TYPE_Compound, nullptr, true);
    DhNbtInstance nbt{};
    if (info->nbt_instance)
        nbt = ((DhNbtInstance *)(info->nbt_instance))
                  ->dup_current_as_original (true);
    DhNbtInstance pos = size_nbt_instance_new (info->pos, "pos");

    ret.prepend (nbt.is_non_null () ? nbt : pos);

    if (nbt.is_non_null ())
        ret.insert_before ({}, pos);
    DhNbtInstance state (info->palette, "state", true);
    ret.insert_before ({}, state);

    return ret;
}

static DhNbtInstance
blocks_nbt_instance_new (BlockInfoArray *array)
{
    DhNbtInstance ret (DH_TYPE_List, "blocks", true);
    for (int i = 0; i < array->len; i++)
        {
            DhNbtInstance cur
                = block_nbt_instance_new ((BlockInfo *)(array->pdata[i]));
            /* before is too slow */
            ret.insert_after ({}, cur);
        }
    return ret;
}

static DhNbtInstance
properties_nbt_instance_new (DhStrArray *name, DhStrArray *data)
{
    if (!name)
        return {};
    DhNbtInstance ret (DH_TYPE_Compound, "Properties", true);
    for (int i = 0; name && i < name->num; i++)
        {
            DhNbtInstance cur (data->val[i], name->val[i], true);
            ret.insert_before ({}, cur);
        }
    return ret;
}

static DhNbtInstance
palette_nbt_instance_new (Palette *palette)
{
    DhNbtInstance ret (DH_TYPE_Compound, nullptr, true);
    DhNbtInstance properties = properties_nbt_instance_new (
        palette->property_name, palette->property_data);
    DhNbtInstance name (palette->id_name, "Name", true);
    if (properties.is_non_null ())
        ret.insert_before ({}, properties);
    ret.insert_before ({}, name);

    return ret;
}

static DhNbtInstance
palettes_nbt_instance_new (PaletteArray *array)
{
    DhNbtInstance ret (DH_TYPE_List, "palette", true);
    for (int i = 0; i < array->len; i++)
        {
            DhNbtInstance cur
                = palette_nbt_instance_new ((Palette *)(array->pdata[i]));
            ret.insert_before ({}, cur);
        }
    return ret;
}

static DhNbtInstance
entities_nbt_instance_new ()
{
    return DhNbtInstance (DH_TYPE_List, "entities", true);
}

static DhNbtInstance
tile_entities_nbt_instance_new (BlockInfoArray *array)
{
    DhNbtInstance ret (DH_TYPE_List, "TileEntities", true);
    for (int i = 0; i < array->len; i++)
        {
            BlockInfo *info = (BlockInfo *)array->pdata[i];
            if (info->nbt_instance)
                {
                    DhNbtInstance entity
                        = ((DhNbtInstance *)(info->nbt_instance))
                              ->dup_current_as_original (true);
                    DhNbtInstance x (info->pos->x, "x", true);
                    DhNbtInstance y (info->pos->y, "y", true);
                    DhNbtInstance z (info->pos->z, "z", true);
                    entity.insert_before ({}, x);
                    entity.insert_before ({}, y);
                    entity.insert_before ({}, z);
                    entity.set_key (nullptr);
                    ret.insert_before ({}, entity);
                }
        }
    return ret;
}

static DhNbtInstance
lite_metadata_new (Region *region)
{
    DhNbtInstance ret (DH_TYPE_Compound, "Metadata", true);
    DhNbtInstance ct (g_date_time_to_unix (region->data->create_time) * 1000,
                      "TimeCreated", true);
    DhNbtInstance mt (g_date_time_to_unix (region->data->modify_time) * 1000,
                      "TimeModified", true);
    ret.insert_before ({}, ct);
    ret.insert_before ({}, mt);

    DhNbtInstance enclosing_size (DH_TYPE_Compound, "EnclosingSize", true);
    DhNbtInstance x (region->region_size->x, "x", true);
    DhNbtInstance y (region->region_size->y, "y", true);
    DhNbtInstance z (region->region_size->z, "z", true);
    enclosing_size.insert_before ({}, x);
    enclosing_size.insert_before ({}, y);
    enclosing_size.insert_before ({}, z);
    ret.insert_before ({}, enclosing_size);

    DhNbtInstance description (region->data->description, "Description", true);
    DhNbtInstance author (region->data->author, "Author", true);
    DhNbtInstance name (region->data->name, "Name", true);
    ret.insert_before ({}, description);
    ret.insert_before ({}, author);
    ret.insert_before ({}, name);

    DhNbtInstance region_count ((gint32)1, "RegionCount", true);
    ret.insert_before ({}, region_count);

    gint32 volume = region->region_size->x * region->region_size->y
                    * region->region_size->z;
    DhNbtInstance total_blocks ((gint32)(region->block_info_array->len),
                                "TotalBlocks", true);
    DhNbtInstance total_volume (volume, "TotalVolume", true);
    ret.insert_before ({}, total_blocks);
    ret.insert_before ({}, total_volume);

    return ret;
}

static DhNbtInstance
lite_regions_new (Region *region)
{
    DhNbtInstance ret (DH_TYPE_Compound, "Regions", true);
    DhNbtInstance name (DH_TYPE_Compound, region->data->name, true);
    ret.insert_before ({}, name);

    int len = 0;
    gint64 *states = region_get_palette_num_from_region (region, &len);
    DhNbtInstance block_states (states, len, "BlockStates", true);
    free (states);
    name.insert_before ({}, block_states);

    DhNbtInstance pbt (DH_TYPE_List, "PendingBlockTicks", true);
    DhNbtInstance pft (DH_TYPE_List, "PendingFluidTicks", true);
    DhNbtInstance entities (DH_TYPE_List, "Entities", true);
    name.insert_before ({}, pbt);
    name.insert_before ({}, pft);
    name.insert_before ({}, entities);

    DhNbtInstance position (DH_TYPE_Compound, "Position", true);
    DhNbtInstance px ((gint32)0, "x", true);
    DhNbtInstance py ((gint32)0, "y", true);
    DhNbtInstance pz ((gint32)0, "z", true);
    position.insert_before ({}, px);
    position.insert_before ({}, py);
    position.insert_before ({}, pz);
    name.insert_before ({}, position);

    DhNbtInstance size (DH_TYPE_Compound, "Size", true);
    DhNbtInstance sx (region->region_size->x, "x", true);
    DhNbtInstance sy (region->region_size->y, "y", true);
    DhNbtInstance sz (region->region_size->z, "z", true);
    size.insert_before ({}, sx);
    size.insert_before ({}, sy);
    size.insert_before ({}, sz);
    name.insert_before ({}, size);

    DhNbtInstance palette = palettes_nbt_instance_new (region->palette_array);
    palette.set_key ("BlockStatePalette");
    name.insert_before ({}, palette);

    DhNbtInstance tile_entities
        = tile_entities_nbt_instance_new (region->block_info_array);
    name.insert_before ({}, tile_entities);

    return ret;
}

static DhNbtInstance
new_schema_fill_palette (PaletteArray *pa)
{
    DhNbtInstance ret (DH_TYPE_Compound, "Palette", true);
    for (int i = 0; i < pa->len; i++)
        {
            Palette *palette = (Palette *)pa->pdata[i];
            char *palette_str = nullptr;
            if (palette->property_data)
                {
                    DhStrArray *arr = dh_str_array_cat_str_array (
                        palette->property_name, palette->property_data, "=");
                    char *str = dh_str_array_cat_with_split (arr, ",");
                    dh_str_array_free (arr);
                    palette_str
                        = g_strdup_printf ("%s[%s]", palette->id_name, str);
                    free (str);
                }
            else
                palette_str = g_strdup (palette->id_name);

            DhNbtInstance palette_nbt (i, palette_str, true);
            g_free (palette_str);
            ret.insert_before ({}, palette_nbt);
        }
    return ret;
}

static DhNbtInstance
new_schema_fill_metadata ()
{
    DhNbtInstance ret (DH_TYPE_Compound, "Metadata", true);
    DhNbtInstance x ((gint32)0, "WEOffsetX", true);
    DhNbtInstance y ((gint32)0, "WEOffsetY", true);
    DhNbtInstance z ((gint32)0, "WEOffsetZ", true);

    ret.insert_before ({}, x);
    ret.insert_before ({}, y);
    ret.insert_before ({}, z);
    return ret;
}

static DhNbtInstance
new_schema_fill_block_data (BlockInfoArray *array)
{
    int len = array->len;
    gint8 *ptr = g_new0 (gint8, len);
    for (int i = 0; i < len; i++)
        {
            BlockInfo *info = (BlockInfo *)array->pdata[i];
            ptr[i] = info->palette;
        }
    DhNbtInstance block_data (ptr, len, "BlockData", true);
    g_free (ptr);
    return block_data;
}

static DhNbtInstance
new_schema_fill_block_entities (BlockInfoArray *array)
{
    DhNbtInstance ret (DH_TYPE_List, "BlockEntities", true);
    for (int i = 0; i < array->len; i++)
        {
            BlockInfo *info = (BlockInfo *)array->pdata[i];
            if (info->nbt_instance)
                {
                    DhNbtInstance entity
                        = ((DhNbtInstance *)(info->nbt_instance))
                              ->dup_current_as_original (true);
                    entity.set_key (nullptr);
                    DhNbtInstance id (entity);
                    id.child ("id");
                    id.set_key ("Id");
                    int pos[3] = { info->pos->x, info->pos->y, info->pos->z };
                    DhNbtInstance pos_nbt (pos, 3, "Pos", true);
                    entity.insert_before ({}, pos_nbt);
                    ret.insert_before ({}, entity);
                }
        }
    return ret;
}

static void
new_schema_fill_non_compound (DhNbtInstance &instance, Region *region)
{
    DhNbtInstance palette_max ((gint32)region->palette_array->len,
                               "PaletteMax", true);
    instance.insert_before ({}, palette_max);
    DhNbtInstance version ((gint32)2, "Version", true);
    instance.insert_before ({}, version);

    gint16 width = region->region_size->x;
    gint16 length = region->region_size->z;
    gint16 height = region->region_size->y;

    DhNbtInstance width_i (width, "Width", true);
    DhNbtInstance length_i (length, "Length", true);
    DhNbtInstance height_i (height, "Height", true);
    instance.insert_before ({}, width_i);
    instance.insert_before ({}, length_i);
    instance.insert_before ({}, height_i);

    DhNbtInstance data_version (region->data_version, "DataVersion", true);
    instance.insert_before ({}, data_version);
}

extern "C"
{

    void *
    nbt_instance_ptr_new_from_region (Region *region, gboolean temp_root)
    {
        DhNbtInstance ret (DH_TYPE_Compound, NULL, temp_root);
        /* First is size */
        DhNbtInstance size_nbt
            = size_nbt_instance_new (region->region_size, "size");
        ret.insert_before ({}, size_nbt);
        /* Second is entities */
        DhNbtInstance entities_nbt = entities_nbt_instance_new ();
        ret.insert_before ({}, entities_nbt);
        /* Third is blocks */
        DhNbtInstance blocks_nbt
            = blocks_nbt_instance_new (region->block_info_array);
        ret.insert_before ({}, blocks_nbt);
        /* Fourth is palette */
        DhNbtInstance palette_nbt
            = palettes_nbt_instance_new (region->palette_array);
        ret.insert_before ({}, palette_nbt);
        /* Fifth is DataVersion */
        DhNbtInstance data_version_nbt (region->data_version, "DataVersion",
                                        true);
        ret.insert_before ({}, data_version_nbt);
        return new DhNbtInstance (ret);
    }

    void *
    lite_instance_ptr_new_from_region (Region *region, gboolean temp_root)
    {
        DhNbtInstance ret (DH_TYPE_Compound, nullptr, temp_root);
        /* First is DataVersion */
        DhNbtInstance data_version_nbt (region->data_version,
                                        "MinecraftDataVersion", true);
        ret.insert_before ({}, data_version_nbt);
        /* Second is Version, default to 5 */
        DhNbtInstance version_nbt ((gint32)5, "Version", true);
        ret.insert_before ({}, version_nbt);
        /* Third is metadata */
        DhNbtInstance metadata_nbt = lite_metadata_new (region);
        ret.insert_before ({}, metadata_nbt);
        /* Then is the regions */
        DhNbtInstance regions = lite_regions_new (region);
        ret.insert_before ({}, regions);
        return new DhNbtInstance (ret);
    }

    void *
    new_schema_instance_ptr_new_from_region (Region *region,
                                             gboolean temp_root)
    {
        DhNbtInstance ret (DH_TYPE_Compound, "Schematic", temp_root);
        /* First is Non-Compound info */
        new_schema_fill_non_compound (ret, region);
        /* Second is palette */
        DhNbtInstance palette
            = new_schema_fill_palette (region->palette_array);
        ret.insert_before ({}, palette);
        /* Third is metadata */
        DhNbtInstance metadata = new_schema_fill_metadata ();
        ret.insert_before ({}, metadata);
        /* Fourth is blockdata */
        DhNbtInstance block_data
            = new_schema_fill_block_data (region->block_info_array);
        ret.insert_before ({}, block_data);
        DhNbtInstance block_entities
            = new_schema_fill_block_entities (region->block_info_array);
        ret.insert_before ({}, block_entities);
        /* Last is offset */
        int offset[3] = { 0, 0, 0 };
        DhNbtInstance offset_nbt (offset, 3, "Offset", true);
        ret.insert_before ({}, offset_nbt);

        return new DhNbtInstance (ret);
    }
}
