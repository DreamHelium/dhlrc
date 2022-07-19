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
#include "libnbt/nbt.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#ifndef DH_DISABLE_TRANSLATION
#include <libintl.h>
#define _(str) gettext (str)
#else
#define _(str) str
#endif

LiteRegion* LiteRegion_Create(NBT* root, int r_num)
{
    LiteRegion* out = (LiteRegion*)malloc(sizeof(LiteRegion));
    if(out)
    {
        dh_StrArray* r_name = lite_region_Name_StrArray(root);
        if(r_num < r_name->num)
        {
            out->name = String_Copy( r_name->val[r_num] );
            dh_StrArray_Free(r_name);
            out->region_num = r_num;
            out->region_nbt = lite_region_RegionNBT( root, r_num );
            out->blocks = lite_region_BlockName_StrArray( root, r_num );
            int* size = lite_region_SizeArray(root, r_num);
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
            dh_StrArray* replaced_names = NULL;
            ReplaceList* rl = ReplaceList_Init();
            for(int i = 0 ; i < out->blocks->num ; i++)
            {

                dh_StrArray_AddStr(&replaced_names, ReplaceList_Replace(rl, out->blocks->val[i]));

            }
            ReplaceList_Free(rl);
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

void LiteRegion_Free(LiteRegion* lr)
{
    free(lr->name);
    dh_StrArray_Free(lr->blocks);
    dh_StrArray_Free(lr->replaced_blocks);
    free(lr);
}

int lite_region_Num(NBT* root)
{
    NBT* regionParent = NBT_GetChild_Deep(root,"Regions",NULL);
    if(regionParent->child)
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

char** lite_region_Name(NBT* root, int rNum, int* err)
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
                lite_region_FreeNameArray(region,i);
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

void lite_region_FreeNameArray(char** region,int rNum)
{
    for(int i = 0; i < rNum ; i++)
        free(region[i]);
    free(region);
    region = NULL;
}

NBT* lite_region_RegionNBT(NBT* root, int r_num)
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

NBT* lite_region_BlockStatePalette(NBT* root, int r_num)
{
    NBT* region_nbt = lite_region_RegionNBT(root, r_num);
    if(region_nbt)
        return NBT_GetChild(region_nbt , "BlockStatePalette") -> child;
    else return NULL;
}

int lite_region_BlockNum(NBT* root, int r_num)
{
    NBT* palette = lite_region_BlockStatePalette(root,r_num);
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

char** lite_region_BlockNameArray(NBT* root, int r_num ,int bNum)
{
    NBT* palette = lite_region_BlockStatePalette(root,r_num);
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
                lite_region_FreeNameArray(l,i);
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

dh_StrArray* lite_region_BlockName_StrArray(NBT* root, int r_num)
{
    NBT* palette = lite_region_BlockStatePalette(root, r_num);
    dh_StrArray* name = NULL;
    while(palette)
    {
        NBT* block_nbt = NBT_GetChild( palette, "Name" );
        if(block_nbt)
            dh_StrArray_AddStr( &name, block_nbt->value_a.value );
        else
        {
            dh_StrArray_Free(name);
            return NULL;
        }
        palette = palette -> next;
    }
    return name;

}

uint64_t* lite_region_BlockStatesArray(NBT* root, int r_num, int* len)
{
    NBT* state = NBT_GetChild(lite_region_RegionNBT(root,r_num),"BlockStates");
    if(len)
        *len = state->value_a.len;
    return (uint64_t*)state->value_a.value;
}

int* lite_region_SizeArray(NBT* root,int r_num)
{
    NBT* size_state = NBT_GetChild(lite_region_RegionNBT(root,r_num),"Size");
    int* a = malloc(3*sizeof(int));
    int x = NBT_GetChild(size_state,"x")->value_i;
    int y = NBT_GetChild(size_state,"y")->value_i;
    int z = NBT_GetChild(size_state,"z")->value_i;
    a[0] = abs(x);
    a[1] = abs(y);
    a[2] = abs(z);
    return a;
}

uint64_t lite_region_BlockIndex(NBT* root, int r_num, int x, int y, int z)
{
    int* a = lite_region_SizeArray(root,r_num);
    // block_y * region_x * region_y + block_z * region_x + block_x
    int index = (a[0]) * (a[2]) * y + z * (a[0]) + x;
    free(a);
    return index;
}

uint64_t lite_region_BlockIndex_lr(LiteRegion* lr, int x, int y, int z)
{
    if( x >= lr->region_size.x || y >= lr->region_size.y || z >= lr->region_size.z )
        return -1;
    else
        return lr->region_size.x * lr->region_size.z * y + lr->region_size.x * z + x;
}

/* The function below uses the implement from another project:
 * "litematica-tools" from KikuGie
 * https://github.com/Kikugie/litematica-tools
 * It uses MIT License, the license file could be found in config/
 * since files in config/ are also from this project.
 */

int lite_region_BlockArrayPos(NBT* root, int r_num, uint64_t index)
{
    uint64_t* state = lite_region_BlockStatesArray(root,r_num,NULL);
    int bits = log2(lite_region_BlockNum(root,r_num));
    if(lite_region_BlockNum(root,r_num) > ((1 << bits)))
        bits++;
    if(bits < 2) bits = 2;
    uint64_t start_bit = index * bits;
    int start_state = start_bit / 64;
    int and_num = (1 << bits) - 1;
    int move_num = start_bit & 63;
    int end_num = start_bit % 64 + bits;
    int id = 0;
    if(end_num <= 64)
        id = (uint64_t)state[start_state] >> move_num & and_num;
    else
    {
        int move_num_2 = 64 - move_num;
        id = ((uint64_t)state[start_state] >> move_num | state[start_state + 1] << move_num_2)& and_num;
    }
    return id;
}

int lite_region_BlockArrayPos_lr(LiteRegion* lr, uint64_t index)
{
    if(index >= 0)
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
            id = ((uint64_t)state[start_state] >> move_num | state[start_state + 1] << move_num_2)& and_num;
            if( start_state + 1 >= lr->states_num)
                printf("??? with start_bit %ld, bits %d\n", start_bit, bits);
        }
        return id;
    }
    else return -1;
}

int lite_region_BlockArrayPos_ByCoordination(LiteRegion* lr, int x, int y, int z)
{
    uint64_t index = lite_region_BlockIndex_lr(lr, x, y, z);
    if(index == -1)
        return -1;
    else
        return lite_region_BlockArrayPos_lr(lr, index);
}

NBT* lite_region_SpecificBlockStatePalette(NBT* root, int r_num, int id)
{
    NBT* a = lite_region_BlockStatePalette(root,r_num);
    if(a)
    for(int i = 0; i < id; i++)
        a = a->next;
    return a;
}

char* lite_region_BlockType(NBT* root, int r_num, int id)
{
    return (char*)NBT_GetChild_Deep(lite_region_SpecificBlockStatePalette(root,r_num,id),"Properties","type",NULL)->value_a.value;
}

ItemList *lite_region_ItemList(NBT* root, int r_num)
{
    return lite_region_ItemListExtend(root,r_num,NULL);
}

ItemList *lite_region_ItemListExtend(NBT* root, int r_num, ItemList* oBlock)
{
    LiteRegion* lr = LiteRegion_Create(root, r_num);
    int bNum = lr->blocks->num;

    // First, read originBlockName and compare it to oBlock, add Blocks to it
    if(bNum == 0)
    {
        return NULL;
    }


    oBlock = lite_region_ItemList_WithoutNum(lr, oBlock);
    if(!oBlock)
        return NULL;

    // Second, read BlockStates and add number to oBlock.num

    BlackList* bl = BlackList_Init();

    //char process[] = "-\\|/";
    uint64_t volume = lr->region_size.x * lr->region_size.y * lr->region_size.z;
    for(int y = 0 ; y < lr->region_size.y ; y++)
    {
        for(int z = 0 ; z < lr->region_size.z ; z++)
        {
            for(int x = 0 ; x < lr->region_size.x ; x++)
            {
                uint64_t index = lite_region_BlockIndex_lr(lr,x,y,z);
                int id = lite_region_BlockArrayPos_lr(lr,index);
                char* id_block_name = lr->replaced_blocks->val[id];
                float percent = ((float)(index + 1) / volume) * 100;
                fprintf(stderr,_("[%.2f%%] Processing Blocks %lu/%lu, (%3d,%3d,%3d)/(%3d,%3d,%3d)"), percent ,index+1, volume ,
                        x,y,z,lr->region_size.x,lr->region_size.y,lr->region_size.z);
                fprintf(stderr, "\r");
                if(!BlackList_Scan(bl,id_block_name))
                {
                    // There is no need for searching repeat.
//                    if(ItemList_ScanRepeat(oBlock,id_block_name))  // search for item name
//                    {
                    if(lite_region_IsBlockWaterlogged(root,r_num,id))
                    {
                        if(!ItemList_ScanRepeat(oBlock,"minecraft_water_bucket"))
                            ItemList_InitNewItem(&oBlock,"minecraft:water_bucket");
                        ItemList_AddNum(oBlock,1,"minecraft:water_bucket");
                    }
                    if(!strcmp(id_block_name,"minecraft:water_bucket") ||
                      !strcmp(id_block_name,"minecraft:lava_bucket"))
                    {
                        if(lite_region_BlockLevel(root,r_num,id) != 0)
                            continue;    // It's not source, so skip
                    }
                    if(strstr(id_block_name,"_slab"))     // special for slab
                    {
                        if(!strcmp(lite_region_BlockType(root,r_num,id),"double"))
                        {   ItemList_AddNum(oBlock,2,id_block_name);
                            continue;
                        }
                    }
                    if(strstr(id_block_name,"_door"))
                    {
                        if(!strcmp(lite_region_DoorHalf(root,r_num,id),"upper"))
                        {
                            if(!strcmp(lite_region_DoorHalf(root,r_num,
                                                            lite_region_BlockArrayPos(root,r_num,lite_region_BlockIndex(root,r_num,x,y-1,z))),"lower"))
                                continue;
                        }
                    }


                    ItemList_AddNum(oBlock,1,id_block_name);
                    //}
                }

            }
        }
    }
    printf("\n");
    BlackList_Free(bl);
    LiteRegion_Free(lr);
    return oBlock;
}

int lite_region_IsBlockWaterlogged(NBT* root,int r_num,int id)
{
    NBT* status = NBT_GetChild_Deep(lite_region_SpecificBlockStatePalette(root,r_num,id),"Properties","waterlogged",NULL);
    if(!status)
        return 0;
    else
        if(!strcmp((char*)status->value_a.value,"true"))
            return 1;
    else return 0;
}

int lite_region_BlockLevel(NBT* root,int r_num,int id)
{
    NBT* level = NBT_GetChild_Deep(lite_region_SpecificBlockStatePalette(root,r_num,id),"Properties","level",NULL);
    if(!level)
        return -1;
    else
    {
        //printf("%d\n",atoi(level->value_a.value));
        return atoi(level->value_a.value);
    }
}

char* lite_region_DoorHalf(NBT* root,int r_num,int id)
{
    NBT* half = NBT_GetChild_Deep(lite_region_SpecificBlockStatePalette(root,r_num,id),"Properties","half",NULL);
    return (char*)half->value_a.value;
}

ItemList *lite_region_ItemList_WithoutNum(LiteRegion* lr, ItemList *o_il)
{
    BlackList* bl = BlackList_Init();
    /* Scan block lists and add blocks to itemlist */
    for(int i = 0 ; i < lr->blocks->num ; i++)
    {
        char* r_block_name = lr->replaced_blocks->val[i];
        if( !BlackList_Scan(bl, r_block_name) && !ItemList_ScanRepeat(o_il, r_block_name) )
        {
            if(ItemList_InitNewItem(&o_il, r_block_name))
            {
                BlackList_Free(bl);
                return NULL;
            }
        }
    }
    BlackList_Free(bl);
    return o_il;
}

dh_StrArray* lite_region_Name_StrArray(NBT* root)
{
    NBT* region_nbt = NBT_GetChild(root, "Regions")->child;
    dh_StrArray* str_arr = NULL;
    while(region_nbt)
    {
        dh_StrArray_AddStr( &str_arr, region_nbt->key);
        region_nbt = region_nbt->next;
    }
    return str_arr;
}
