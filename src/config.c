/*  config - make some config
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

#include "config.h"
#include <cjson/cJSON.h>
#include <dh/dhutil.h>
#include <gio/gio.h>

static cJSON* content = NULL;

void dhlrc_make_config()
{
    gchar* config_dir = g_strconcat(g_get_user_config_dir(), "/dhlrc/", NULL);
    gchar* config_file = g_strconcat(g_get_user_config_dir(), "/dhlrc/config.json" , NULL);
    if(!dhlrc_file_exist(config_file))
    {
        gchar* minecraft_dir = g_strconcat(g_get_home_dir(), "/.minecraft", NULL);
        cJSON* config = cJSON_CreateObject();
        cJSON_AddStringToObject(config, "overrideLang", "");
        cJSON_AddStringToObject(config, "recipeConfig", "recipes/");
        cJSON_AddStringToObject(config, "itemTranslate", "translation.json");
        cJSON_AddStringToObject(config, "gameDir", minecraft_dir);
        char* config_text = cJSON_Print(config);
        printf("%s\n", config_file);
        g_free(minecraft_dir);

        GFile* dir = g_file_new_for_path(config_dir);
        g_file_make_directory_with_parents(dir, NULL, NULL);
        g_object_unref(dir);
        g_free(config_dir);

        GFile* file = g_file_new_for_path(config_file);
        GFileIOStream* ios = g_file_create_readwrite(file, G_FILE_CREATE_REPLACE_DESTINATION, NULL, NULL);
        printf("%d\n", ios == NULL);
        GOutputStream* os = g_io_stream_get_output_stream(G_IO_STREAM(ios));
        g_output_stream_write(os, config_text, strlen(config_text), NULL, NULL);
        g_object_unref(ios);
        g_object_unref(file);

        content = config;
        free(config_text);
        g_free(config_file);
    }
    else
    {
        g_free(config_dir);
        g_free(config_file);

    }
}

void dh_exit1()
{
    cJSON_Delete(content);
}

char* dh_get_game_dir()
{
    if(!content)
        dhlrc_reread_config();
    cJSON* gamedir_obj = cJSON_GetObjectItem(content, "gameDir");
    if(gamedir_obj)
        return cJSON_GetStringValue(gamedir_obj);
    else return NULL;
}

void dhlrc_reread_config()
{
    cJSON_Delete(content);
    gchar* config_file = g_strconcat(g_get_user_config_dir(), "/dhlrc/config.json" , NULL);
    content = dhlrc_file_to_json(config_file);
    g_free(config_file);
}
