/*  path - Get Path of the game
    Copyright (C) 2024 Dream Helium
    This file is part of dhmcdir.

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

#include "path.h"
#include "../config.h"
#include "dh_file_util.h"
#include "../json_util.h"
#include "glib.h"
#include "glibconfig.h"
#include <cjson/cJSON.h>

static char* get_large_version(const char* version)
{
    /* TODO */
    return g_strdup(version);
}

char* dh_mcdir_get_translation_file(const char* gamedir, const char* version, const char* lang)
{
    char* json_in_cfg = dh_get_translation_dir();
    if(dh_file_exist(json_in_cfg)) return json_in_cfg;
    g_free(json_in_cfg);

    /* Find translation file by version */
    char* larger_version = get_large_version(version);
    char* index_file = NULL;
    if(gamedir && larger_version && dh_file_exist(gamedir))
        index_file = g_strconcat(gamedir, G_DIR_SEPARATOR_S, "assets", G_DIR_SEPARATOR_S, "indexes", G_DIR_SEPARATOR_S, larger_version, ".json", NULL);
    g_free(larger_version);
    if(dh_file_exist(index_file))
    {
        cJSON* index = dhlrc_file_to_json(index_file);
        g_free(index_file);
        cJSON* objects = cJSON_GetObjectItem(index, "objects");
        char* lang_dup = lang ? g_strdup(lang) : g_strdup("zh_cn");
        for(int i = 0 ; i < strlen(lang_dup) ; i++)
            lang_dup[i] = g_ascii_tolower(lang_dup[i]);
        char* lang_dup_last_p = NULL;
        if(strchr(lang_dup, '.'))
            lang_dup_last_p = strrchr(lang_dup, '.');
        if(lang_dup_last_p) *lang_dup_last_p = 0;
        g_message("%s", lang_dup);
        char* lang_dir = g_strconcat("minecraft/lang/", lang_dup, ".json", NULL);
        cJSON* translation_file = cJSON_GetObjectItem(objects, lang_dir);
        g_free(lang_dir);
        g_free(lang_dup);
        cJSON* hash = cJSON_GetObjectItem(translation_file, "hash");
        char* hash_name = cJSON_GetStringValue(hash);
        char hash_name_pre[3] = {};
        char* transfile = NULL;
        if(hash_name)
        {
            hash_name_pre[0] = hash_name[0];
            hash_name_pre[1] = hash_name[1];
            hash_name_pre[2] = 0;
            transfile = g_strconcat(gamedir, G_DIR_SEPARATOR_S, "assets", G_DIR_SEPARATOR_S, "objects", G_DIR_SEPARATOR_S, hash_name_pre, G_DIR_SEPARATOR_S, hash_name, NULL);
        }
        cJSON_Delete(index);
        return transfile;
    }
    else
    {
        g_free(index_file);
        return NULL;
    }
}