/*  litematica_region - the region utilities for litematic file
    Copyright (C) 2022 Dream Helium
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

#include "litematica_region.h"
#include "dh_string_util.h"
#include "nbt_interface_cpp/libnbt/nbt.h"
#include "nbt_interface_cpp/libnbt/nbt_util.h"
#include "nbt_interface_cpp/nbt_interface.hpp"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TEST_OFFSET 2

typedef struct _LiteRegion
{

  /** Data Version */
  int data_version;

  /** Region name */
  char *name;

  /** Number of the region */
  int region_num;

  /** Block names and nums */
  DhStrArray *blocks;

  /** Replaced name of blocks */
  // DhStrArray* replaced_blocks;

  /** Region NBT */
  DhNbtInstance region_nbt_instance_cpp;

  /** Block states */
  const int64_t *states;

  /** numbers of BlockStates */
  int states_num;

  struct
  {
    int x;
    int y;
    int z;
  } region_size;

  /** In many cases you don't need it, it's used to get block id. */
  int move_bits;

  gint64 create_time;
  gint64 modify_time;
  char *description;
  char *author;

} _LiteRegion;

static DhStrArray *
get_blocks (DhNbtInstance region)
{
  DhStrArray *arr = nullptr;
  region.child ("BlockStatePalette");
  region.child ();
  for (; region.is_non_null (); region.next ())
    {
      auto internal_region (region);
      internal_region.child ("Name");
      dh_str_array_add_str (&arr, internal_region.get_string ());
    }
  return arr;
}

LiteRegion *
lite_region_create_from_root_instance_cpp (DhNbtInstance &root, int r_num)
{
  LiteRegion *out = new LiteRegion;

  auto data_version (root);
  data_version.goto_root ();
  data_version.child ("MinecraftDataVersion");
  out->data_version = data_version.get_int ();

  auto metadata (root);
  metadata.goto_root ();
  metadata.child ("Metadata");

  auto ct (metadata);
  ct.child ("TimeCreated");
  out->create_time = ct.get_long ();

  auto mt (metadata);
  mt.child ("TimeModified");
  out->modify_time = mt.get_long ();

  auto des (metadata);
  des.child ("Description");
  out->description = g_strdup (des.get_string ());

  auto author (metadata);
  author.child ("Author");
  out->author = g_strdup (author.get_string ());

  auto region (root);
  region.goto_root ();
  region.child ("Regions");
  region.child ();
  for (int i = 0; i < r_num; i++)
    {
      if (!region.next ())
        {
          g_free (out);
          return nullptr;
        }
    }

  out->name = g_strdup (region.get_key ());
  out->region_num = r_num;
  out->region_nbt_instance_cpp = region;
  auto region_dup (region);

  out->blocks = get_blocks (region_dup);
  auto bs (region_dup);
  bs.child ("BlockStates");
  out->states = bs.get_long_array (out->states_num);

  auto rsize (region_dup);
  rsize.child ("Size");
  auto get_pos = [] (DhNbtInstance instance, const char *str) {
    instance.child (str);
    return ABS (instance.get_int ());
  };
  out->region_size.x = get_pos (rsize, "x");
  out->region_size.y = get_pos (rsize, "y");
  out->region_size.z = get_pos (rsize, "z");

  int bits = g_bit_storage (out->blocks->num - 1);
  bits = bits <= TEST_OFFSET ? TEST_OFFSET : bits;
  out->move_bits = bits;

  return out;
}

void
lite_region_free (LiteRegion *lr)
{
  g_free (lr->name);
  g_free (lr->author);
  g_free (lr->description);
  dh_str_array_free (lr->blocks);
  delete lr;
}

int
lite_region_num_instance (void *instance)
{
  DhNbtInstance instance_copy (*(DhNbtInstance *)instance);
  if (instance_copy.child ("Regions"))
    {
      if (instance_copy.child ())
        {
          int i = 1;
          while (instance_copy.next ())
            {
              if (instance_copy.is_non_null ())
                i++;
            }
          return i;
        }
    }
  return 0;
}

DhStrArray *
lite_region_block_name_array (LiteRegion *lr)
{
  return lr->blocks;
}

int
lite_region_data_version (LiteRegion *lr)
{
  return lr->data_version;
}

int
lite_region_size_x (LiteRegion *lr)
{
  return lr->region_size.x;
}

int
lite_region_size_y (LiteRegion *lr)
{
  return lr->region_size.y;
}

int
lite_region_size_z (LiteRegion *lr)
{
  return lr->region_size.z;
}

const char *
lite_region_name (LiteRegion *lr)
{
  return lr->name;
}

gint64
lite_region_create_time (LiteRegion *lr)
{
  return lr->create_time;
}

gint64
lite_region_modify_time (LiteRegion *lr)
{
  return lr->modify_time;
}

const char *
lite_region_description (LiteRegion *lr)
{
  return lr->description;
}

const char *
lite_region_author (LiteRegion *lr)
{
  return lr->author;
}

const DhNbtInstance &
lite_region_get_instance (LiteRegion *lr)
{
  return lr->region_nbt_instance_cpp;
}

uint64_t
lite_region_block_index (LiteRegion *lr, int x, int y, int z)
{
  if (x >= lr->region_size.x || y >= lr->region_size.y
      || z >= lr->region_size.z)
    g_error ("Coordination out of range.");
  else
    return lr->region_size.x * lr->region_size.z * y + lr->region_size.x * z
           + x;
}

/* The function below uses the implement from another project:
 * "litematica-tools" from KikuGie
 * https://github.com/Kikugie/litematica-tools
 * It uses MIT License, the license file could be found in config/
 * since files in config/ are also from this project.
 */

int
lite_region_block_id (LiteRegion *lr, uint64_t index)
{
  const int64_t *state = lr->states;
  int bits = lr->move_bits;
  uint64_t start_bit = index * bits;
  int start_state = start_bit / 64;
  int and_num = (1 << bits) - 1;
  int move_num = start_bit & 63;
  int end_num = start_bit % 64 + bits;
  int id = 0;
  if (end_num <= 64)
    id = (uint64_t)(state[start_state]) >> move_num & and_num;
  else
    {
      int move_num_2 = 64 - move_num;
      if (start_state + 1 >= lr->states_num)
        g_error ("Out of range!");
      id = ((uint64_t)state[start_state] >> move_num
            | (uint64_t)state[start_state + 1] << move_num_2)
           & and_num;
    }
  return id;
}

int
lite_region_block_id_xyz (LiteRegion *lr, int x, int y, int z)
{
  uint64_t index = lite_region_block_index (lr, x, y, z);
  return lite_region_block_id (lr, index);
}

DhStrArray *
lite_region_name_array_instance (void *instance)
{
  DhNbtInstance instance_dup (*(DhNbtInstance *)instance);
  instance_dup.child ("Regions");
  instance_dup.child ();
  DhStrArray *str_arr = NULL;
  while (instance_dup.is_non_null ())
    {
      dh_str_array_add_str (&str_arr, instance_dup.get_key ());
      instance_dup.next ();
    }
  return str_arr;
}

const gint64 *
lite_region_state (LiteRegion *lr, int *len)
{
  *len = lr->states_num;
  return lr->states;
}

DhStrArray *
lite_region_name_array_node (NbtNode *root)
{
  DhStrArray *str_array = NULL;
  NbtNode *region_node = nbt_node_child_to_key (root, "Regions");
  if (region_node)
    {
      NbtNode *region_child_node = region_node->children;
      while (region_child_node)
        {
          dh_str_array_add_str (&str_array,
                                nbt_node_get_key (region_child_node));
          region_child_node = region_child_node->next;
        }
    }
  return str_array;
}