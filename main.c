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
#include "dhlrc_config.h"
#define _TOSTR(a) #a
#define TOSTR(a) _TOSTR(a)

enum option{Reader, Litematic_material_lister, Litematic_block_show,
        #ifdef DH_DEBUG_IN_IDE
            Debug,
        #endif
            Exit};

enum option start_without_option();
int start_func(NBT* root, enum option opt);
void start_lrc_main(NBT* root);
void start_lrc_extend(NBT* root);
int lrc_extend_instance(NBT* root, int r_num, int64_t* array);
#ifdef DH_DEBUG_IN_IDE
int debug();
#endif

int main(int argc,char** argb)
{
    //printf( TOSTR(dhlrc_VERSION_MAJOR) "." TOSTR(dhlrc_VERSION_MINOR) "." TOSTR(dhlrc_VERSION_PATCH) "\n" );
    if(argc == 1)
    {
        char* trans = String_Translate("dh.main.usage");
        if(!strcmp(trans,"dhlrc.usage"))
            printf("Usage: %s [file] \n",argb[0]);
        else printf(trans, argb[0]);
        free(trans);
#ifndef DH_DEBUG_IN_IDE
        String_Translate_FreeLocale();
        return -1;
#endif
    }
    int size = 0;
#ifndef DH_DEBUG_IN_IDE
    uint8_t* data = (uint8_t*)dhlrc_ReadFile(argb[1],&size);
#else
    printf("You are in debug mode! Don't define \"DH_DEBUG_IN_IDE\" to use the normal program!\n");
    printf("Anyway, enter 3 in the following program (if success reading file) to enter debug function. \n\n");
    uint8_t* data = (uint8_t*)dhlrc_ReadFile("/path/to/litematic",&size);
#endif
    if(!data)
    {
        String_Translate_printfRaw("dh.main.fileError");
        String_Translate_FreeLocale();
        return -10;
    }
    NBT* root = NBT_Parse(data,size);
    free(data);

    if(!root)
    {
        String_Translate_printfRaw("dh.main.notValidNBT");
        String_Translate_FreeLocale();
        return -1;
    }
    else
    {
        dhlrc_mkconfig();
        String_Translate_printfRaw("dh.main.validNBT");
        start_func(root, start_without_option());
        NBT_Free(root);
        String_Translate_FreeLocale();
        return 0;
    }
}

enum option start_without_option()
{
    char* trans = String_Translate("dh.main.startWithoutOption");
    printf("%s", trans);
    free(trans);
#ifndef DH_DEBUG_IN_IDE
    dh_LineOut* output = InputLine_Get_OneOpt(1,1,1,0,2,'q');
#else
    dh_LineOut* output = InputLine_Get_OneOpt(1, 1, 1, 0 ,3 ,'q');
#endif
    if(output)
    {
        switch(output->type){
        case Integer:
        {
            int option = output->num_i;
            dh_LineOut_Free(output);
            return option;
        }
        case Character:
        case Empty:
        default:
            dh_LineOut_Free(output);
            break;
        }
        return Exit;
    }
    else
        return Exit;
}

int start_func(NBT *root, enum option opt)
{
    switch (opt) {
    case Reader:
        nbtlr_Start(root);
        return 0;
    case Litematic_material_lister:
        start_lrc_main(root);
        return 0;
    case Litematic_block_show:
        start_lrc_extend(root);
        return 0;
#ifdef DH_DEBUG_IN_IDE
    case Debug:
        debug();
        return 0;
#endif
    case Exit:
        return 0;
    default: return 0;
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
            char* trans = String_Translate("dhlrc.regionNum");
            printf(trans,region_num);
            free(trans);
            for(int i = 0 ; i < region_num ; i++)
            {
                printf("[%2d] %s\n",i,region_name[i]);
            }
            trans = String_Translate("dhlrc.extend.enterRegionNum");
            printf("%s",trans);
            free(trans);
            dh_LineOut* output1 = InputLine_Get_OneOpt(1,1,1,0,region_num,'q');
            if(output1)
            {
                switch(output1->type)
                {
                case Integer:
                {
                    int read_region_num = output1->num_i;
                    dh_LineOut_Free(output1);
                    system("clear");

                    int* region_size = lite_region_SizeArray(root, read_region_num);
                    char* trans = String_Translate("dhlrc.extend.regionInfo");
                    printf(trans, read_region_num, region_name[read_region_num], region_size[0], region_size[1], region_size[2]);
                    free(trans);

                    trans = String_Translate("dhlrc.extend.askRequest");
                    printf("%s",trans);
                    free(trans);
                    dh_LineOut* output2 = InputLine_Get_MoreDigits(1, 3, 2, 0, region_size[0] - 1, 0, region_size[1] - 1, 0 ,region_size[2] - 1, 'b', 'q');
                    if(output2)
                    {
                        switch(output2->type)
                        {
                        case NumArray:
                        {
                            int ret = lrc_extend_instance(root, read_region_num, output2->val);
                            dh_LineOut_Free(output2);
                            free(region_size);
                            if(ret == 0 || ret == -1)
                            {
                                // exit program
                                lite_region_FreeNameArray(region_name, region_num);
                                return;
                            }
                            else if(ret == 1)
                                break;
                            else break; // Silence warning
                        }
                        case Character:
                        {
                            if(output2->val_c == 'q')
                            {
                                free(region_size);
                                dh_LineOut_Free(output2);
                                lite_region_FreeNameArray(region_name, region_num);
                                return;
                            }
                        }
                        default: // default: back
                            free(region_size);
                            dh_LineOut_Free(output2);
                            break;
                        }
                    }
                    else // Second output (Enter coordination) failed
                    {
                        free(region_size);
                        lite_region_FreeNameArray(region_name, region_num);
                        return; // out of the instance
                    }
                    break; // Each case should follow a break.
                }
                default: // The first case. Character is the only second option
                    dh_LineOut_Free(output1);
                    lite_region_FreeNameArray(region_name, region_num);
                    return;
                }
            }
            else  // First output (entry) failed
            {
                lite_region_FreeNameArray(region_name, region_num);
                break;
            }
        }
    }
}

int lrc_extend_instance(NBT* root, int r_num, int64_t *array)
{
    int block_num = lite_region_BlockNum(root,r_num);
    char** block_name = lite_region_BlockNameArray(root, r_num, block_num);
    int index = lite_region_BlockIndex(root, r_num, array[0], array[1], array[2]);
    int id = lite_region_BlockArrayPos(root, r_num, index);
//    char lang[] = "en_us";
    char* trans_first = String_Translate("dhlrc.extend.blockPosInfo");
    if(strcmp(trans_first, "dhlrc.extend.blockPosInfo"))
        printf(trans_first, array[0], array[1], array[2], block_name[id]);
    else printf("Translation lost! But the block is %s.\n",block_name[id]);
    free(trans_first);
    trans_first = String_Translate("dhlrc.extend.askRequestTwice");
    printf("%s", trans_first);
    free(trans_first);
    int* region_size = lite_region_SizeArray(root, r_num);
    dh_LineOut* input = InputLine_Get_MoreDigits(1, 3, 2, 0, region_size[0] - 1, 0, region_size[1] - 1, 0, region_size[2] - 1, 'b', 'q');
    if(input){
    switch (input->type) {
    case Character:
    {
        free(region_size);
        lite_region_FreeNameArray(block_name, block_num);
        char opt = input->val_c;
        dh_LineOut_Free(input);
        if(opt == 'q')
            return 0;
        else if(opt == 'b')
            return 1;
        else return 0;  // Just use this to silence warning.
    }
    case NumArray:
    {
        free(region_size);
        lite_region_FreeNameArray(block_name, block_num);
        int ret = lrc_extend_instance(root, r_num, input->val); // This is the only entry to the next instance, so return value will stay same.
        dh_LineOut_Free(input);
        return ret;
    }
    default:
        free(region_size);
        lite_region_FreeNameArray(block_name, block_num);
        dh_LineOut_Free(input);
        return 1;  // Empty: back
    }
    }
    else
    {
        free(region_size);
        lite_region_FreeNameArray(block_name, block_num);
        return -1;
    }

}

#ifdef DH_DEBUG_IN_IDE

int debug()
{
    return 0;
}


#endif
