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
#include "dhlrc_list.h"
#include "nbt_interface/libnbt/nbt.h"
#include "region.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "nbt_interface/nbt_interface.h"
#include "translation.h"
#include "nbt_interface/nbt_if_common.h"

static DhStrArray* block_name_array(NBT* root, int r_num);
static NBT* lite_region_nbt_region(NBT* root,int r_num);
static NbtInstance* lite_region_nbt_region_instance(NbtInstance* instance, int r);
static DhStrArray* block_name_array_instance(NbtInstance* instance);
static NbtInstance* nbt_block_properties(LiteRegion* lr, int id);
static int* size_array(NBT* root,int r_num);
static int* size_array_instance(NbtInstance* instance);

typedef struct _LiteRegion{

    /** Data Version */
    int data_version;

    /** Region name */
    char* name;

    /** Number of the region */
    int region_num;

    /** Block names and nums */
    DhStrArray* blocks;

    /** Replaced name of blocks */
    DhStrArray* replaced_blocks;

    /** Region NBT */
    // NBT* region_nbt;
    NbtInstance* region_nbt_instance;

    /** Block Properties */
    NbtInstance** block_properties;

    /** Block states */
    int64_t* states;

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

LiteRegion* lite_region_create_instance(NbtInstance* instance, int r_num)
{
    LiteRegion* out = g_new0(LiteRegion, 1);

    NbtInstance* data_version = dh_nbt_instance_dup(instance);
    dh_nbt_instance_goto_root(data_version);
    dh_nbt_instance_child_to_node(data_version, "MinecraftDataVersion");
    out->data_version = dh_nbt_instance_get_int(data_version);
    dh_nbt_instance_free(data_version);

    DhStrArray* r_name = lite_region_name_array_instance(instance);
    if(r_num < r_name->num)
    {
        out->name = dh_strdup( r_name->val[r_num] );
        dh_str_array_free(r_name);

        out->region_num = r_num;
        out->region_nbt_instance = lite_region_nbt_region_instance(instance, r_num);

        out->blocks = block_name_array_instance(out->region_nbt_instance);

        out->replaced_blocks = NULL;

        NbtInstance** properties = g_new0(NbtInstance*, out->blocks->num);
        if(properties)
        {
            for(int i = 0 ; i < out->blocks->num ; i++)
                properties[i] = nbt_block_properties(out, i);
        }

        out->block_properties = properties;
        int* size = size_array_instance(out->region_nbt_instance);
        out->region_size.x = size[0];
        out->region_size.y = size[1];
        out->region_size.z = size[2];
        g_free(size);

        NbtInstance* states = dh_nbt_instance_dup(out->region_nbt_instance);
        dh_nbt_instance_child_to_node(states, "BlockStates");
        int len = 0;
        int64_t* state_val = (int64_t*)dh_nbt_instance_get_long_array(states, &len);
        
        out->states = state_val;
        out->states_num = len;

        dh_nbt_instance_free(states);

        /* Try to get move bits */
        int bits = g_bit_storage(out->blocks->num);
        bits = bits <= 2 ? 2 : bits;
        out->move_bits = bits;

        /* Replace name of block here so you don't have to do it in the following step */
        DhStrArray* replaced_names = NULL;
        ReplaceList* rl = replace_list_init();
        for(int i = 0 ; i < out->blocks->num ; i++)
        {
            dh_str_array_add_str(&replaced_names, replace_list_replace(rl, out->blocks->val[i]));
        }
        replace_list_free(rl);
        out->replaced_blocks = replaced_names;

        return out;
    }
    else
    {
        dh_str_array_free(r_name);
        g_free(out);
        return NULL;
    }
}

LiteRegion* lite_region_create(NBT* root, int r_num)
{
    NbtInstance* instance = dh_nbt_instance_new_from_real_nbt((RealNbt*)root);
    LiteRegion* ret = lite_region_create_instance(instance, r_num);
    dh_nbt_instance_free(instance);
    return ret;
}

void lite_region_free(LiteRegion* lr)
{
    for(int i = 0 ; i < lr->blocks->num ; i++)
        dh_nbt_instance_free(lr->block_properties[i]);
    free(lr->name);
    dh_str_array_free(lr->blocks);
    dh_str_array_free(lr->replaced_blocks);
    dh_nbt_instance_free_only_instance(lr->region_nbt_instance);
    g_free(lr->block_properties);
    g_free(lr->states);
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

int lite_region_num_instance(NbtInstance* instance)
{
    NbtInstance* new_instance = dh_nbt_instance_dup(instance);
    dh_nbt_instance_goto_root(new_instance);
    dh_nbt_instance_child_to_node(new_instance, "Regions");
    int ret = 0;
    if(dh_nbt_instance_is_non_null(new_instance))
    {
        if(dh_nbt_instance_child(new_instance))
        {
            for(; dh_nbt_instance_is_non_null(new_instance) ; dh_nbt_instance_next(new_instance))
                ret++;
        }
    }
    dh_nbt_instance_free(new_instance);
    return ret;
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

static NBT* lite_region_nbt_region(NBT* root, int r_num)
{
    NBT* OutRegion = NBT_GetChild(root,"Regions")->child; //region 0
    for(int i = 0; i < r_num ; i++)
    {
        if(OutRegion->next)
            OutRegion = OutRegion->next;
        else
            return NULL;
    }
    return OutRegion;
}

static NbtInstance* lite_region_nbt_region_instance(NbtInstance* instance, int r)
{
    NbtInstance* new_instance = dh_nbt_instance_dup(instance);
    dh_nbt_instance_goto_root(new_instance);
    dh_nbt_instance_child_to_node(new_instance, "Regions");
    dh_nbt_instance_child(new_instance);
    for(int i = 0 ; i < r ; i++)
    {
        if(dh_nbt_instance_next(new_instance)) ;
        else return NULL;
    }
    return new_instance;
}

NBT* lite_region_nbt_block_state_palette(NBT* root, int r_num)
{
    NBT* region_nbt = lite_region_nbt_region(root, r_num);
    if(region_nbt)
        return NBT_GetChild(region_nbt , "BlockStatePalette") -> child;
    else return NULL;
}

int lite_region_block_num(NBT* root, int r_num)
{
    NBT* palette = lite_region_nbt_block_state_palette(root,r_num);
    if(palette)
    {
        int i = 0;
        for( ; palette ; i++)
            palette = palette -> next;
        return i;
    }
    else
        return 0;
}

char** lite_region_block_names(NBT* root, int r_num ,int bNum)
{
    NBT* palette = lite_region_nbt_block_state_palette(root,r_num);
    if(bNum == 0) return NULL;
    char** l = (char**)malloc(bNum * sizeof(char*));
    int i = 0;
    if(palette)
    {
        NBT* pName;
        for(i = 0 ; i < bNum ; i++)
        {
            pName = NBT_GetChild(palette,"Name");
            if(pName)
            {
                int len = pName->value_a.len;
                l[i] = (char*)malloc(len * sizeof(char));
                //region[i] = (regionName -> key);
                strcpy(l[i],pName->value_a.value);
                //printf("%s \n",region[i]);
            }
            else
            {
                lite_region_free_names(l,i);
                return NULL;
            }

             palette = palette -> next;
        }
    }
    else
    {
        free(l);
        return NULL;
    }
    return l;
}

static DhStrArray* block_name_array(NBT* root, int r_num)
{
    NBT* palette = lite_region_nbt_block_state_palette(root, r_num);
    DhStrArray* name = NULL;
    while(palette)
    {
        NBT* block_nbt = NBT_GetChild( palette, "Name" );
        if(block_nbt)
            dh_str_array_add_str( &name, block_nbt->value_a.value );
        else
        {
            dh_str_array_free(name);
            return NULL;
        }
        palette = palette -> next;
    }
    return name;
}

static DhStrArray* block_name_array_instance(NbtInstance* instance)
{
    NbtInstance* new_instance = dh_nbt_instance_dup(instance);
    dh_nbt_instance_child_to_node(new_instance, "BlockStatePalette");
    dh_nbt_instance_child(new_instance);
    DhStrArray* arr = NULL;
    for(; dh_nbt_instance_is_non_null(new_instance) ; dh_nbt_instance_next(new_instance))
    {
        NbtInstance* block_nbt = dh_nbt_instance_dup(new_instance);
        dh_nbt_instance_child_to_node(block_nbt, "Name");
        if(dh_nbt_instance_is_non_null(block_nbt))
        {
            const char* str = dh_nbt_instance_get_string(block_nbt);
            dh_str_array_add_str(&arr, str);
            free((void*)str);
        }
        /* else free? */
        dh_nbt_instance_free(block_nbt);
    }
    dh_nbt_instance_free(new_instance);
    return arr;
}

DhStrArray* lite_region_block_name_array(LiteRegion* lr)
{
    return lr->blocks;
}

NbtInstance** lite_region_block_properties(LiteRegion* lr)
{
    return lr->block_properties;
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

NbtInstance* lite_region_region_instance(LiteRegion* lr)
{
    return lr->region_nbt_instance;
}

uint64_t* lite_region_block_states_array(NBT* root, int r_num, int* len)
{
    NBT* state = NBT_GetChild(lite_region_nbt_region(root,r_num),"BlockStates");
    if(len)
        *len = state->value_a.len;
    return (uint64_t*)state->value_a.value;
}

static int* size_array(NBT* root,int r_num)
{
    NBT* size_state = NBT_GetChild(lite_region_nbt_region(root,r_num),"Size");
    int* a = malloc(3*sizeof(int));
    int x = NBT_GetChild(size_state,"x")->value_i;
    int y = NBT_GetChild(size_state,"y")->value_i;
    int z = NBT_GetChild(size_state,"z")->value_i;
    a[0] = ABS(x);
    a[1] = ABS(y);
    a[2] = ABS(z);
    return a;
}

static int* size_array_instance(NbtInstance* instance)
{
    NbtInstance* size_state = dh_nbt_instance_dup(instance);
    dh_nbt_instance_child_to_node(size_state, "Size");
    int* ret = g_new0(int, 3);

    dh_nbt_instance_child_to_node(size_state, "x");
    int x = ABS(dh_nbt_instance_get_int(size_state));
    dh_nbt_instance_parent(size_state);

    dh_nbt_instance_child_to_node(size_state, "y");
    int y = ABS(dh_nbt_instance_get_int(size_state));
    dh_nbt_instance_parent(size_state);

    dh_nbt_instance_child_to_node(size_state, "z");
    int z = ABS(dh_nbt_instance_get_int(size_state));

    dh_nbt_instance_free(size_state);
    ret[0] = x;
    ret[1] = y;
    ret[2] = z;
    return ret;
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
    int64_t* state = lr->states;
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

NBT* lite_region_nbt_specific_block_state_palette(NBT* root, int r_num, int id)
{
    NBT* a = lite_region_nbt_block_state_palette(root,r_num);
    if(a)
        for(int i = 0; i < id; i++)
            a = a->next;
    return a;
}

char* lite_region_block_type(NBT* root, int r_num, int id)
{
    return (char*)NBT_GetChild_Deep(lite_region_nbt_specific_block_state_palette(root,r_num,id),"Properties","type",NULL)->value_a.value;
}

ItemList *lite_region_item_list(NBT* root, int r_num)
{
    return lite_region_item_list_extend(root,r_num,NULL, 0);
}

ItemList *lite_region_item_list_extend(NBT* root, int r_num, ItemList* oBlock, int print_process)
{
    clock_t start = clock();
    LiteRegion* lr = lite_region_create(root, r_num);
    int block_num = lr->blocks->num;

    // First, read originBlockName and compare it to oBlock, add Blocks to it

    /* If no blocks there's no need to add items */
    if(block_num == 0)
    {
        lite_region_free(lr);
        return oBlock;
    }

    TmpItemList* til = NULL;

    // Second, read BlockStates and add number to oBlock.num

    BlackList* bl = black_list_init();

    //char process[] = "-\\|/";
    uint64_t volume = lr->region_size.x * lr->region_size.y * lr->region_size.z;
    for(int y = 0 ; y < lr->region_size.y ; y++)
    {
        for(int z = 0 ; z < lr->region_size.z ; z++)
        {
            for(int x = 0 ; x < lr->region_size.x ; x++)
            {
                uint64_t index = lite_region_block_index(lr,x,y,z);
                int id = lite_region_block_id(lr,index);
                char* id_block_name = lr->replaced_blocks->val[id];
                int passed_ms = (double)(1000.0f * (clock() - start) / CLOCKS_PER_SEC);
                if((print_process && (passed_ms % 500 == 0)) || (index + 1) == volume ){
                    float percent = ((float)(index + 1) / volume) * 100;
                    fprintf(stderr,_("[%.2f%%] Processing Blocks %lu/%lu, (%3d,%3d,%3d)/(%3d,%3d,%3d)"), percent ,index+1, volume ,
                            x,y,z,lr->region_size.x,lr->region_size.y,lr->region_size.z);
                    fprintf(stderr, "\r");

                }
                // if(verbose_level == 3)
                //         g_message(_("Processing Blocks %lu/%lu, (%3d,%3d,%3d)/(%3d,%3d,%3d)"" %s"), index+1, volume ,
                //             x,y,z,lr->region_size.x,lr->region_size.y,lr->region_size.z, trm(id_block_name));
                if(!black_list_scan(bl,id_block_name))
                {
                    if(lite_region_block_properties_equal(lr, id, "waterlogged", "true"))
                    {
                        tmp_item_list_add_num(&til, "minecraft:water_bucket");
                        // item_list_add_num(&oBlock,1,"minecraft:water_bucket");
                    }
                    if(!strcmp(id_block_name,"minecraft:water_bucket") ||
                      !strcmp(id_block_name,"minecraft:lava_bucket"))
                    {
                        if(!lite_region_block_properties_equal(lr, id, "level", "0"))
                            continue;    // It's not source, so skip
                    }
                    if(strstr(id_block_name,"_slab"))     // special for slab
                    {
                        if(lite_region_block_properties_equal(lr,id,"type","double"))
                        {
                            // item_list_add_num(&oBlock,2,id_block_name);
                            /* Add once then once */
                            tmp_item_list_add_num(&til, id_block_name);
                            // continue;
                        }
                    }
                    if(strstr(id_block_name,"_door"))
                    {
                        if(!lite_region_block_properties_equal(lr, id, "half" ,"upper"))
                        {
                            if(!lite_region_block_properties_equal(lr,
                                lite_region_block_id_xyz(lr,x,y-1,z)
                                ,"half","lower"))
                                continue;
                        }
                    }
                    tmp_item_list_add_num(&til, id_block_name);
                    // item_list_add_num(&oBlock,1,id_block_name);
                }

            }
        }
    }
    printf("\n");
    gchar* description = g_strdup_printf(_("Add %%d items from region %s"), lr->name );
    black_list_free(bl);
    lite_region_free(lr);

    

    /* Copy items to the ItemList */
    for(TmpItemList* tild = til; tild ; tild = tild->next)
    {
        TmpItem* data = tild->data;
        oBlock = item_list_add_item(&oBlock, data->total, data->name, description);
    }
    g_free(description);
    tmpitem_list_free(til);
    return oBlock;
}

ItemList *lite_region_item_list_without_num(LiteRegion* lr, ItemList *o_il)
{
    BlackList* bl = black_list_init();
    /* Scan block lists and add blocks to itemlist */
    for(int i = 0 ; i < lr->blocks->num ; i++)
    {
        char* r_block_name = lr->replaced_blocks->val[i];
        if( !black_list_scan(bl, r_block_name) && !item_list_scan_repeated(o_il, r_block_name) )
        {
            if(item_list_init_new_item(&o_il, r_block_name))
            {
                black_list_free(bl);
                return NULL;
            }
        }
    }
    if(!item_list_scan_repeated(o_il, "minecraft:water_bucket"))
        item_list_init_new_item(&o_il, "minecraft:water_bucket");
    black_list_free(bl);
    return o_il;
}

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

DhStrArray* lite_region_name_array_instance(NbtInstance* instance)
{
    NbtInstance* new_instance = dh_nbt_instance_dup(instance);
    dh_nbt_instance_goto_root(new_instance);
    dh_nbt_instance_child_to_node(new_instance, "Regions");
    dh_nbt_instance_child(new_instance);
    DhStrArray* arr = NULL;
    for(; dh_nbt_instance_is_non_null(new_instance) ; dh_nbt_instance_next(new_instance))
        dh_str_array_add_str(&arr, dh_nbt_instance_get_key(new_instance));
    dh_nbt_instance_free(new_instance);
    return arr;
}

static NbtInstance* nbt_block_properties(LiteRegion* lr, int id)
{
    NbtInstance* region_nbt_instance = lr->region_nbt_instance;
    NbtInstance* region_nbt_copy = dh_nbt_instance_dup(region_nbt_instance);
    dh_nbt_instance_child_to_node(region_nbt_copy, "BlockStatePalette");
    dh_nbt_instance_child(region_nbt_copy);
    for(int i = 0 ; i < id ; i++)
        dh_nbt_instance_next(region_nbt_copy);
    if(dh_nbt_instance_child_to_node(region_nbt_copy, "Properties"))
    {
        dh_nbt_instance_child(region_nbt_copy);
        return region_nbt_copy;
    }
    else
    {
        dh_nbt_instance_free(region_nbt_copy);
        return dh_nbt_instance_new_from_real_nbt(NULL);
    }
}

gboolean lite_region_block_properties_equal(LiteRegion* lr, int id, char* key, char* val)
{
    NbtInstance* current = (lr->block_properties)[id];
    NbtInstance* current_copy = dh_nbt_instance_dup(current);
    gboolean ret = FALSE;
    for(; dh_nbt_instance_is_non_null(current_copy) ; dh_nbt_instance_next(current_copy))
    {
        const char* str = dh_nbt_instance_get_string(current_copy);
        if( str && g_str_equal(dh_nbt_instance_get_key(current_copy), key))
        {   /* This item */
            if(g_str_equal(str, val))
                ret = TRUE;
            ret = FALSE;
            free((void*)str);
            dh_nbt_instance_free(current_copy);
            return ret;
        }
    }
    dh_nbt_instance_free(current_copy);
    return ret;
}

