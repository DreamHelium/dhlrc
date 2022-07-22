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
#include "lrc_extend.h"
/*#include "dhlrc_config.h"*/

#ifndef DH_DISABLE_TRANSLATION
#include <libintl.h>
#include <locale.h>
#define _(str) gettext (str)
#else
#define _(str) str
#endif

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
#ifdef DH_DEBUG_IN_IDE
int debug(NBT* root);
#endif

int main(int argc,char** argb)
{
#ifndef DH_DISABLE_TRANSLATION
    setlocale(LC_CTYPE, "");
    setlocale(LC_MESSAGES, "");
    bindtextdomain("dhlrc", "locale");
    textdomain("dhlrc");
#endif
    //printf( TOSTR(dhlrc_VERSION_MAJOR) "." TOSTR(dhlrc_VERSION_MINOR) "." TOSTR(dhlrc_VERSION_PATCH) "\n" );
    if(argc == 1)
    {
        /*
        int err = 0;
        char* trans = String_TranslateWithErrCode("dh.main.usage", &err);
        if(err != 0)
            printf("Usage: %s [file] \n",argb[0]);
        else printf(trans, argb[0]);
        free(trans);
        */


        printf(_("Usage: %s [file]\n"), argb[0]);
#ifndef DH_DEBUG_IN_IDE
        return -1;
#endif
    }
    int size = 0;
    uint8_t* data = NULL;
    data = (uint8_t*)dhlrc_ReadFile(argb[1],&size);
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
//        char* trans = String_TranslateWithErrCode("dh.main.validNBT", &err);
//        if(err != 0) // err encounter
//            printf("Error when loading translation file, although the program could still run, you may have no idea what to do next.\n"
//                   "If you know how to operate, just continue. If not, you could enter 'q' to exit the program.\n");
//        else printf("%s",trans);
//        free(trans);
//        trans = NULL;

        puts(_("Valid NBT file!"));
        int ret = start_func(root, start_without_option());
        NBT_Free(root);
        return ret;
    }
}

enum option start_without_option()
{
    printf(_("There are three functions:\n\
[0] NBT lite reader with modifier\n\
[1] Litematica material list with recipe combination\n\
[2] Litematica block reader\n\n"));
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
        return debug(root);
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
            ItemList_CombineRecipe(&il);
            ItemList_Sort(&il);
            ItemList_toCSVFile("test.csv",il);
            ItemList_Free(il);
        }
    }
}



#ifdef DH_DEBUG_IN_IDE

int debug(NBT* root)
{
    NBT_Pos* pos = NBT_Pos_init(root);
    if(NBT_Pos_GetChild_Deep(pos,"Metadata","TimeCreated",NULL))
    {
        int ret = nbtlr_Start_Pos(pos);
        NBT_Pos_Free(pos);
        return ret;
    }
    else
    {
        NBT_Pos_Free(pos);
        return -1;
    }
}


#endif
