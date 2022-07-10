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
#include "dh_string_util.h"

long* NumArray_GetFromInput(int* array_num, int max_num)
{
    long* array = NULL;
    int i = 0;
    char* input = NULL;
    size_t len = 0;
    printf("Input nums directly, or type 'a' for all numbers: ");
    while(dh_string_getline(&input,&len,stdin) != -1)
    {
        char* inputl = input;
        while(*inputl == ' ')
            inputl++;
        if(*inputl == 'a' || *inputl == 'A')
        {
            char* after_a = inputl+1;
            while(*after_a == ' ')
                after_a++;
            if(*after_a == '\n')
            {
                array = (long*)malloc(max_num * sizeof(long));
                *array_num = max_num;
                for(int i = 0 ; i < max_num ; i++)
                    array[i] = i;
                free(input);
                return array;
            }
            else
                printf("Unrecognized string, please enter again: ");
        }
        else if(*inputl >= '0' && *inputl <= '9')
        { // when input is num
            while(1)
            {
                char* end;
                long value = strtol(inputl,&end,10);
                if(inputl == end) break;

                long* parray = realloc(array,(i+1)*sizeof(long));
                if(!parray)
                {
                    free(array);
                    free(input);
                    *array_num = 0;
                    return NULL;
                }
                array = parray;
                while(*end == ' ')
                    end++;
                for(int j = 0 ; j < i ; j++)
                {
                    if( value == array[j] )
                    {
                        free(input);
                        free(array);
                        printf("Repeated number detected!\n");
                        return NumArray_GetFromInput(array_num, max_num);
                    }
                }
                if((value < 0 || value >= max_num) || !(*end == '\n' || isdigit(*end)) )
                {
                // hopefully this will work
                    free(input);
                    free(array);
                    printf("Out of range! \n");
                    return NumArray_GetFromInput(array_num, max_num);
                }
                array[i] = value;
                //printf("%ld ",value);
                inputl = end;
                i++;
            }
            free(input);
            *array_num = i;
            return array;
        }
        else if(*inputl == '\n')
        {
            free(input);
            *array_num = 0;
            return NULL;
        }
        else
        {
            printf("Unrecognized string, please enter again: ");
        }
    }
    free(input);
    *array_num = 0;
    return NULL;
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
    FILE* f = fopen(directory,"rb");
    free(directory);
    if(f)
        {
            fseek(f,0,SEEK_END);
            int size = ftell(f);
            fseek(f,0,SEEK_SET);
            char* data = (char*)malloc(size* sizeof(char));
            fread(data,1,size,f);
            fclose(f);

            cJSON* block_data = cJSON_ParseWithLength(data,size);
            free(data);
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
                                    ItemList_AddNum(out_recipe,craft_num,item_name);
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
    else
    {
        //printf("Couldn't find recipes, did you put the recipe in?\n");
        return NULL;
    }
}

char* Name_BlockTranslate(const char *block_name)
{
    FILE* f = fopen("translation.json","rb");
    if(f)
    {
        // read file
        fseek(f,0,SEEK_END);
        int size = ftell(f);
        fseek(f,0,SEEK_SET);
        char* data = (char*)malloc(size* sizeof(char));  // origin data
        fread(data,1,size,f);
        fclose(f);
        // end of reading file

        cJSON* trans_data = cJSON_ParseWithLength(data,size);
        free(data);

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
    else
    {
        return NULL;
    }
}

char** NameArray_CanCraft(int* num, ItemList *il)
{
    char** recipe_list = NULL;
    ItemList* ild = il;
    int recipe_num = 0;
    while(ild)
    {
        ItemList* recipe = ItemList_Recipe(ild->name,0);
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
            recipe_list[recipe_num - 1] = (char*)malloc(ild->len * sizeof(char));
            strcpy(recipe_list[recipe_num - 1],ild->name);
            ItemList_Free(recipe);
        }
        ild = ild->next;
    }
    *num = recipe_num;
    return recipe_list;
}
