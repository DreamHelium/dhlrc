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
#include "dh_file_util.h"
#include "internal_config.h"
#include <cjson/cJSON.h>
#include <glib.h>

static cJSON *
file_to_json (const char *path)
{
    GFile *file = g_file_new_for_path (path);
    char *data = NULL;
    gsize len = 0;
    g_file_load_contents (file, NULL, &data, &len, NULL, NULL);


    cJSON *json = cJSON_Parse (data);
    g_object_unref (file);
    g_free (data);
    return json;
}

static char *
get_large_version (const char *version)
{
    /* TODO */
    return g_strdup (version);
}

static char *
get_pure_lang (const char *lang)
{
    char *lang_dup = lang ? g_strdup (lang) : g_strdup ("zh_cn");
    for (int i = 0; i < strlen (lang_dup); i++)
        {
            lang_dup[i] = g_ascii_tolower (lang_dup[i]);
        }
    char *point_pos = NULL;
    if ((point_pos = strchr (lang_dup, '.')) != NULL)
        *point_pos = 0;
    return lang_dup;
}

char *
dhmcdir_get_translation_file (const char *gamedir, const char *version,
                              const char *lang)
{
    const DhStrArray *arr = dhmcdir_get_translation_dir ();
    for (int i = 0; arr && i < arr->num; i++)
        if (dh_file_exist (arr->val[i]))
            return arr->val[i];

    /* Find translation file by version */
    char *lang_dup = get_pure_lang (lang);
    char *object = g_strconcat ("minecraft/lang/", lang_dup, ".json", NULL);
    char *ret = dhmcdir_get_object_dir (gamedir, version, object);
    g_free (lang_dup);
    g_free (object);
    return ret;
}

char *
dhmcdir_get_object_hash (const char *gamedir, const char *version,
                         const char *object)
{
    char *larger_version = get_large_version (version);
    char *index_file = NULL;
    char *ret = NULL;
    if (gamedir && larger_version && dh_file_exist (gamedir))
        {
            char *filename = g_strconcat (larger_version, ".json",
                                          NULL); /* Like 1.18.json */
            index_file = g_build_path (G_DIR_SEPARATOR_S, gamedir, "assets",
                                       "indexes", filename, NULL);
            g_free (filename);
        }
    g_free (larger_version);
    if (dh_file_exist (index_file))
        {
            cJSON *index = file_to_json (index_file);
            g_free (index_file);
            cJSON *objects = cJSON_GetObjectItem (index, "objects");
            cJSON *object_json = cJSON_GetObjectItem (objects, object);
            char *hash = cJSON_GetStringValue (
                cJSON_GetObjectItem (object_json, "hash"));
            ret = g_strdup (hash);
            cJSON_Delete (index);
        }
    return ret;
}

char *
dhmcdir_get_object_dir (const char *gamedir, const char *version,
                        const char *object)
{
    char *hash = dhmcdir_get_object_hash (gamedir, version, object);
    if (gamedir && version && hash)
        {
            char hash_before[3] = { hash[0], hash[1], 0 };
            char *dir = g_build_path (G_DIR_SEPARATOR_S, gamedir, "assets",
                                      "objects", hash_before, hash, NULL);
            g_free (hash);
            return dir;
        }
    return NULL;
}