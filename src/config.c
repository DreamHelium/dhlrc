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
#include "json_util.h"

/* Unused until I get understand of monitor */
static cJSON* content = NULL;
/* Now it's a temporary value */
static DhStrArray* settings_strv = NULL;
static DhStrArray* value_strv = NULL;
static gchar* config_file = NULL;

static cJSON* get_content();

void dhlrc_make_config()
{
    config_file = g_strconcat(g_get_user_config_dir(), "/dhlrc/config.json" , NULL);
    if(!dh_file_exist(config_file))
    {
#ifdef __APPLE__
        gchar* minecraft_dir = g_strconcat(g_get_home_dir(), "/Library/Application Support/minecraft", NULL);
#elif defined G_OS_WIN32
        gchar* minecraft_dir = g_strconcat(g_getenv("APPDATA"), "\\.minecraft", NULL);
#else
        gchar* minecraft_dir = g_strconcat(g_get_home_dir(), "/.minecraft", NULL);
#endif

        gchar* cache_dir = g_strconcat(g_get_user_cache_dir(), "/dhlrc", NULL);

        cJSON* config = cJSON_CreateObject();
        cJSON_AddStringToObject(config, "cacheDir", cache_dir);
        cJSON_AddStringToObject(config, "overrideLang", "");

        cJSON_AddStringToObject(config, "recipeConfig", "recipes/");
        cJSON_AddStringToObject(config, "itemTranslate", "translation.json");
        cJSON_AddStringToObject(config, "overrideVersion", "1.18.2");
        cJSON_AddStringToObject(config, "gameDir", minecraft_dir);
        char* config_text = cJSON_Print(config);
        printf("%s\n", config_file);
        g_free(minecraft_dir);
        g_free(cache_dir);

        dh_write_file(config_file, config_text, strlen(config_text));

        /* content = config; */
        free(config_text);
    }
}

void dh_exit1()
{
    /* cJSON_Delete(content); */
    g_free(config_file);
}

char* dh_get_game_dir()
{
    cJSON* content_d = get_content();
    cJSON* gamedir_obj = cJSON_GetObjectItem(content_d, "gameDir");
    char* ret = NULL;
    if(gamedir_obj)
        ret = g_strdup(cJSON_GetStringValue(gamedir_obj));
    else ret = NULL;
    cJSON_Delete(content_d);
    return ret;
}

char* dh_get_cache_dir()
{
    cJSON* content_d = get_content();
    cJSON* cachedir_obj = cJSON_GetObjectItem(content_d, "cacheDir");
    char* ret = NULL;
    if(cachedir_obj)
        ret = g_strdup(cJSON_GetStringValue(cachedir_obj));
    else ret = g_strconcat(g_get_user_cache_dir(), "/dhlrc", NULL);
    cJSON_Delete(content_d);
    return ret;
}

char* dh_get_version()
{
    cJSON* content_d = get_content();
    cJSON* version_obj = cJSON_GetObjectItem(content_d, "overrideVersion");
    char* ret = NULL;
    if(version_obj)
        ret = g_strdup(cJSON_GetStringValue(version_obj));
    else ret = g_strdup("1.18.2");
    cJSON_Delete(content_d);
    return ret;
}

char* dh_get_recipe_dir()
{
    cJSON* content_d = get_content();
    cJSON* recipe_obj = cJSON_GetObjectItem(content_d, "recipeConfig");
    char* ret = NULL;
    if(recipe_obj)
    {
        ret = g_strdup(cJSON_GetStringValue(recipe_obj));
        if(*ret == 0)
            goto no_recipe_dir;
    }
    else
no_recipe_dir:
    {
        char* version = dh_get_version();
        char* cache_dir = dh_get_cache_dir();
        ret = g_strconcat(cache_dir, "/", version, "/extracted/data/minecraft/recipes/", NULL);
    }
    cJSON_Delete(content_d);
    return ret;
}

void dhlrc_reread_config()
{
    cJSON_Delete(content);
    content = dhlrc_file_to_json(config_file);
}

static cJSON* get_content()
{
    dhlrc_make_config();
    return dhlrc_file_to_json(config_file);
}
