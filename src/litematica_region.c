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
#include "dhlrc_list.h"
#include "libnbt/nbt.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "translation.h"

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

LiteRegion* lite_region_create(NBT* root, int r_num)
{
    LiteRegion* out = (LiteRegion*)malloc(sizeof(LiteRegion));
    if(out)
    {
        NbtPos* pos = nbt_pos_init(root);
        if(pos)
        {
            nbt_pos_get_child(pos, "Regions");
            nbt_pos_add_to_tree(pos, r_num);
            out->region_pos = pos;
        }
        else
        {
            free(out);
            return NULL;
        }
        
        NBT* data_version = NBT_GetChild(root, "MinecraftDataVersion");
        out->data_version = data_version->value_i;

        DhStrArray* r_name = lite_region_name_array(root);
        if(r_num < r_name->num)
        {
            out->name = dh_strdup( r_name->val[r_num] );
            dh_str_array_free(r_name);

            out->region_num = r_num;
            out->region_nbt = lite_region_nbt_region( root, r_num );

            out->blocks = lite_region_block_name_array( root, r_num );

            out->replaced_blocks = NULL;

            NBT** properties = (NBT**)malloc( out->blocks->num * sizeof(NBT*));
            if(properties)
            {
                for(int i = 0 ; i < out->blocks->num ; i++)
                    properties[i] = lite_region_nbt_block_properties(out, i);
            }
            else{
                lite_region_free(out);
                return NULL;
            }

            out->block_properties = properties;
            int* size = lite_region_size_array(root, r_num);
            out->region_size.x = size[0];
            out->region_size.y = size[1];
            out->region_size.z = size[2];
            free(size);

            NBT* states = NBT_GetChild(out->region_nbt, "BlockStates");
            out->states = states->value_a.value;
            out->states_num = states->value_a.len;

            /* Try to get move bits */
            int bits = 0;
            while(out->blocks->num > (1 << bits))
                bits++;
            if(bits <= 2) bits = 2;
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
            free(out);
            return NULL;
        }
    }
    else return NULL;
}

void lite_region_free(LiteRegion* lr)
{
    free(lr->name);
    dh_str_array_free(lr->blocks);
    dh_str_array_free(lr->replaced_blocks);
    nbt_pos_free(lr->region_pos);
    free(lr->block_properties);
    free(lr);
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

NBT* lite_region_nbt_region(NBT* root, int r_num)
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

DhStrArray* lite_region_block_name_array(NBT* root, int r_num)
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

uint64_t* lite_region_block_states_array(NBT* root, int r_num, int* len)
{
    NBT* state = NBT_GetChild(lite_region_nbt_region(root,r_num),"BlockStates");
    if(len)
        *len = state->value_a.len;
    return (uint64_t*)state->value_a.value;
}

int* lite_region_size_array(NBT* root,int r_num)
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

NBT * lite_region_nbt_block_properties(LiteRegion* lr, int id)
{
    NbtPos* pos_copy = nbt_pos_copy(lr->region_pos);
    if(pos_copy)
    {
        nbt_pos_get_child( pos_copy, "BlockStatePalette" );
        nbt_pos_add_to_tree( pos_copy, id);
        if(nbt_pos_get_child( pos_copy, "Properties" )){
            NBT* ret = pos_copy->current;
            nbt_pos_free(pos_copy);
            return ret;
        }
        else{
            nbt_pos_free(pos_copy);
            return NULL;
        }
    }
    else return NULL;
}

gboolean lite_region_block_properties_equal(LiteRegion* lr, int id, char* key, char* val)
{
    NBT* current = (lr->block_properties)[id];
    while(current)
    {
        if(current->key && current->value_a.value
            && g_str_equal(current->key, key))
        {   /* This item */
            if(g_str_equal(current->value_a.value, val))
                return TRUE;
            return FALSE;
        }
        current = current -> next;
    }
    return FALSE;
}

