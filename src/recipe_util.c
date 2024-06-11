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
#include "json_util.h"
#include <cjson/cJSON.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "config.h"
#include "dhlrc_list.h"
#include <dhutil.h>
#include "translation.h"

static cJSON* translation_json = NULL;

static gchar* find_transfile();

long* num_array_get_from_input(int* array_num, int max_num)
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

static int ItemList_CombineRecipe_ng(ItemList** il, const char* dirpos, DhGeneral* general)
{
    gboolean processing = TRUE;
    while(processing)
    {
        /* Get item names */
        RecipeList* rcl = recipe_list_init(dirpos, *il);
        DhStrArray* item_names = recipe_list_item_names_with_namespace(rcl);

        /* If no items need processing just return */
        if(item_names == NULL)
            return 1;
        for(int i = 0 ; i < item_names->num; i++)
        {
single_process_start:
            dh_new_win(general, FALSE);
            dh_printf(general, _("Processing %s with %d items, continue or read item list? [Y/n/q/r] :"),
                      trm(item_names->val[i]), item_list_get_item_num(*il, item_names->val[i]));
            int option = dh_selector(general, "", 0, "Ynqr", _("&Yes"), _("&No"), _("&Quit"), _("&Read"));

            if(option == 0 || option == -1)
            {
                char* item_name = (item_names->val)[i];
                ItemList* return_recipe = item_list_recipe(rcl, item_list_get_item_num(*il, item_name),item_name, general);
                if(return_recipe)
                {
                    item_list_combine(il, return_recipe);
                    item_list_delete_item(il, item_name);
                    item_list_free(return_recipe);
                }
            }
            else if(option == -3 || option == -100)
            {
                recipe_list_free(rcl);
                dh_str_array_free(item_names);
                return 1;
            }
            else if(option == -2)
                ;
            else if(option == -4)
            {
                item_list_read(*il, general);
                goto single_process_start;
            }
        }
        recipe_list_free(rcl);
        dh_str_array_free(item_names);
    }
    return 1;
}

int item_list_combine_recipe(ItemList** o_bl, const char* dirpos, DhGeneral* general)
{
    return ItemList_CombineRecipe_ng(o_bl, dirpos, general);
    // /* Get item names */
    // RecipeList* rcl = RecipeList_Init(dirpos, *o_bl);
    // dh_StrArray* item_names = RecipeList_ItemNamesWithNamespace(rcl);
    //
    // /* TODO: A newer function for combine recipe */
    // dh_printf(general, _("There are some items to craft:\n"));
    // for(int i = 0 ; i < item_names->num; i++)
    // {
    //     dh_option_printer(general, i,"%s, %d" ,trm((item_names->val)[i]), ItemList_GetItemNum(*o_bl, item_names->val[i]));
    // }
    //
    // int arr_num = 0;
    // long* num_arr = NumArray_GetFromInput(&arr_num, item_names->num);
    // if(num_arr == NULL)
    // {
    //     RecipeList_Free(rcl);
    //     dh_StrArray_Free(item_names);
    //     return 0;
    // }
    //
    // for(int i = 0 ; i < arr_num ; i++)
    // {
    //     char* item_name = (item_names->val)[num_arr[i]];
    //     ItemList* return_recipe = ItemList_Recipe(rcl, ItemList_GetItemNum(*o_bl, item_name),item_name, general);
    //     if(return_recipe)
    //     {
    //         ItemList_Combine(o_bl, return_recipe);
    //         ItemList_DeleteItem(o_bl, item_name);
    //         ItemList_Free(return_recipe);
    //     }
    // }
    // free(num_arr);
    // RecipeList_Free(rcl);
    // dh_StrArray_Free(item_names);
    // return 1;
}

const char* name_block_translate(const char *block_name)
{
    if(!translation_json)
    {
        gchar* filename = find_transfile();
        cJSON* trans_data = dhlrc_file_to_json(filename);
        translation_json = trans_data;
        g_free(filename);
    }
    if(!translation_json)
        return block_name;

    char* pure_name = strchr(block_name,':') + 1;

    int len = strlen(pure_name) + 1 + 5 + 10;

    char* origin_name = (char*) malloc(len * sizeof(char));
    strcpy(origin_name,"item.minecraft.");
    strcat(origin_name,pure_name);
    cJSON* trans_line = cJSON_GetObjectItem(translation_json,origin_name);
    if(cJSON_IsString(trans_line))
    {
        char* trans_name = cJSON_GetStringValue(trans_line);
        free(origin_name);

        return trans_name;
    }
    else
    {
        len++;
        free(origin_name);
        origin_name = NULL;
        origin_name = (char*)malloc(len * sizeof(char));
        strcpy(origin_name,"block.minecraft.");
        strcat(origin_name,pure_name);
        trans_line = cJSON_GetObjectItem(translation_json,origin_name);

        if(cJSON_IsString(trans_line))
        {
            char* trans_name = cJSON_GetStringValue(trans_line);
            free(origin_name);

            return trans_name;
        }
        else
        {
            free(origin_name);

            return block_name;
        }
    }
}

static gchar* find_transfile()
{
    if(dh_file_exist("translation.json"))
        return g_strdup("translation.json");
    else
    {
        gchar* game_dir = NULL;
        gchar* index_file = NULL;
        /* Analyse .minecraft filepos */
        if(dh_get_game_dir())
        {
            game_dir = dh_get_game_dir();
            index_file = g_strconcat(game_dir, "/assets/indexes/1.18.json" , NULL);
        }
        else index_file = g_strconcat(g_get_home_dir(), "/.minecraft/assets/indexes/1.18.json", NULL);
        if(dh_file_exist(index_file))
        {
            cJSON* index = dhlrc_file_to_json(index_file);
            g_free(index_file);
            /* Analyse index file */
            cJSON* objects = cJSON_GetObjectItem(index, "objects");
            cJSON* translation_file = cJSON_GetObjectItem(objects, "minecraft/lang/zh_cn.json");
            cJSON* hash = cJSON_GetObjectItem(translation_file, "hash");
            gchar* hash_name = cJSON_GetStringValue(hash);
            gchar hash_name_pre[3] = {hash_name[0], hash_name[1] ,0};
            gchar* transfile = g_strconcat(g_get_home_dir(), "/.minecraft/assets/objects/", hash_name_pre, "/", hash_name, NULL);
            cJSON_Delete(index);
            return transfile;
        }
        else
        {
            g_free(index_file);
            return NULL;
        }
    }
}

int dh_exit()
{
    cJSON_Delete(translation_json);
    return 0;
}
