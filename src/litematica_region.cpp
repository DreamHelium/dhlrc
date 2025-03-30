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
#include "nbt_interface/libnbt/nbt.h"
#include "nbt_interface_cpp/nbt_interface.hpp"
#include "region.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "translation.h"
#include "nbt_interface/nbt_if_common.h"

typedef struct _LiteRegion{

    /** Data Version */
    int data_version;

    /** Region name */
    const char* name;

    /** Number of the region */
    int region_num;

    /** Block names and nums */
    DhStrArray* blocks;

    /** Replaced name of blocks */
    // DhStrArray* replaced_blocks;

    /** Region NBT */
    // NBT* region_nbt;
    // NbtInstance* region_nbt_instance;
    DhNbtInstance region_nbt_instance_cpp;

    /** Block states */
    const int64_t* states;

    /** numbers of BlockStates */
    int states_num;

    struct{
        int x;
        int y;
        int z;
    } region_size;

    /** In many cases you don't need it, it's used to get block id. */
    int move_bits;

} _LiteRegion;

typedef struct TmpItem{
    gchar* name;
    guint total;
} TmpItem;

typedef GList TmpItemList;

static int tmpitem_strcmp(gconstpointer a, gconstpointer b)
{
    return strcmp( (char*)(((TmpItem*)a)->name) , (char*)b );
}

static void tmp_item_list_add_num(TmpItemList** til, char* item_name)
{
    TmpItemList* il = g_list_find_custom(*til, item_name, tmpitem_strcmp);
    if(il)
    {
        TmpItem* ti = (TmpItem*)(il->data);
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
    TmpItem* item = (TmpItem*)ti;
    g_free(item->name);
    g_free(item);
}

static void tmpitem_list_free(TmpItemList* til)
{
    g_list_free_full(til, tmpitem_free);
}

/* TODO: finish all
static NbtInstance* create_lr_instance(Region* region)
{
    NbtInstance* region_name = dh_nbt_instance_new_compound("");

    int len = 0;
    gint64* states = region_get_palette_num_from_region(region, &len);
    NbtInstance* block_states = dh_nbt_instance_new_long_array(states, len, "BlockStates");
    free(states);
    dh_nbt_instance_insert_before(region_name, NULL, block_states);
    dh_nbt_instance_free_only_instance(block_states);

    NbtInstance* pending_block_ticks = dh_nbt_instance_new_list("PendingBlockTicks");
    dh_nbt_instance_insert_before(region_name, NULL, pending_block_ticks);

    NbtInstance* pos_x = dh_nbt_instance_new_int(0, "x");
    NbtInstance* pos_y = dh_nbt_instance_new_int(0, "y");
    NbtInstance* pos_z = dh_nbt_instance_new_int(0, "z");

    NbtInstance* pos = dh_nbt_instance_new_compound("Position");
    dh_nbt_instance_insert_before(pos, NULL, pos_x);
    dh_nbt_instance_insert_before(pos, NULL, pos_y);
    dh_nbt_instance_insert_before(pos, NULL, pos_z);

    dh_nbt_instance_insert_before(region_name, NULL, pos);
    dh_nbt_instance_free_only_instance(pos);
    dh_nbt_instance_free_only_instance(pos_x);
    dh_nbt_instance_free_only_instance(pos_y);
    dh_nbt_instance_free_only_instance(pos_z);

    return region_name;
}

LiteRegion* lite_region_create_from_region(void* p)
{
    Region* region = (Region*)p;
    LiteRegion* out = g_new0(LiteRegion, 1);

    NbtInstance* instance = create_lr_instance(region);
}
*/

static DhStrArray* get_blocks(DhNbtInstance region)
{
    DhStrArray* arr = nullptr;
    region.child("BlockStatePalette");
    region.child();
    for(; region.is_non_null() ; region.next())
    {
        auto internal_region(region);
        internal_region.child("Name");
        dh_str_array_add_str(&arr, internal_region.get_string());
    }
    return arr;
}

LiteRegion* lite_region_create_from_root_instance_cpp(DhNbtInstance root, int r_num)
{
    LiteRegion* out = g_new0(LiteRegion, 1);

    auto data_version(root);
    data_version.goto_root();
    data_version.child("MinecraftDataVersion");
    out->data_version = data_version.get_int();

    auto region(root);
    region.goto_root();
    region.child("Regions");
    region.child();
    for(int i = 0 ; i < r_num ; i++)
    {
        if(!region.next())
        {
            g_free(out);
            return nullptr;
            printf("%s", region.get_key());
        }
    }

    out->name = region.get_key();
    out->region_num = r_num;
    out->region_nbt_instance_cpp = region;
    auto region_dup(region);

    out->blocks = get_blocks(region_dup);
    auto bs(region_dup);
    bs.child("BlockStates");
    out->states = bs.get_long_array(out->states_num);

    auto rsize(region_dup);
    rsize.child("Size");
    auto get_pos = [](DhNbtInstance instance, const char* str)
    {
        instance.child(str);
        return ABS(instance.get_int());
    };
    out->region_size.x = get_pos(rsize, "x");
    out->region_size.y = get_pos(rsize, "y");
    out->region_size.z = get_pos(rsize, "z");

    int bits = g_bit_storage(out->blocks->num);
    bits = bits <= 2 ? 2 : bits;
    out->move_bits = bits;

    return out;
}

LiteRegion* lite_region_create(NBT* root, int r_num)
{
    DhNbtInstance instance(root, true);
    LiteRegion* ret = lite_region_create_from_root_instance_cpp(instance, r_num);
    return ret;
}

void lite_region_free(LiteRegion* lr)
{
    dh_str_array_free(lr->blocks);
    g_free(lr);
}

int lite_region_num(NBT* root)
{
    NBT* regionParent = NBT_GetChild_Deep(root,"Regions",NULL);
    if(regionParent && regionParent->child)
    {
        NBT* regionName = regionParent -> child;

        int i = 1;
        for( ; regionName->next; i++)
        {
            regionName = regionName->next;
        }
        return i;
    }
    else
    {
        return 0;
    }
}

char** lite_region_names(NBT* root, int rNum, int* err)
{
    NBT* regionParent = NBT_GetChild(root,"Regions");
    char** region = (char**)malloc(rNum * sizeof(char*));
    int i = 0;
    NBT* regionName = regionParent -> child;
    if(regionName)
    {
        for(i = 0 ; i < rNum ; i++)
        {
            if(regionName)
            {
                int len = strlen(regionName->key) + 1;
                region[i] = (char*)malloc(len * sizeof(char));
                //region[i] = (regionName -> key);
                strcpy(region[i],regionName->key);
                //printf("%s \n",region[i]);
            }
            else
            {
                lite_region_free_names(region,i);
                *err = -2;
                return NULL;
            }
             regionName = regionName -> next;
        }
    }
    else
    {
        free(region);
        *err = -1;
        return NULL;
    }
    *err =  0;
    return region;
}

void lite_region_free_names(char** region,int rNum)
{
    for(int i = 0; i < rNum ; i++)
        free(region[i]);
    free(region);
    region = NULL;
}

DhStrArray* lite_region_block_name_array(LiteRegion* lr)
{
    return lr->blocks;
}

int lite_region_data_version(LiteRegion* lr)
{
    return lr->data_version;
}

int lite_region_size_x(LiteRegion* lr)
{
    return lr->region_size.x;
}

int lite_region_size_y(LiteRegion* lr)
{
    return lr->region_size.y;
}

int lite_region_size_z(LiteRegion* lr)
{
    return lr->region_size.z;
}

const char*   lite_region_name(LiteRegion* lr)
{
    return lr->name;
}

DhNbtInstance lite_region_get_instance(LiteRegion *lr)
{
    return lr->region_nbt_instance_cpp;
}

uint64_t lite_region_block_index(LiteRegion* lr, int x, int y, int z)
{
    if( x >= lr->region_size.x || y >= lr->region_size.y || z >= lr->region_size.z )
        g_error("Coordination out of range.");
    else
        return lr->region_size.x * lr->region_size.z * y + lr->region_size.x * z + x;
}

/* The function below uses the implement from another project:
 * "litematica-tools" from KikuGie
 * https://github.com/Kikugie/litematica-tools
 * It uses MIT License, the license file could be found in config/
 * since files in config/ are also from this project.
 */

int lite_region_block_id(LiteRegion* lr, uint64_t index)
{
    const int64_t* state = lr->states;
    int bits = lr->move_bits;
    uint64_t start_bit = index * bits;
    int start_state = start_bit / 64;
    int and_num = (1 << bits) - 1;
    int move_num = start_bit & 63;
    int end_num = start_bit % 64 + bits;
    int id = 0;
    if(end_num <= 64)
        id = (uint64_t)(state[start_state]) >> move_num & and_num;
    else
    {
        int move_num_2 = 64 - move_num;
        if( start_state + 1 >= lr->states_num)
            g_error("Out of range!");
        id = ((uint64_t)state[start_state] >> move_num | state[start_state + 1] << move_num_2)& and_num;
    }
    return id;
}

int lite_region_block_id_xyz(LiteRegion* lr, int x, int y, int z)
{
    uint64_t index = lite_region_block_index(lr, x, y, z);
    return lite_region_block_id(lr, index);
}

// ItemList *lite_region_item_list(NBT* root, int r_num)
// {
//     return lite_region_item_list_extend(root,r_num,NULL, 0);
// }

// ItemList *lite_region_item_list_extend(NBT* root, int r_num, ItemList* oBlock, int print_process)
// {
//     clock_t start = clock();
//     LiteRegion* lr = lite_region_create(root, r_num);
//     int block_num = lr->blocks->num;

//     // First, read originBlockName and compare it to oBlock, add Blocks to it

//     /* If no blocks there's no need to add items */
//     if(block_num == 0)
//     {
//         lite_region_free(lr);
//         return oBlock;
//     }

//     TmpItemList* til = NULL;

//     // Second, read BlockStates and add number to oBlock.num

//     BlackList* bl = black_list_init();

//     //char process[] = "-\\|/";
//     uint64_t volume = lr->region_size.x * lr->region_size.y * lr->region_size.z;
//     for(int y = 0 ; y < lr->region_size.y ; y++)
//     {
//         for(int z = 0 ; z < lr->region_size.z ; z++)
//         {
//             for(int x = 0 ; x < lr->region_size.x ; x++)
//             {
//                 uint64_t index = lite_region_block_index(lr,x,y,z);
//                 int id = lite_region_block_id(lr,index);
//                 char* id_block_name = lr->replaced_blocks->val[id];
//                 int passed_ms = (double)(1000.0f * (clock() - start) / CLOCKS_PER_SEC);
//                 if((print_process && (passed_ms % 500 == 0)) || (index + 1) == volume ){
//                     float percent = ((float)(index + 1) / volume) * 100;
//                     fprintf(stderr,_("[%.2f%%] Processing Blocks %lu/%lu, (%3d,%3d,%3d)/(%3d,%3d,%3d)"), percent ,index+1, volume ,
//                             x,y,z,lr->region_size.x,lr->region_size.y,lr->region_size.z);
//                     fprintf(stderr, "\r");

//                 }
//                 // if(verbose_level == 3)
//                 //         g_message(_("Processing Blocks %lu/%lu, (%3d,%3d,%3d)/(%3d,%3d,%3d)"" %s"), index+1, volume ,
//                 //             x,y,z,lr->region_size.x,lr->region_size.y,lr->region_size.z, trm(id_block_name));
//                 if(!black_list_scan(bl,id_block_name))
//                 {
//                     if(lite_region_block_properties_equal(lr, id, "waterlogged", "true"))
//                     {
//                         tmp_item_list_add_num(&til, "minecraft:water_bucket");
//                         // item_list_add_num(&oBlock,1,"minecraft:water_bucket");
//                     }
//                     if(!strcmp(id_block_name,"minecraft:water_bucket") ||
//                       !strcmp(id_block_name,"minecraft:lava_bucket"))
//                     {
//                         if(!lite_region_block_properties_equal(lr, id, "level", "0"))
//                             continue;    // It's not source, so skip
//                     }
//                     if(strstr(id_block_name,"_slab"))     // special for slab
//                     {
//                         if(lite_region_block_properties_equal(lr,id,"type","double"))
//                         {
//                             // item_list_add_num(&oBlock,2,id_block_name);
//                             /* Add once then once */
//                             tmp_item_list_add_num(&til, id_block_name);
//                             // continue;
//                         }
//                     }
//                     if(strstr(id_block_name,"_door"))
//                     {
//                         if(!lite_region_block_properties_equal(lr, id, "half" ,"upper"))
//                         {
//                             if(!lite_region_block_properties_equal(lr,
//                                 lite_region_block_id_xyz(lr,x,y-1,z)
//                                 ,"half","lower"))
//                                 continue;
//                         }
//                     }
//                     tmp_item_list_add_num(&til, id_block_name);
//                     // item_list_add_num(&oBlock,1,id_block_name);
//                 }

//             }
//         }
//     }
//     printf("\n");
//     gchar* description = g_strdup_printf(_("Add %%d items from region %s"), lr->name );
//     black_list_free(bl);
//     lite_region_free(lr);

    

//     /* Copy items to the ItemList */
//     for(TmpItemList* tild = til; tild ; tild = tild->next)
//     {
//         TmpItem* data = (TmpItem*)(tild->data);
//         oBlock = item_list_add_item(&oBlock, data->total, data->name, description);
//     }
//     g_free(description);
//     tmpitem_list_free(til);
//     return oBlock;
// }

// ItemList *lite_region_item_list_without_num(LiteRegion* lr, ItemList *o_il)
// {
//     BlackList* bl = black_list_init();
//     /* Scan block lists and add blocks to itemlist */
//     for(int i = 0 ; i < lr->blocks->num ; i++)
//     {
//         char* r_block_name = lr->replaced_blocks->val[i];
//         if( !black_list_scan(bl, r_block_name) && !item_list_scan_repeated(o_il, r_block_name) )
//         {
//             if(item_list_init_new_item(&o_il, r_block_name))
//             {
//                 black_list_free(bl);
//                 return NULL;
//             }
//         }
//     }
//     if(!item_list_scan_repeated(o_il, "minecraft:water_bucket"))
//         item_list_init_new_item(&o_il, "minecraft:water_bucket");
//     black_list_free(bl);
//     return o_il;
// }

DhStrArray* lite_region_name_array(NBT* root)
{
    NBT* region_nbt = NBT_GetChild(root, "Regions")->child;
    DhStrArray* str_arr = NULL;
    while(region_nbt)
    {
        dh_str_array_add_str( &str_arr, region_nbt->key);
        region_nbt = region_nbt->next;
    }
    return str_arr;
}

// gboolean lite_region_block_properties_equal(LiteRegion* lr, int id, char* key, char* val)
// {
//     NbtInstance* current = (lr->block_properties)[id];
//     NbtInstance* current_copy = dh_nbt_instance_dup(current);
//     gboolean ret = FALSE;
//     for(; dh_nbt_instance_is_non_null(current_copy) ; dh_nbt_instance_next(current_copy))
//     {
//         const char* str = dh_nbt_instance_get_string(current_copy);
//         if( str && g_str_equal(dh_nbt_instance_get_key(current_copy), key))
//         {   /* This item */
//             if(g_str_equal(str, val))
//                 ret = TRUE;
//             ret = FALSE;
//             free((void*)str);
//             dh_nbt_instance_free(current_copy);
//             return ret;
//         }
//     }
//     dh_nbt_instance_free(current_copy);
//     return ret;
// }