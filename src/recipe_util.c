/*  recipe_util - utilities to process recipes
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

#include "recipe_util.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "dhlrc_list.h"
#include <dhelium/dh_string_util.h>
#include <dhelium/file_util.h>
#ifndef DH_DISABLE_TRANSLATION
#include <libintl.h>
#define _(str) gettext (str)
#else
#define _(str) str
#endif

long* NumArray_GetFromInput(int* array_num, int max_num)
{
    if(!array_num) return NULL;
    printf(_("Input numbers directly, or type 'a' for all numbers, 'q' to quit (a): "));
    dh_limit* limit = dh_limit_Init(NumArray);
    if(limit)
    {
        dh_limit_SetArrayArgs(limit, -1, 1, 1, 1);
        if(!dh_limit_AddInt(limit, 0 , max_num - 1 ))
        {
            dh_limit_Free(limit);
            *array_num = 0;
            return NULL;
        }
    }
    else
    {
        *array_num = 0;
        return NULL;
    }
    dh_LineOut* dout = InputLine_General( sizeof(long), limit, 0, "aq", 1);
    dh_limit_Free(limit);
    if(dout)
    {
        if(dout->type == NumArray)
        {
            long* out = (long*)malloc( dout->len * sizeof(long) );
            if(out)
            {
                memcpy( out, dout->val, dout->len*sizeof(long) );
                *array_num = dout->len;
                dh_LineOut_Free(dout);
                return out;
            }
            else
            {
                dh_LineOut_Free(dout);
                *array_num = 0;
                return NULL;
            }
        }
        else if(dout->type == Empty || (dout->type == Character && dout->val_c == 'a'))
        {
            dh_LineOut_Free(dout);
            long* out = (long*)malloc( max_num * sizeof(long) );
            if(out)
            {
                for(int i = 0 ; i < max_num ; i++ )
                    out[i] = i;
                *array_num = max_num;
                return out;
            }
            else
            {
                *array_num = 0;
                return NULL;
            }
        }
        else
        {
            dh_LineOut_Free(dout);
            *array_num = 0;
            return NULL;
        }
    }
    else
    {
        *array_num = 0;
        return NULL;
    }
}

int ItemList_CombineRecipe(ItemList** o_bl, const char* dirpos, DhGeneral* general)
{
    /* Get item names */
    RecipeList* rcl = RecipeList_Init(dirpos, *o_bl);
    dh_StrArray* item_names = RecipeList_ItemNamesWithNamespace(rcl);

    dh_printf(general, _("There are some items to craft:\n"));
    for(int i = 0 ; i < item_names->num; i++)
    {
        dh_option_printer(general, i, (item_names->val)[i]);
    }

    int arr_num = 0;
    long* num_arr = NumArray_GetFromInput(&arr_num, item_names->num);
    if(num_arr == NULL)
    {
        RecipeList_Free(rcl);
        dh_StrArray_Free(item_names);
        return 0;
    }

    for(int i = 0 ; i < arr_num ; i++)
    {
        char* item_name = (item_names->val)[i];
        ItemList* return_recipe = ItemList_Recipe(rcl, ItemList_GetItemNum(*o_bl, item_name),item_name, general);
        if(return_recipe)
        {
            ItemList_Combine(o_bl, return_recipe);
            ItemList_DeleteItem(o_bl, item_name);
        }
    }
    RecipeList_Free(rcl);
    dh_StrArray_Free(item_names);
    return 1;
}

char* Name_BlockTranslate(const char *block_name)
{
    cJSON* trans_data = dhlrc_FileToJSON("translation.json");

    char* pure_name = strchr(block_name,':') + 1;

    int len = strlen(pure_name) + 1 + 5 + 10;

    char* origin_name = (char*) malloc(len * sizeof(char));
    strcpy(origin_name,"item.minecraft.");
    strcat(origin_name,pure_name);
    cJSON* trans_line = cJSON_GetObjectItem(trans_data,origin_name);
    if(cJSON_IsString(trans_line))
    {
        char* trans_name = cJSON_GetStringValue(trans_line);
        free(origin_name);
        origin_name = NULL;

        char* out = (char*)malloc((strlen(trans_name)+1)*sizeof(char));
        strcpy(out,trans_name);

        cJSON_Delete(trans_data);

        return out;
    }
    else
    {
        len++;
        free(origin_name);
        origin_name = NULL;
        origin_name = (char*)malloc(len * sizeof(char));
        strcpy(origin_name,"block.minecraft.");
        strcat(origin_name,pure_name);
        trans_line = cJSON_GetObjectItem(trans_data,origin_name);

        if(cJSON_IsString(trans_line))
        {
            char* trans_name = cJSON_GetStringValue(trans_line);
            free(origin_name);
            origin_name = NULL;

            char* out = (char*)malloc((strlen(trans_name)+1)*sizeof(char));
            strcpy(out,trans_name);

            cJSON_Delete(trans_data);

            return out;
        }
        else
        {
            cJSON_Delete(trans_data);
            free(origin_name);
            origin_name = NULL;
            return NULL;
        }
    }
}

char** NameArray_CanCraft(int* num, ItemList *il)
{
    return NULL;
}
