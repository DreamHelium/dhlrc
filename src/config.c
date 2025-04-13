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
#include <gio/gio.h>
#include <string.h>
#include "glib.h"
#include "dh_file_util.h"

static cJSON* content = NULL;

static gchar* config_file = NULL;
static GTask* task = NULL;
static GFile* file = NULL;
static GMainLoop* loop = NULL;

static void file_changed_cb()
{
    /* Reload file */
    cJSON_Delete(content);
    gchar* content_val = NULL;
    gboolean get_file = g_file_load_contents(file, NULL, &content_val, NULL, NULL , NULL);
    if(!get_file) g_error("Get file failed!");
    content = cJSON_Parse(content_val);
    g_free(content_val);
}

static void monitor_task(GTask* task, gpointer source_object, gpointer task_data, GCancellable* cancellable)
{
    GFile* file = source_object;
    GFileMonitor* monitor = g_file_monitor_file(file, G_FILE_MONITOR_NONE,NULL, NULL);
    if(monitor)
    {
        loop = g_main_loop_new(NULL, FALSE);
        g_signal_connect(monitor, "changed", G_CALLBACK(file_changed_cb), NULL);
        /* Make the "content" */
        gchar* content_val = NULL;
        gboolean get_file = g_file_load_contents(file, NULL, &content_val, NULL, NULL , NULL);
        if(!get_file) g_error("Get file failed!");
        content = cJSON_Parse(content_val);
        g_free(content_val);
        g_main_loop_run(loop);

        g_object_unref(monitor);
        g_main_loop_unref(loop);
        g_task_return_boolean(task, TRUE);
    }
    else
    {
        g_error("The file monitor is not created correctly!");
        g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed!");
    }
}

void dhlrc_make_config()
{
    if(!config_file)
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

        gchar* cache_dir = g_strconcat(g_get_user_cache_dir(), G_DIR_SEPARATOR_S ,"dhlrc", NULL);

        cJSON* config = cJSON_CreateObject();
        cJSON_AddStringToObject(config, "cacheDir", cache_dir);
        cJSON_AddStringToObject(config, "overrideLang", "");

        gchar* recipe_dir = g_strconcat("recipes", G_DIR_SEPARATOR_S ,NULL);
        cJSON_AddStringToObject(config, "recipeConfig", recipe_dir);
        cJSON_AddStringToObject(config, "itemTranslate", "translation.json");
        cJSON_AddStringToObject(config, "overrideVersion", "1.18.2");
        cJSON_AddStringToObject(config, "gameDir", minecraft_dir);
        char* config_text = cJSON_Print(config);
        printf("%s\n", config_file);
        g_free(minecraft_dir);
        g_free(cache_dir);
        g_free(recipe_dir);

        dh_write_file(config_file, config_text, strlen(config_text));

        /* content = config; */
        free(config_text);
    }

    file = g_file_new_for_path(config_file);
    task = g_task_new(file, NULL, NULL, NULL);
    g_task_run_in_thread(task, monitor_task);
}

char* dh_get_config_item(const char* item)
{
    cJSON* obj = cJSON_GetObjectItem(content, item);
    char* ret = NULL;
    if(obj)
    {
        char* val = cJSON_GetStringValue(obj);
        if(val && *val != 0) ret = g_strdup(val);
    }
    if(ret)
        return ret;
    else return g_strdup("");
}

void dh_exit1()
{
    dh_rm_cache_dir();
    cJSON_Delete(content);
    g_free(config_file);
    g_main_loop_quit(loop);
    g_object_unref(task);
    g_object_unref(file);
}

char* dh_get_translation_dir()
{
    return dh_get_config_item("itemTranslate");
}

char* dh_get_game_dir()
{
    return dh_get_config_item("gameDir");
}

char* dh_get_cache_dir()
{
    char* ret = dh_get_config_item("cacheDir");
    if(ret) return ret;
    else return g_strconcat(g_get_user_cache_dir(), "/dhlrc", NULL);
}

char* dh_get_version()
{
    char* ret = dh_get_config_item("overrideVersion");
    if(ret) return ret;
    else return g_strdup("1.18.2");
}

char* dh_get_recipe_dir()
{
    return dh_get_config_item("recipeConfig");
}

char* dh_get_assets_dir()
{
    return dh_get_config_item("assetsDir");
}

gboolean dh_get_show_wizard()
{
    char* ret = dh_get_config_item("showWizardOnStart");
    if(g_str_equal(ret, "false")) return FALSE;
    else return TRUE;
}

void dh_set_or_create_item(const char* item, const char* val, gboolean save)
{
    cJSON* json_item = cJSON_GetObjectItem(content, item);
    if(json_item)
        cJSON_SetValuestring(json_item, val);
    else
    {
        cJSON_AddStringToObject(content, item, val);
    }
    if(save)
    {
        char* data = cJSON_Print(content);
        dh_write_file(config_file, data, strlen(data));
        free(data);
    }
}

void dh_rm_cache_dir()
{
    char* cache_dir = dh_get_cache_dir();
    GList* all_file = dh_file_list_create_recursive(cache_dir);
    g_free(cache_dir);

    GList* tmp_list = all_file;
    for(; tmp_list ; tmp_list = tmp_list->next)
        dh_file_rm_file(tmp_list->data);

    g_list_free_full(all_file, free);
}