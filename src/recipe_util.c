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
#include "glib.h"
#include "json_util.h"
#include <cjson/cJSON.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "config.h"
#include "dhlrc_list.h"
#include <dhutil.h>
#include "mcdir/path.h"
#include "translation.h"

static cJSON* translation_json = NULL;

static gchar* find_transfile();

/*
long* num_array_get_from_input(int* array_num, int max_num)
{
    if(!array_num) return NULL;
    DhOut* out = dh_out_new();
    DhIntArrayValidator* validator = dh_int_array_validator_new();
    dh_int_array_validator_add_range(validator, 0, max_num - 1);
    dh_int_array_validator_set_split_str(validator, " ");
    dh_int_array_validator_set_allow_repeated(validator, FALSE);
    DhArgInfo* arg = dh_arg_info_new();
    dh_arg_info_add_arg(arg, 'a', "all", N_("Choose all numbers"));
    dh_arg_info_add_arg(arg, 'q', "quit", N_("Quit application"));
    GValue val = {0};
    dh_out_read_and_output(out, N_("Please enter numbers or option: "), 
    "dhlrc", arg, DH_VALIDATOR(validator), TRUE, &val);
    g_object_unref(out);
    g_object_unref(validator);
    g_object_unref(arg);
    
    if(G_VALUE_HOLDS_POINTER(&val))
    {
        GList* list = g_value_get_pointer(&val);
        long* out = malloc(g_list_length(list) * sizeof(long));
        *array_num = g_list_length(list);
        for(int i = 0 ; i < g_list_length(list) ; i++)
        {
            gint64 tmp_val = *(gint64*)g_list_nth_data(list, i);
            out[i] = tmp_val;
        }
        g_list_free_full(list, g_free);
        g_value_unset(&val);
        return out;
    }
    else if(G_VALUE_HOLDS_CHAR(&val))
    {
        char val_c = g_value_get_schar(&val);
        if(val_c == 'a')
        {
            long* out = (long*)malloc( max_num * sizeof(long) );
            if(out)
            {
                for(int i = 0 ; i < max_num ; i++ )
                    out[i] = i;
                *array_num = max_num;
                return out;
            }
        }
        else *array_num = 0;
        return NULL;
    }
    else
    {
        *array_num = 0;
        return NULL;
    }
}
*/

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
    char* gamedir = dh_get_game_dir();
    const char* const* locales = g_get_language_names();
    char* ret = dh_mcdir_get_translation_file(gamedir, "1.18", locales[0]);
    g_free(gamedir);
    return ret;
}

int dh_exit()
{
    cJSON_Delete(translation_json);
    return 0;
}
