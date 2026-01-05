#include "../global_variant.h"
#include "../nbt_interface_cpp/libnbt/nbt.h"
#include "../nbt_interface_cpp/libnbt/nbt_util.h"
#include "../region.h"
#include "../translation.h"
#include "../nbt_interface_cpp/nbt_interface.hpp"

#define DHLRC_REGION_CONV_ERROR dhlrc_region_conv_error_quark ()

typedef enum
{
  DHLRC_REGION_CONV_ERROR_CANCELLED
} DhlrcRegionConvError;

static GQuark
dhlrc_region_conv_error_quark (void)
{
  static GQuark q;
  if G_UNLIKELY (q == 0)
    q = g_quark_from_static_string ("dhlrc-region-conv-error-quark");
  return q;
}

static NbtNode *
size_nbt_new (Pos *pos, const char *key)
{
  NbtNode *ret = nbt_node_new_list (key);

  NbtNode *x = nbt_node_new_int (NULL, pos->x);
  NbtNode *y = nbt_node_new_int (NULL, pos->y);
  NbtNode *z = nbt_node_new_int (NULL, pos->z);
  nbt_node_insert_after (ret, NULL, x);
  nbt_node_insert_after (ret, x, y);
  nbt_node_insert_after (ret, y, z);

  return ret;
}

static gboolean
find_same_block (const GPtrArray *arr, const char *name)
{
  for (int i = 0; i < arr->len; i++)
    {
      if (g_str_equal (name, arr->pdata[i]))
        return TRUE;
    }
  return FALSE;
}

static void
size_change (Region *region, int *x, int *y, int *z)
{
  if (*x < region->region_size->x - 1)
    (*x)++;
  else if (*z < region->region_size->z - 1)
    {
      *x = 0;
      (*z)++;
    }
  else if (*y < region->region_size->y - 1)
    {
      *x = 0;
      *z = 0;
      (*y)++;
    }
}

static NbtNode *
blocks_nbt_new (Region *region, gboolean ignore_air, GError **err,
                DhProgressFullSet set_func, void *main_klass,
                GCancellable *cancellable)
{
  int len = region->region_size->x * region->region_size->y
            * region->region_size->z;
  NbtNode *ret = nbt_node_new_list ("blocks");
  int x = 0;
  int y = 0;
  int z = 0;
  clock_t start = clock ();
  for (int i = 0; i < len; i++)
    {
      const char *name = region_get_id_name (region, i);
      char *value = g_strdup_printf ("Adding block: %s", name);
      if (set_func && main_klass)
        {
          clock_t now = clock ();
          int passed_ms = 1000 * (now - start) / CLOCKS_PER_SEC;
          if (passed_ms % 500 == 0 || ((i + 1) == len))
            set_func (main_klass, (i + 1) * 100 / len, value);
        }
      g_free (value);
      if (g_cancellable_is_cancelled (cancellable))
        {
          g_set_error_literal (err, DHLRC_REGION_CONV_ERROR,
                               DHLRC_REGION_CONV_ERROR_CANCELLED,
                               _ ("The task was cancelled."));
          nbt_node_free (ret);
          ret = NULL;
          break;
        }
      if (ignore_air && find_same_block (dhlrc_get_ignore_air_list (), name))
        {
          size_change (region, &x, &y, &z);
          continue;
        }

      Pos *pos_pos = g_new0 (Pos, 1);
      pos_pos->x = x;
      pos_pos->y = y;
      pos_pos->z = z;
      NbtNode *cur = nbt_node_new_compound (NULL);
      NbtNode *nbt = NULL;

      for (int j = 0; j < region->block_entity_array->len; j++)
        {
          BlockEntity *be
              = (BlockEntity *)(region->block_entity_array->pdata[j]);
          if (be->pos->x == x && be->pos->y == y && be->pos->z == z
              && be->nbt_instance)
            {
              NbtNode *original_nbt
                  = ((DhNbtInstance *)(be->nbt_instance))->get_original_nbt ();
              nbt = nbt_node_dup (original_nbt);
            }
        }

      NbtNode *pos = size_nbt_new (pos_pos, "pos");

      nbt_node_prepend (cur, nbt ? nbt : pos);

      if (nbt)
        {
          nbt_node_insert_after (cur, nbt, pos);
        }
      g_free (pos_pos);
      NbtNode *state
          = nbt_node_new_int ("state", region_get_block_palette (region, i));
      nbt_node_insert_after (cur, pos, state);
      nbt_node_prepend (ret, cur);

      size_change (region, &x, &y, &z);
    }
  return ret;
}

static NbtNode *
properties_nbt_new (DhStrArray *name, DhStrArray *data)
{
  if (!name)
    return NULL;
  NbtNode *ret = nbt_node_new_compound ("Properties");
  for (int i = 0; i < name->num; i++)
    {
      NbtNode *cur = nbt_node_new_string (name->val[i], data->val[i]);
      nbt_node_append (ret, cur);
    }
  return ret;
}

static NbtNode *
palette_nbt_new (Palette *palette)
{
  NbtNode *ret = nbt_node_new_compound (NULL);
  NbtNode *properties
      = properties_nbt_new (palette->property_name, palette->property_data);
  NbtNode *name = nbt_node_new_string ("Name", palette->id_name);
  if (properties)
    nbt_node_append (ret, properties);
  nbt_node_append (ret, name);

  return ret;
}

static NbtNode *
palettes_nbt_new (PaletteArray *array)
{
  NbtNode *ret = nbt_node_new_list ("palette");
  for (int i = 0; i < array->len; i++)
    {
      NbtNode *cur = palette_nbt_new ((Palette *)(array->pdata[i]));
      nbt_node_append (ret, cur);
    }
  return ret;
}

static NbtNode *
entities_nbt_new ()
{
  return nbt_node_new_list ("entities");
}

extern "C"
{
  NbtNode *
  nbt_node_new_from_region_full (Region *region, gboolean ignore_air,
                                 DhProgressFullSet set_func, void *main_klass,
                                 GCancellable *cancellable, GError **err)
  {
    NbtNode *ret = nbt_node_new_compound (NULL);
    /* First is size */
    NbtNode *size_nbt = size_nbt_new (region->region_size, "size");
    nbt_node_insert_after (ret, NULL, size_nbt);
    /* Second is entities */
    NbtNode *entities_nbt = entities_nbt_new ();
    nbt_node_insert_after (ret, size_nbt, entities_nbt);
    /* Third is blocks */
    NbtNode *blocks_nbt = blocks_nbt_new (region, ignore_air, err, set_func,
                                          main_klass, cancellable);
    if (g_cancellable_is_cancelled (cancellable))
      {
        nbt_node_free (ret);
        return NULL;
      }
    nbt_node_insert_after (ret, entities_nbt, blocks_nbt);
    /* Fourth is palette */
    NbtNode *palette_nbt = palettes_nbt_new (region->palette_array);
    nbt_node_insert_after (ret, blocks_nbt, palette_nbt);
    /* Fifth is DataVersion */
    NbtNode *data_version_nbt
        = nbt_node_new_int ("DataVersion", region->data_version);
    nbt_node_insert_after (ret, palette_nbt, data_version_nbt);

    return ret;
  }
}