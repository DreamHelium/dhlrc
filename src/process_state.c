/*  process_state - Processing state
    Copyright (C) 2025 Dream Helium
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

#include "process_state.h"
#include "dh_string_util.h"
#include "dhlrc_list.h"
#include "glib.h"
#include "string.h"
#include <cjson/cJSON.h>

typedef struct TmpItem
{
    gchar *name;
    guint total;
    guint half_size;
} TmpItem;

typedef GList TmpItemList;

static int
tmpitem_strcmp (gconstpointer a, gconstpointer b)
{
    return strcmp (((TmpItem *)a)->name, (const char *)b);
}

static void
tmp_item_list_add_num (TmpItemList **til, char *item_name, int num)
{
    TmpItemList *il = g_list_find_custom (*til, item_name, tmpitem_strcmp);
    if (il)
        {
            TmpItem *ti = (TmpItem *)(il->data);
            if (!ti->half_size)
                ti->total = ti->total + num;
        }
    else
        {
            TmpItem *ti = g_new0 (TmpItem, 1);
            ti->name = g_strdup (item_name);
            ti->total = num;
            ti->half_size = 0;
            *til = g_list_prepend (*til, ti);
        }
}

static void
tmp_item_list_add_half (TmpItemList **til, char *item_name, int num)
{
    TmpItemList *il = g_list_find_custom (*til, item_name, tmpitem_strcmp);
    if (il)
        {
            TmpItem *ti = (TmpItem *)(il->data);
            ti->half_size = ti->half_size + num;
        }
    else
        {
            TmpItem *ti = g_new0 (TmpItem, 1);
            ti->name = g_strdup (item_name);
            ti->total = 0;
            ti->half_size = num;
            *til = g_list_prepend (*til, ti);
        }
}

static void
tmpitem_free (gpointer ti)
{
    TmpItem *item = (TmpItem *)ti;
    g_free (item->name);
    g_free (item);
}

static void
tmpitem_list_free (TmpItemList *til)
{
    g_list_free_full (til, tmpitem_free);
}

ItemList *
item_list_new_from_region (Region *region)
{
    TmpItemList *til = NULL;
    BlackList *bl = black_list_init ();
    ReplaceList *rl = replace_list_init ();
    int size = region->region_size->x * region->region_size->y
               * region->region_size->z;
    for (int i = 0; i < size; i++)
        {
            char *id_name = region_get_id_name (region, i);
            if (black_list_scan (bl, id_name))
                continue;
            DhStrArray *arr = NULL;
            if ((arr = replace_list_replace_str_array (rl, id_name)) != NULL)
                {
                    for (int i = 0; i < arr->num; i++)
                        tmp_item_list_add_num (&til, arr->val[i], 1);
                    continue;
                }
            int palette_num = region_get_block_palette (region, i);
            Palette *palette = region->palette_array->pdata[palette_num];
            for (int i = 0;
                 palette->property_name && i < palette->property_name->num;
                 i++)
                {
                    char *name = palette->property_name->val[i];
                    char *val = palette->property_data->val[i];

                    if (g_str_equal (name, "waterlogged")
                        && g_str_equal (val, "true"))
                        tmp_item_list_add_num (&til, "minecraft:water_bucket",
                                               1);
                    if (g_str_equal (name, "type")
                        && g_str_equal (val, "double"))
                        tmp_item_list_add_num (&til, id_name, 1);
                    if (g_str_equal (name, "half"))
                        tmp_item_list_add_half (&til, id_name, 1);
                }
            tmp_item_list_add_num (&til, id_name, 1);
        }

    const gchar *description = "Add items from Region.";
    ItemList *oblock = NULL;

    /* Copy items to the ItemList */
    for (TmpItemList *tild = til; tild; tild = tild->next)
        {
            TmpItem *data = (TmpItem *)(tild->data);
            oblock = item_list_add_item (
                &oblock, data->half_size ? data->half_size / 2 : data->total,
                data->name, description);
        }

    tmpitem_list_free (til);
    return oblock;
}