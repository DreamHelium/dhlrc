/*  litematica_reader_c - litematic file reader in C
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

#include <stdio.h>
#include "litematica_region.h"
#include <stdint.h>
#include <stdlib.h>
#include "libnbt/nbt.h"
#include <string.h>
#include "recipe_util.h"
#include "file_util.h"
#include "nbt_litereader.h"


int main(int argc,char** argb)
{
    //Test();
    if(argc == 1)
    {
        printf("Usage: %s [file] \n",argb[0]);
        printf("\n");
    }

    int size = 0;
    uint8_t* data = (uint8_t*)dhlrc_ReadFile(argb[1],&size);
    //uint8_t* data = (uint8_t*)dhlrc_ReadFile("/path/to/litematic",&size);
    if(!data)
    {
        if(argc != 1)
            printf("Encounter error reading file!\n");
        return -10;
    }
    NBT* root = NBT_Parse(data,size);
    free(data);

    if(!root)
    {
        printf("Not a vaild NBT file!\n");
        return -1;
    }
    nbtlr_Start(root,NULL);

    int rNum = lite_region_Num(root);
    //printf("%d\n",rNum);
    int region_err = 0;
    char** region = lite_region_Name(root,rNum,&region_err);
    if(region);
    else
    {
        NBT_Free(root);
        return -1;
    }

    ItemList* blockList0 = NULL;
    for(int i = 0 ; i < rNum ; i++)
    {
        printf("Processing Region %d / %d : %s \n",i + 1,rNum,region[i]);
        blockList0 = lite_region_ItemListExtend(root,i,blockList0);
    }
    ItemList_DeleteZeroItem(&blockList0);
    ItemList_CombineRecipe(&blockList0);
    ItemList_Sort(&blockList0);


    //printf("\n\nAfter Sorting Block list: \n");
    //    ItemList* block_list_read = blockList0;
    // below is code to print item list
    /*
    for( ; block_list_read ; block_list_read = block_list_read->next)
    {
        char* trans_name = Name_BlockTranslate(block_list_read->name);
        if(trans_name)
            printf("%s,%d\n",trans_name,block_list_read->num);
        else
            printf("%s,%d\n",block_list_read->name,block_list_read->num);
        free(trans_name);
    }
    */
    ItemList_toCSVFile("test.csv",blockList0);
    ItemList_Free(blockList0);
    lite_region_FreeNameArray(region,rNum);
    NBT_Free(root);
    return 0;
}
