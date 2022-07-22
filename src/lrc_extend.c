/*  lrc_extend - Litematica Reader Extension
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

#include "lrc_extend.h"
#include "litematica_region.h"
#include <stdio.h>
#include <stdlib.h>
#ifndef DH_DISABLE_TRANSLATION
#include <libintl.h>
#define _(str) gettext (str)
#else
#define _(str) str
#endif

static int lrc_extend_instance(LiteRegion* lr, dh_LineOut* dout);

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
            printf(_("There are %d regions:\n"),region_num);
            for(int i = 0 ; i < region_num ; i++)
            {
                printf("[%2d] %s\n",i,region_name[i]);
            }

            printf("\n");
            printf(_("Enter the region number, or enter 'q' to exit program (q): "));
            dh_limit* limit_1 = dh_limit_Init(Integer);
            dh_limit_AddInt(limit_1, 0 , region_num - 1);
            dh_LineOut* output1 = InputLine_General( sizeof(int64_t), limit_1, 0, "q", 1);
            dh_limit_Free(limit_1);
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
                    printf(_("You are reading region: [%2d] %s:\nThe size of the region is (%d, %d, %d).\n\n")
                           , read_region_num, region_name[read_region_num], region_size[0], region_size[1], region_size[2]);

                    printf(_("Please enter the coordination of the block (just numbers of x y z without additional character\n\
or enter 'b' to choose another region, enter 'q' to exit the program (b): "));

                    dh_limit* limit_2 = dh_limit_Init(NumArray);
                    dh_limit_SetArrayArgs( limit_2, 3, 0, 0, 0);
                    dh_limit_AddInt(limit_2, 0, region_size[0] - 1);
                    dh_limit_AddInt(limit_2, 0, region_size[1] - 1);
                    dh_limit_AddInt(limit_2, 0, region_size[2] - 1);
                    dh_LineOut* output2 = InputLine_General(sizeof(int64_t), limit_2, 0, "bq", 1);
                    dh_limit_Free(limit_2);
                    if(output2)
                    {
                        switch(output2->type)
                        {
                        case NumArray:
                        {
                            LiteRegion* lr = LiteRegion_Create(root, read_region_num);
                            if(!lr)
                            {
                                dh_LineOut_Free(output2);
                                free(region_size);
                                lite_region_FreeNameArray(region_name, region_num);
                            }
                            int ret = lrc_extend_instance(lr, output2);
                            LiteRegion_Free(lr);
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

static int lrc_extend_instance(LiteRegion* lr, dh_LineOut* dout)
{
    int come_back = 1;
    while(1){
        if(!lr)
            return -1;
        if(come_back){
        system("clear");
        printf(_("You are reading region: [%2d] %s:\nThe size of the region is (%d, %d, %d).\n\n")
                           , lr->region_num, lr->name, lr->region_size.x , lr->region_size.y , lr->region_size.z);
        come_back = 0;
        }
        int64_t* array = dout->val;
        char** block_name = lr->blocks->val;
        int index = lite_region_BlockIndex_lr(lr, array[0], array[1], array[2]);
        int id = lite_region_BlockArrayPos_lr(lr, index);
        printf(_("The block in (%ld, %ld, %ld) is %s.\n"), array[0], array[1], array[2], block_name[id]);
        printf(_("The block's position in BlockStatePalette is %d.\n"), id);
        printf(_("Please enter the coordination of the block again, or enter 'b' to choose another region, \
enter 'r' to read the detail of the block, enter 'q' to exit the program (b): "));
        dh_limit* limit = dh_limit_Init(NumArray);
        dh_limit_SetArrayArgs(limit, 3, 0, 0, 0);
        dh_limit_AddInt(limit, 0, lr->region_size.x - 1);
        dh_limit_AddInt(limit, 0, lr->region_size.y - 1);
        dh_limit_AddInt(limit, 0, lr->region_size.z - 1);
        dh_LineOut* input = InputLine_General(sizeof(int64_t), limit, 0, "bqr", 1);
        dh_limit_Free(limit);
        if(input){
        switch (input->type) {
        case Character:
        {
            char opt = input->val_c;
            dh_LineOut_Free(input);

            if(opt == 'r')
            {
                NBT_Pos* n_pos = NBT_Pos_Copy(lr->region_pos);
                NBT_Pos_GetChild(n_pos, "BlockStatePalette");
                NBT_Pos_AddToTree(n_pos, id);
                int ret = nbtlr_Start_Pos(n_pos);
                NBT_Pos_Free(n_pos);
                come_back = 1;
                if(ret == 1)
                    continue;
                else{
                    dh_LineOut_Free(dout);
                    return ret;
                }
            }
            dh_LineOut_Free(dout);
            if(opt == 'q')
                return 0;
            if(opt == 'b')
                return 1;

            else return 0;  // Just use this to silence warning.
        }
        case NumArray:
        {
            /* Continue */
            dh_LineOut_Free(dout);
            dout = input;
            continue;
        }
        default:
            dh_LineOut_Free(input);
            dh_LineOut_Free(dout);
            return 1;  // Empty: back
        }
        }
        else
        {
            dh_LineOut_Free(dout);
            return -1;
        }
    }
}
