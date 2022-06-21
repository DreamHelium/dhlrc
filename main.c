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
#include <ctype.h>
#include "recipe_util.h"
#include "file_util.h"
#include "nbt_litereader.h"
#include "dh_string_util.h"

enum option{Reader, Modifier, Litematic_material_lister, Litematic_block_show, Exit};

enum option start_without_option();
void start_func(NBT* root, enum option opt);
void start_lrc_main(NBT* root);
void start_lrc_extend(NBT* root);

int main(int argc,char** argb)
{
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
    else
    {
        printf("It's a valid NBT file, continue.\n");
        start_func(root, start_without_option());
        NBT_Free(root);
        return 0;
    }
//    //printf("\n\nAfter Sorting Block list: \n");
//    //    ItemList* block_list_read = blockList0;
//    // below is code to print item list
//    /*
//    for( ; block_list_read ; block_list_read = block_list_read->next)
//    {
//        char* trans_name = Name_BlockTranslate(block_list_read->name);
//        if(trans_name)
//            printf("%s,%d\n",trans_name,block_list_read->num);
//        else
//            printf("%s,%d\n",block_list_read->name,block_list_read->num);
//        free(trans_name);
//    }
//    */
//    ItemList_toCSVFile("test.csv",blockList0);
//    ItemList_Free(blockList0);
//    lite_region_FreeNameArray(region,rNum);
}

enum option start_without_option()
{
    printf("Please select a option to start:\n");
    printf("[1] NBT lite reader\n");
    printf("[2] NBT Modifier (Currently couldn't use and only an empty func)\n");
    printf("[3] Litematic material lister with recipe combiner\n");
    printf("[4] Litematic block reader\n");

    printf("Enter a number or enter 'q' to exit program: ");
    char* output = InputLine_Get_OneOpt(1,1,4,1,'q');
    if(output)
    {
        if(isalpha(output[0]))
        {
            free(output);
            return Exit;
        }
        else
        {
            int value = atoi(output);
            free(output);
            return value - 1;
        }
    }
    else
        return Exit;
}

void start_func(NBT *root, enum option opt)
{
    switch (opt) {
    case Reader:
        nbtlr_Start(root, NULL);
        break;
    case Modifier:
        nbtlr_Modifier_Start(root);
        break;
    case Litematic_material_lister:
        start_lrc_main(root);
        break;
    case Litematic_block_show:
        start_lrc_extend(root);
        break;
    case Exit:
        break;
    default: break;
    }
}

void start_lrc_main(NBT *root)
{
    int region_num = lite_region_Num(root);
    int err = 0;
    char** region_name = lite_region_Name(root,region_num,&err);
    if(region_name)
    {
        printf("There are %d regions:\n",region_num);
        for(int i = 0 ; i < region_num ; i++)
        {
            printf("[%2d] %s\n",i,region_name[i]);
        }
        int process_num = 0;
        long* process_region_i = NumArray_GetFromInput(&process_num, region_num);
        ItemList* il = NULL;
        if(process_region_i)
        {
            for(int i = 0 ; i < process_num ; i++)
            {
                printf("Processing: region %d / %d : [%ld] %s \n",
                       i,process_num,process_region_i[i],region_name[process_region_i[i]]);
                il = lite_region_ItemListExtend(root, process_region_i[i], il);
            }
            free(process_region_i);
            lite_region_FreeNameArray(region_name,region_num);
            ItemList_DeleteZeroItem(&il);
            ItemList_CombineRecipe(&il);
            ItemList_Sort(&il);
            ItemList_toCSVFile("test.csv",il);
            ItemList_Free(il);
        }
    }
}

void start_lrc_extend(NBT* root)
{
    int region_num = lite_region_Num(root);
    int err = 0;
    char** region_name = lite_region_Name(root, region_num, &err);
    int continue_func_out = 1;
    if(region_name)
    {
        while(continue_func_out)
        {
            system("clear");
            printf("There are %d regions:\n",region_num);
            for(int i = 0 ; i < region_num ; i++)
            {
                printf("[%2d] %s\n",i,region_name[i]);
            }
            printf("Enter a number or enter 'q' to exit the program: ");
            char* output1 = InputLine_Get_OneOpt(1,0,region_num,1,'q');
            if(output1)
            {
                if(output1[0] == 'q')
                {
                    free(output1);
                    lite_region_FreeNameArray(region_name,region_num);
                    break;
                }
                else
                {
                    int read_region_num = atoi(output1);
                    free(output1);
                    system("clear");
                    printf("You are reading region [%2d] %s :",read_region_num,region_name[read_region_num]);

                    int* region_size = lite_region_SizeArray(root, read_region_num);
                    printf("This region's size is (%d, %d, %d)\n",region_size[0], region_size[1], region_size[2]);

                    printf("Please enter the coordination of the block (in format: x y z without addtional character).\n");
                    printf("Or enter 'b' to choose another region, enter 'q' to exit the program: ");
                    char* output2 = InputLine_Get_MoreDigits(3, 2, 0, region_size[0] - 1, 0, region_size[1] - 1, 0 ,region_size[2] - 1, 'b', 'q');
                    if(output2)
                    {
                        if(output2[0] == 'q')
                        {
                            free(output2);
                            lite_region_FreeNameArray(region_name,region_num);
                            break;
                        }
                        else if(output2[0] == 'b')
                        {
                            free(output2);
                        }
                        else
                        {
                            int pos_num = 0;
                            long* block_pos = NumArray_From_String(output2, &pos_num, 0);
                            if(block_pos)
                            {
                                if(pos_num == 3)
                                {
                                    int continue_func_in = 1; // Inside the reading process
                                    free(output2);
                                    int block_num = lite_region_BlockNum(root, read_region_num);
                                    char** block_name = lite_region_BlockNameArray(root, read_region_num,block_num);
                                    int block_index = lite_region_BlockIndex(root, read_region_num, block_pos[0], block_pos[1], block_pos[2]);
                                    int block_id = lite_region_BlockArrayPos(root, read_region_num, block_index);
                                    printf("Block in (%ld, %ld, %ld) is: %s\n",block_pos[0], block_pos[1], block_pos[2], block_name[block_id]);
                                    while(continue_func_in)
                                    {
                                        printf("Please enter another coordination or enter 'b' to choose another region, enter 'q' to exit the program: ");
                                        char* output3 = InputLine_Get_MoreDigits(3, 2 ,0 , region_size[0] - 1,0, region_size[1] - 1,0,region_size[2] - 1,'b','q');
                                        if(output3)
                                        {
                                            if(output3[0] == 'q')
                                            {
                                                free(output3);
                                                lite_region_FreeNameArray(region_name, region_num);
                                                continue_func_out = 0;
                                                break;
                                            }
                                            else if(output3[0] == 'b')
                                            {
                                                free(output3);
                                                break;
                                            }
                                            else
                                            {
                                                int pos_num2 = 0;
                                                long* block2_pos = NumArray_From_String(output3, &pos_num2, 0);
                                                if(pos_num2 == 3)
                                                {
                                                    int block2_index = lite_region_BlockIndex(root, read_region_num, block2_pos[0], block2_pos[1], block2_pos[2]);
                                                    int block2_id = lite_region_BlockArrayPos(root, read_region_num, block2_index);
                                                    printf("Block in (%ld, %ld, %ld) is: %s\n", block2_pos[0], block2_pos[1], block2_pos[2], block_name[block2_id]);
                                                }
                                                else
                                                {
                                                    printf("??? Exiting the program\n");
                                                    continue_func_out = 0;
                                                    free(output3);
                                                    lite_region_FreeNameArray(region_name,region_num);
                                                    break;
                                                }
                                            }
                                        }
                                        else
                                        {
                                            lite_region_FreeNameArray(region_name,region_num);
                                            continue_func_out = 0;
                                            break;
                                        }
                                    }
                                }
                                else // This shouldn't happen, if seeing this, should be a bug.
                                {
                                    printf("??? Exiting the program\n");
                                    free(output2);
                                    lite_region_FreeNameArray(region_name,region_num);
                                    break;
                                }
                            }
                        }
                    }
                    else
                    {
                        lite_region_FreeNameArray(region_name,region_num);
                        break;
                    }
                }
            }
            else
            {
                lite_region_FreeNameArray(region_name,region_num);
                break;
            }
        }
    }
}
