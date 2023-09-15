/*  litematica_reader_c - litematic file reader in C (ISO C mode)
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

#include "dhlrc_list.h"
#include "main.h"
#include "translation.h"
#include <dh/file_util.h>
#include <dh/dh_string_util.h>
#include "litematica_region.h"
#include "recipe_util.h"
#include "lrc_extend.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <dh/dh_general_isoc.h>

enum option{Reader, Litematic_material_lister, Litematic_block_show,
        #ifdef DH_DEBUG_IN_IDE
            Debug,
        #endif
            Exit};

static enum option start_without_option();
static int start_func(NBT* root, enum option opt);
static int start_lrc_main(NBT* root);
#ifdef DH_DEBUG_IN_IDE
int debug();
#endif


int main_isoc(int argc, char** argv)
{
#ifndef DH_DEBUG_IN_IDE
    if(argc == 1)
        return -1;
#endif
    int size = 0;
    uint8_t* data = NULL;
    data = (uint8_t*)dhlrc_ReadFile(argv[1],&size);
#ifdef DH_DEBUG_IN_IDE
    printf("You are in debug mode! Don't define \"DH_DEBUG_IN_IDE\" to use the normal program!\n");
    printf("Anyway, enter 3 in the following program (if success reading file) to enter debug function. \n\n");
    if(!data)
        data = (uint8_t*)dhlrc_ReadFile("/path/to/litematic",&size);
#endif
    if(!data)
    {
        puts(_("Error when reading file."));
        return -10;
    }
    NBT* root = NBT_Parse(data,size);
    free(data);

    if(!root)
    {
        puts(_("Not a valid NBT file!"));
        return -1;
    }
    else
    {
        if(!dhlrc_FileExist("config.json"))
            dhlrc_mkconfig();
        puts(_("Valid NBT file!"));
        int ret = start_func(root, start_without_option());
        NBT_Free(root);
        return ret;
    }
}


static enum option start_without_option()
{
    printf(_("There are three functions:\n"
    "[0] NBT lite reader with modifier\n"
    "[1] Litematica material list with recipe combination\n"
    "[2] Litematica block reader\n\n"));
    printf(_("Please select an option, or enter 'q' to exit the program (q): "));
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

static int start_func(NBT *root, enum option opt)
{
    switch (opt) {
    case Reader:
        nbtlr_Start(root);
        return 0;
    case Litematic_material_lister:
        return start_lrc_main(root);
    case Litematic_block_show:
        start_lrc_extend(root);
        return 0;
#ifdef DH_DEBUG_IN_IDE
    case Debug:
        return debug();
#endif
    case Exit:
        return 0;
    default: return 0;
    }
}

static int start_lrc_main(NBT *root)
{
    int region_num = lite_region_Num(root);
    int err = 0;
    char** region_name = lite_region_Name(root,region_num,&err);
    if(region_name)
    {
        printf(_("There are %d regions:\n"),region_num);
        for(int i = 0 ; i < region_num ; i++)
        {
            printf("[%2d] %s\n",i,region_name[i]);
        }
        printf("\n");
        int process_num = 0;
        long* process_region_i = NumArray_GetFromInput(&process_num, region_num);
        ItemList* il = NULL;
        if(process_region_i)
        {
            for(int i = 0 ; i < process_num ; i++)
            {
                printf(_("Processing: region %d / %d : [%ld] %s \n"),
                       i,process_num,process_region_i[i],region_name[process_region_i[i]]);
                il = lite_region_ItemListExtend(root, process_region_i[i], il, 1);
            }
            free(process_region_i);
            lite_region_FreeNameArray(region_name,region_num);
            ItemList_DeleteZeroItem(&il);

            DhIsoc* isoc = dh_general_isoc_new();
            g_message("%d", DH_IS_GENERAL(isoc));

            ItemList_CombineRecipe(&il, "recipes", DH_GENERAL(isoc));
            g_object_unref(isoc);

            ItemList_Sort(&il);
            ItemList_toCSVFile("test.csv",il);
            ItemList_Free(il);
        }
    }
    return dh_exit();
}



#ifdef DH_DEBUG_IN_IDE

int debug()
{
    printf("%d\n", g_str_has_suffix("minecraft:stonecutting","smelting"));
    return 0;
}


#endif
