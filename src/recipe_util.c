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
#include "litematica_region.h"
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
    printf(_("Input numbers directly, or type 'a' for all numbers (a): "));
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
    dh_LineOut* dout = InputLine_General( sizeof(long), limit, 0, "a", 1);
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
        else
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
    }
    else
    {
        *array_num = 0;
        return NULL;
    }
}

int ItemList_CombineRecipe(ItemList** o_bl)
{
    ItemList* il = *o_bl;
    do
    {
        int craft_num = 0;
        char** craft_list = NameArray_CanCraft(&craft_num,il);
        if(!craft_list)
        {
            printf("There is no items to craft.\n");
            return 0;
        }
        else
        {
            printf("Please select recipes to combine: \n");
            for(int i = 0 ; i < craft_num ; i++)
            {
                char* trans_name = Name_BlockTranslate(craft_list[i]);
                if(trans_name)
                    printf("[%d] %s \n",i,trans_name);
                else
                    printf("[%d] %s\n",i,craft_list[i]);
                free(trans_name);
                trans_name = NULL;
            }
            int need_num = 0;
            long* need_craft = NumArray_GetFromInput(&need_num,craft_num);
            if(!need_craft)
            {
                lite_region_FreeNameArray(craft_list,craft_num);
                return 0;
            }
            else
            {
                // combine itemlist
                for(int i = 0 ; i < need_num ; i++)
                {
                    if(need_craft[i] < 0 || need_craft[i] >= craft_num)
                    {
                        perror("Unexpected num\n");
                    }
                    else
                    {
                        char* need_item = craft_list[need_craft[i]];
                        int item_num = ItemList_GetItemNum(il,need_item);
                        ItemList* recipe = ItemList_Recipe(need_item,item_num);
                        ItemList_Combine(o_bl,recipe);
                        ItemList_DeleteItem(o_bl,need_item);
                        ItemList_Free(recipe);
                        il = *o_bl;
                    }
                }
            }
            lite_region_FreeNameArray(craft_list,craft_num);
            free(need_craft);
        }
        il = *o_bl;
    }
    while(1);
}

ItemList* ItemList_Recipe(char* block_name,int num)
{
    //d = opendir("recipes");
    char* name = strchr(block_name,':') + 1;
    char* directory = (char*)malloc((14 + strlen(name))*sizeof(char));
    strcpy(directory,"recipes/");
    strcat(directory,name);
    strcat(directory,".json");
    ItemList* out_recipe = NULL;
    cJSON* block_data = dhlrc_FileToJSON(directory);
    free(directory);
    if(block_data){
        char* type = cJSON_GetStringValue(cJSON_GetObjectItem(block_data,"type"));
        if(strstr(type,"minecraft:crafting_shaped"))
        {
            if(num == 0) // just return the origin name
            {
                cJSON_Delete(block_data);
                return ItemList_Init(block_name);
            }
            // get the pattern and lines of recipe
            cJSON* block_pattern = cJSON_GetObjectItem(block_data,"pattern");
            int line = cJSON_GetArraySize(block_pattern);

            // get result count
            int count_num = 1;
            if(cJSON_IsNumber(cJSON_GetObjectItem(cJSON_GetObjectItem(block_data,"result"),"count")))
                count_num = cJSON_GetNumberValue(cJSON_GetObjectItem(cJSON_GetObjectItem(block_data,"result"),"count"));

            //get craft numbers
            int craft_num = 0;
            if(num % count_num == 0) craft_num = num / count_num;
            else craft_num = num / count_num + 1;

            for(int i = 0 ; i < line ; i++)
            {
                cJSON* table = cJSON_GetArrayItem(block_pattern,i);
                char* table_content = cJSON_GetStringValue(table);
                for(int j = 0 ; j < strlen(table_content) ; j++)
                {
                    if(table_content[j] != ' ')
                    {
                        // find the item
                        char item_key[2] = {table_content[j],'\0'};
                        cJSON* key = cJSON_GetObjectItem(block_data,"key");
                        cJSON* item = cJSON_GetObjectItem(key,item_key);
                        char* item_name = NULL;
                        if(cJSON_IsArray(item))
                        {
                            item_name = cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetArrayItem(item,0),"item"));
                            if(!item_name)
                                item_name = cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetArrayItem(item,0),"tag"));
                        }
                        else
                        {
                            item_name = cJSON_GetStringValue(cJSON_GetObjectItem(item,"item"));
                            if(!item_name) item_name = cJSON_GetStringValue(cJSON_GetObjectItem(item,"tag"));
                        }
                            if(!ItemList_ScanRepeat(out_recipe,item_name))
                            {
                                if(ItemList_InitNewItem(&out_recipe,item_name))
                                {
                                    cJSON_Delete(block_data);
                                    return NULL;
                                }
                            }
                                ItemList_AddNum(&out_recipe,craft_num,item_name);
                        }
                    }
                }
            cJSON_Delete(block_data);
            if(out_recipe)
                return out_recipe;
            return NULL;
        }
        else
        {
            cJSON_Delete(block_data);
            return NULL;
        }
    }
    else return NULL;
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
    char** recipe_list = NULL;
    ItemList* ild = il;
    int recipe_num = 0;
    while(ild)
    {
        ItemList* recipe = ItemList_Recipe(ItemList_ItemName(ild),0);
        if(recipe)
        {
            recipe_num++;
            char** temp_rl = realloc(recipe_list,recipe_num * sizeof(char*));
            if(!temp_rl)
            {
                ItemList_Free(recipe);
                *num = 0;
                perror("Allocate recipe list error\n");
                lite_region_FreeNameArray(recipe_list,recipe_num);
                return NULL;
            }
            recipe_list = temp_rl;
            recipe_list[recipe_num - 1] = dh_strdup(ItemList_ItemName(ild));
            ItemList_Free(recipe);
        }
        ild = ild->next;
    }
    *num = recipe_num;
    return recipe_list;
}
