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
#include "config.h"
#include "glib.h"
#include "feature/mcdata_feature.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dh_file_util.h"

static gboolean first_try = FALSE;
static gchar *find_transfile ();

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

const char *
name_block_translate (const char *block_name)
{
    // if(!dhlrc_has_translation () && !first_try) /* No translation json and
    // not try */
    // {
    //     first_try = TRUE;
    //     dhlrc_update_transfile ("1.18", NULL, NULL);
    // }
    //
    // const char* trans_name = dhlrc_get_translation(block_name);
    return block_name;
}

static gchar *
find_transfile (const char *version, SetFunc set_func, void *klass,
                char **large_version)
{
    char *gamedir = dh_get_game_dir ();
    char *ret = NULL;
    if (strchr (gamedir, ':'))
        g_error ("Multiple game directory is not supported!");
    /* OverrideLang is deprecated. */
    // char* override_lang = dh_get_override_lang();
    const char *real_lang = NULL;
    const char *const *locales = g_get_language_names ();
    const char *locale_lang = locales[0];
    real_lang = locale_lang;

    *large_version
        = dhlrc_get_version_json_string (version, set_func, klass, 0, 75);

    if (!*large_version)
        {
            *large_version = g_strdup ("1.18");
            g_message ("Large version not found, fall back to 1.18.");
        }

    ret = dhlrc_get_translation_file (gamedir, *large_version, real_lang);
    if (!ret || !dh_file_exist (ret))
        {
            g_free (ret);
            ret = NULL;
        }
    else if (set_func && klass)
        set_func (klass, 100);
    g_free (gamedir);
    return ret;
}

gboolean
dhlrc_found_transfile ()
{
    /* Try to initialize the file. */
    const char *tr = name_block_translate ("minecraft:air");
    if (g_str_equal (tr, "minecraft:air"))
        return FALSE;
    return TRUE;
}

typedef struct UpdateInfo
{
    char *version;
    SetFunc set_func;
    void *klass;
    char **large_version;
} UpdateInfo;

static void
update_info_free (gpointer data)
{
    UpdateInfo *info = data;
    g_free (info->version);
    g_free (info);
}

static void
real_update_transfile (GTask *task, gpointer source_object, gpointer task_data,
                       GCancellable *cancellable)
{
    UpdateInfo *data = task_data;
    char *version = data->version;
    SetFunc set_func = data->set_func;
    void *klass = data->klass;
    char **large_version = data->large_version;
    gchar *filename = find_transfile (version, set_func, klass, large_version);
    /* The en_US translation is in the jar file, so ...... */
    if (filename)
        dhlrc_init_translation_from_file (filename, *large_version);
    g_free (filename);
}

void
dhlrc_update_transfile (const char *version, SetFunc set_func, void *klass,
                        char **large_version)
{
    GTask *task = g_task_new (NULL, NULL, NULL, NULL);
    UpdateInfo *info = g_new0 (UpdateInfo, 1);
    info->version = g_strdup (version);
    info->set_func = set_func;
    info->klass = klass;
    info->large_version = large_version;
    g_task_set_task_data (task, info, update_info_free);
    g_task_run_in_thread (task, real_update_transfile);

    g_object_unref (task);
}