/*  region_litematic - Region Structure converted from Litematic
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

#include "region_litematic.h"
#include "nbt_interface_cpp/libnbt/nbt_util.h"
#include "public_text.h"

#define DHLRC_REGION_LITEMATIC_ERROR dhlrc_region_litematic_error_quark ()

static GQuark
dhlrc_region_litematic_error_quark (void)
{
  static GQuark q;
  if G_UNLIKELY (q == 0)
    q = g_quark_from_static_string ("dhlrc-region-litematic-error-quark");
  return q;
}

static NbtNode *
goto_child_node (NbtNode *root, const char *key, GError **err)
{
  NbtNode *child_node = nbt_node_child_to_key (root, key);
  if (child_node)
    return child_node;
  g_set_error_literal (err, DHLRC_REGION_LITEMATIC_ERROR,
                       DHLRC_REGION_LITEMATIC_ERROR_NO_CHILD,
                       DHLRC_NO_CHILD_NODE);
  return NULL;
}

static int
litematic_data_version (NbtNode *root, GError **err)
{
  NbtNode *data_version_nbt
      = nbt_node_child_to_key (root, "MinecraftDataVersion");
  if (data_version_nbt)
    {
      gboolean failed = FALSE;
      int version = nbt_node_get_int (data_version_nbt, &failed);
      if (failed)
        g_set_error (err, DHLRC_REGION_LITEMATIC_ERROR,
                     DHLRC_REGION_LITEMATIC_ERROR_WRONG_TYPE, DHLRC_WRONG_TYPE,
                     DHLRC_TYPE_INT_STRING);
      return version;
    }
  else
    {
      g_set_error_literal (err, DHLRC_REGION_LITEMATIC_ERROR,
                           DHLRC_REGION_LITEMATIC_ERROR_NO_CHILD,
                           DHLRC_NO_CHILD_NODE);
      return -1;
    }
}

static void
litematic_fill_data (NbtNode *root, Region *region, GError **err, int i)
{
  NbtNode *metadata = nbt_node_child_to_key (root, "Metadata");
  if (metadata)
    {
      gboolean failed = FALSE;
      NbtNode *time_created_node
          = goto_child_node (metadata, "TimeCreated", err);
      if (!time_created_node)
        return;
      gint64 time_created = nbt_node_get_long (time_created_node, &failed);

      NbtNode *time_modified_node
          = goto_child_node (metadata, "TimeModified", err);
      if (!time_modified_node)
        return;
      gint64 time_modified = nbt_node_get_long (time_modified_node, &failed);

      if (failed)
        goto wrong_long_type;

      NbtNode *description_node
          = goto_child_node (metadata, "Description", err);
      if (!description_node)
        return;
      const char *description
          = nbt_node_get_string (description_node, &failed);

      NbtNode *author_node = goto_child_node (metadata, "Author", err);
      if (!author_node)
        return;
      const char *author = nbt_node_get_string (author_node, &failed);

      if (failed)
        goto wrong_string_type;

      BaseData *data = g_new0 (BaseData, 1);
      data->create_time = g_date_time_new_from_unix_utc (time_created / 1000);
      data->modify_time = g_date_time_new_from_unix_utc (time_modified / 1000);
      data->description = g_strdup (description);
      data->author = g_strdup (author);

      NbtNode *region_node = goto_child_node (root, "Regions", err);
      if (!region_node)
        return;
      region_node = region_node->children;
      for (int j = 0; j < i && region_node; j++)
        region_node = region_node->next;
      if (!region_node)
        goto no_child;

      data->name = g_strdup (nbt_node_get_key (region_node));
      region->data = data;
      return;
    }
no_child:
  g_set_error_literal (err, DHLRC_REGION_LITEMATIC_ERROR,
                       DHLRC_REGION_LITEMATIC_ERROR_NO_CHILD,
                       DHLRC_NO_CHILD_NODE);
  return;
wrong_long_type:
  g_set_error (err, DHLRC_REGION_LITEMATIC_ERROR,
               DHLRC_REGION_LITEMATIC_ERROR_WRONG_TYPE, DHLRC_WRONG_TYPE,
               DHLRC_TYPE_LONG_STRING);
  return;
wrong_string_type:
  g_set_error (err, DHLRC_REGION_LITEMATIC_ERROR,
               DHLRC_REGION_LITEMATIC_ERROR_WRONG_TYPE, DHLRC_WRONG_TYPE,
               DHLRC_TYPE_STRING_STRING);
}

static void
litematic_fill_pos (NbtNode *root, Region *region, GError **err)
{
  NbtNode *size_node = goto_child_node (root, "Size", err);
  if (!size_node)
    return;

  NbtNode *x_node = goto_child_node (size_node, "x", err);
  NbtNode *y_node = goto_child_node (size_node, "y", err);
  NbtNode *z_node = goto_child_node (size_node, "z", err);
  if (x_node && y_node && z_node)
    {
      gboolean failed = FALSE;
      int x = ABS (nbt_node_get_int (x_node, &failed));
      int y = ABS (nbt_node_get_int (y_node, &failed));
      int z = ABS (nbt_node_get_int (z_node, &failed));

      if (failed)
        {
          g_set_error (err, DHLRC_REGION_LITEMATIC_ERROR,
                       DHLRC_REGION_LITEMATIC_ERROR_WRONG_TYPE,
                       DHLRC_WRONG_TYPE, DHLRC_TYPE_INT_STRING);
          return;
        }
      region->region_size = g_new0 (RegionSize, 1);
      region->region_size->x = x;
      region->region_size->y = y;
      region->region_size->z = z;
    }
}

static void
palette_free (gpointer mem)
{
  Palette *palette = mem;
  g_free (palette->id_name);
  dh_str_array_free (palette->property_name);
  dh_str_array_free (palette->property_data);
  g_free (mem);
}

static void
block_entity_free (gpointer mem)
{
  BlockEntity *entity = mem;
  nbt_node_free (entity->nbt_node);
  g_free (entity->pos);
  g_free (entity);
}

static void
litematic_fill_entity_array (NbtNode *root, Region *region, GError **err)
{
  region->block_entity_array
      = g_ptr_array_new_with_free_func (block_entity_free);
  NbtNode *entities = nbt_node_child_to_key (root, "TileEntities");
  if (!entities)
    goto no_child;
  entities = entities->children;
  // if (!entities)
  //   goto no_child;
  while (entities)
    {
      NbtNode *x_node = nbt_node_child_to_key (entities, "x");
      NbtNode *y_node = nbt_node_child_to_key (entities, "y");
      NbtNode *z_node = nbt_node_child_to_key (entities, "z");
      if (!x_node || !y_node || !z_node)
        goto no_child;

      gboolean failed = FALSE;
      int x = nbt_node_get_int (x_node, &failed);
      int y = nbt_node_get_int (y_node, &failed);
      int z = nbt_node_get_int (z_node, &failed);

      if (failed)
        goto wrong_int_type;

      BlockEntity *entity = g_new0 (BlockEntity, 1);
      entity->pos = g_new0 (Pos, 1);
      entity->pos->x = x;
      entity->pos->y = y;
      entity->pos->z = z;

      int index = region_get_index (region, x, y, z);

      NbtNode *tile_entities = nbt_node_dup (entities);
      nbt_node_remove_node_key (tile_entities, "x");
      nbt_node_remove_node_key (tile_entities, "y");
      nbt_node_remove_node_key (tile_entities, "z");
      nbt_node_reset_key (tile_entities, "nbt");

      /* It seems that some version lacks id, we temporarily add one. */
      NbtNode *entities_child = nbt_node_child_to_key (tile_entities, "id");
      if (!entities_child)
        {
          int palette_num = region_get_block_palette (region, index);
          Palette *palette = region_get_palette (region, palette_num);
          char *name = palette->id_name;
          NbtNode *id_nbt = nbt_node_new_string ("id", name);

          nbt_node_insert_before (tile_entities, NULL, id_nbt);
        }

      entity->nbt_node = tile_entities;
      g_ptr_array_add (region->block_entity_array, entity);

      entities = entities->next;
    }
  return;
no_child:
  g_set_error_literal (err, DHLRC_REGION_LITEMATIC_ERROR,
                       DHLRC_REGION_LITEMATIC_ERROR_NO_CHILD,
                       DHLRC_NO_CHILD_NODE);
  g_ptr_array_free (region->palette_array, TRUE);
  return;
wrong_int_type:
  g_set_error (err, DHLRC_REGION_LITEMATIC_ERROR,
               DHLRC_REGION_LITEMATIC_ERROR_WRONG_TYPE, DHLRC_WRONG_TYPE,
               DHLRC_TYPE_INT_STRING);
  g_ptr_array_free (region->palette_array, TRUE);
}

static void
litematic_fill_palettes (NbtNode *root, Region *region, GError **err)
{
  NbtNode *palette = goto_child_node (root, "BlockStatePalette", err);
  if (!palette)
    return;
  palette = palette->children;
  region->palette_array = g_ptr_array_new_with_free_func (palette_free);
  if (!palette)
    goto no_child;

  while (palette)
    {
      const char *name = NULL;
      DhStrArray *properties_name = NULL;
      DhStrArray *properties_data = NULL;

      NbtNode *name_node = goto_child_node (palette, "Name", err);
      if (!name_node)
        goto no_child;
      name = nbt_node_get_string (name_node, NULL);
      if (!name)
        goto wrong_string_type;

      NbtNode *properties_node = nbt_node_child_to_key (palette, "Properties");
      if (properties_node)
        {
          properties_node = properties_node->children;
          if (!properties_node)
            goto no_child;
          const char *single_name = NULL;
          const char *single_data = NULL;
          while (properties_node)
            {
              single_name = nbt_node_get_key (properties_node);
              single_data = nbt_node_get_string (properties_node, NULL);
              if (!single_data)
                {
                  dh_str_array_free (properties_name);
                  dh_str_array_free (properties_data);
                  goto wrong_string_type;
                }
              dh_str_array_add_str (&properties_name, single_name);
              dh_str_array_add_str (&properties_data, single_data);
              properties_node = properties_node->next;
            }
        }
      Palette *palette_data = g_new0 (Palette, 1);
      palette_data->id_name = g_strdup (name);
      palette_data->property_name = properties_name;
      palette_data->property_data = properties_data;

      g_ptr_array_add (region->palette_array, palette_data);

      palette = palette->next;
    }
  return;

no_child:
  g_set_error_literal (err, DHLRC_REGION_LITEMATIC_ERROR,
                       DHLRC_REGION_LITEMATIC_ERROR_NO_CHILD,
                       DHLRC_NO_CHILD_NODE);
  g_ptr_array_free (region->palette_array, TRUE);
  return;

wrong_string_type:
  g_set_error (err, DHLRC_REGION_LITEMATIC_ERROR,
               DHLRC_REGION_LITEMATIC_ERROR_WRONG_TYPE, DHLRC_WRONG_TYPE,
               DHLRC_TYPE_STRING_STRING);
  g_ptr_array_free (region->palette_array, TRUE);
}

Region *
region_new_from_litematic (NbtNode *root, int i, GError **err,
                           DhProgressFullSet func, void *main_klass)
{
  Region *region = g_new0 (Region, 1);
  GError *internal_error = NULL;

  if (func && main_klass)
    func (main_klass, 0,
          _ ("Starting to parse litematic NBT node to Region."));

  /* Fill DataVersion */
  int version = litematic_data_version (root, &internal_error);
  if (internal_error)
    goto fail;
  region->data_version = version;

  /* Fill Data */
  litematic_fill_data (root, region, &internal_error, i);
  if (internal_error)
    goto fail;

  NbtNode *region_node = nbt_node_child_to_key (root, "Regions");
  if (!region_node)
    goto no_child;
  region_node = region_node->children;
  for (int j = 0; j < i && region_node; j++)
    region_node = region_node->next;
  if (!region_node)
    goto no_child;

  /* Fill RegionSize */
  litematic_fill_pos (region_node, region, &internal_error);
  if (internal_error)
    goto fail;

  /* Fill PaletteArray */
  litematic_fill_palettes (region_node, region, &internal_error);
  if (internal_error)
    goto fail;

  /* Fill BlockArray */
  NbtNode *states_node = nbt_node_child_to_key (region_node, "BlockStates");
  if (!states_node)
    goto no_child;
  gboolean failed = FALSE;
  const gint64 *states = nbt_node_get_long_array (
      states_node, &region->block_array_len, &failed);
  if (failed)
    {
      g_set_error (err, DHLRC_REGION_LITEMATIC_ERROR,
                   DHLRC_REGION_LITEMATIC_ERROR_WRONG_TYPE, DHLRC_WRONG_TYPE,
                   DHLRC_TYPE_LONG_ARRAY_STRING);
      goto fail;
    }
  region->block_array = g_new0 (gint64, region->block_array_len);
  memcpy (region->block_array, states,
          sizeof (gint64) * region->block_array_len);

  /* Fill BlockEntityArray */
  litematic_fill_entity_array (region_node, region, &internal_error);
  if (internal_error)
    goto fail;

  if (func && main_klass)
    func (main_klass, 100, _ ("Finish parsing litematic NBT node to Region."));
  return region;

no_child:
  g_set_error_literal (&internal_error, DHLRC_REGION_LITEMATIC_ERROR,
                       DHLRC_REGION_LITEMATIC_ERROR_NO_CHILD,
                       DHLRC_NO_CHILD_NODE);
fail:
  if (err)
    *err = internal_error;
  else
    g_error_free (internal_error);
  if (func && main_klass)
    func (main_klass, 0,
          _ ("Failed when parsing litematic NBT node to Region."));
  region_free (region);
  return NULL;
}