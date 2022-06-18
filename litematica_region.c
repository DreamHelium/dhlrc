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
        return NBT_GetChild(lite_region_RegionNBT(root,r_num),"BlockStatePalette") -> child;
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
    if(end_num < 64)
        id = state[start_state] >> move_num & and_num;
    else
    {
        int move_num_2 = 64 - move_num;
        id = ((uint64_t)state[start_state] >> move_num | state[start_state + 1] << move_num_2)& and_num;
    }
    return id;
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
    int bNum = lite_region_BlockNum(root,r_num);
    char** originBlockName = lite_region_BlockNameArray(root,r_num,bNum);

    // First, read originBlockName and compare it to oBlock, add Blocks to it
    if(bNum == 0)
    {
        return NULL;
    }
    BlackList* bl = BlackList_Init();
    ReplaceList* rl = ReplaceList_Init();
    for(int i = 0 ; i < bNum ; i++)    //scan block
    {
        char* i_block_name = originBlockName[i];
        i_block_name = ReplaceList_Replace(rl,i_block_name);
        if(!ItemList_ScanRepeat(oBlock,i_block_name) && !BlackList_Scan(bl,i_block_name))
        {
            if(ItemList_InitNewItem(&oBlock,i_block_name))
            {
                BlackList_Free(bl);
                bl = NULL;
                ReplaceList_Free(rl);
                rl = NULL;
                lite_region_FreeNameArray(originBlockName,bNum);
                return NULL;
            }
        }    
    }

    // Second, read BlockStates and add number to oBlock.num

    char process[4] = {'-','\\','|','/'};
    int* rSize = lite_region_SizeArray(root,r_num);
    for(int y = 0 ; y < rSize[1] ; y++)
    {
        for(int z = 0 ; z < rSize[2] ; z++)
        {
            for(int x = 0 ; x < rSize[0] ; x++)
            {
                uint64_t index = lite_region_BlockIndex(root,r_num,x,y,z);
                int id = lite_region_BlockArrayPos(root,r_num,index);
                char* id_block_name = originBlockName[id];
                id_block_name = ReplaceList_Replace(rl,id_block_name);
                printf("[%c] Processing Blocks %ld/%d, (%3d,%3d,%3d)/(%3d,%3d,%3d)\r", process[id % 4] ,index+1,rSize[0] * rSize[1] * rSize[2],
                        x,y,z,rSize[0],rSize[1],rSize[2]);
                fflush(stdout);
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
    bl = NULL;
    ReplaceList_Free(rl);
    rl = NULL;
    lite_region_FreeNameArray(originBlockName,bNum);
    free(rSize);
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
