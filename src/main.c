/*  litematica_reader_c - litematic file reader in C
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

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
/*#include "dhlrc_config.h"*/
#include "config.h"
#include "il_info.h"
#include "libnbt/nbt.h"
#include "main.h"
#include "dh_file_util.h"
#include "dh_validator.h"
#include "nbt_info.h"
#include "region_info.h"
#include "translation.h"
#include <dhutil.h>

static gboolean reader_mode = FALSE;
static gboolean block_mode = FALSE;
static gboolean list_mode = FALSE;
gchar* log_filename = NULL;
static guint mode_num = 0;

static gchar* get_filename();

static GOptionEntry entries[] =
{
    {"reader", 'r', 0, G_OPTION_ARG_NONE, &reader_mode, N_("Enter NBT reader mode."), NULL},
    {"block", 'b', 0, G_OPTION_ARG_NONE, &block_mode, N_("Enter litematica block reader."), NULL},
    {"list", 'l', 0, G_OPTION_ARG_NONE, &list_mode, N_("Enter litematica material list with recipe combination."), NULL},
    {"log", 0, 0, G_OPTION_ARG_FILENAME, &log_filename, N_("Output log file to FILE"), "FILE"}
};

static void startup(GApplication* self, gpointer user_data)
{
    dhlrc_make_config();
    il_info_list_init();
    nbt_info_list_init();
    region_info_list_init();
}

static void app_shutdown(GApplication* self, gpointer user_data)
{
    il_info_list_free();
    nbt_info_list_free();
    region_info_list_free();
}

static int start_point()
{
    printf(_("The functions are listed below:\n"));
    printf("[0] %s\n", _("Manage NBT"));
    printf("[1] %s\n", _("Manage Region"));
    printf("[2] %s\n", _("Manage item list"));
    printf("[3] %s\n", _("Config settings"));
    DhOut* out = dh_out_new();
    DhArgInfo* arg = dh_arg_info_new();
    dh_out_set_show_opt(out, TRUE);
    dh_arg_info_add_arg(arg, 'n', "mnbt", N_("Manage NBT"));
    dh_arg_info_add_arg(arg, 'r', "mregion", N_("Manage Region"));
    dh_arg_info_add_arg(arg, 'i', "mitem", N_("Manage item list"));
    dh_arg_info_add_arg(arg, 'c', "config", N_("Config settings"));
    dh_arg_info_add_arg(arg, 'q', "quit", N_("Quit application"));
    dh_arg_info_change_default_arg(arg, 'q');
    DhIntValidator* validator = dh_int_validator_new(0, 3);
    
    GValue val = {0};
    int ret_val = -1;
    char ret_val_c = 0;
    dh_out_read_and_output(out, N_("Please enter a number or an option"), "dhlrc", arg, DH_VALIDATOR(validator), FALSE, &val);

    if(G_VALUE_HOLDS_INT64(&val)) ret_val = g_value_get_int64(&val);
    else if(G_VALUE_HOLDS_CHAR(&val)) ret_val_c = g_value_get_schar(&val);
    else return -1;

    g_message("%d", ret_val);
    g_message("%d", ret_val_c);
    g_object_unref(out);
    g_object_unref(arg);
    g_object_unref(validator);
    return 0;
}

static gboolean file_open(GFile* file, gboolean single)
{
    gboolean ret = FALSE;
    if(!g_file_query_exists(file, NULL))
    {
        if(single) g_critical(_("Not a valid NBT file!"));
        return ret;
    }
    char* filename = g_file_get_basename(file); /* Should be freed */
    char* description = NULL;
    if(single)
    {
        DhOut* out = dh_out_new();
        GValue val = {0};
        dh_out_read_and_output(out, N_("Enter desciption for the NBT file."), "dhlrc", NULL, NULL, FALSE, &val);
        if(G_VALUE_HOLDS_STRING(&val))
        {
            description = g_value_get_string(&val);
        }
        else description = g_strdup(filename);
        g_free(filename);
    }
    else description = filename;
    if(description)
    {
        guint8* content = NULL;
        gsize len;
        g_file_load_contents(file, NULL, (char**)&content, &len, NULL, NULL);
        NBT* root = NBT_Parse(content, len);
        if(root)
        {
            nbt_info_new(root, g_date_time_new_now_local(), description);
            ret = TRUE;
        }
        else if(single)
            g_critical(_("Not a valid NBT file!"));
        g_free(description);
        g_free(content);
    }
    return ret;
}

static void app_open(GApplication* self, gpointer files, gint n_files, gchar* hint, gpointer user_data)
{
    GFile** f = files;
    DhStrArray* arr = NULL;
    if(n_files == 1)
        file_open(f[0], TRUE);
    else
    {
        for(int i = 0 ; i < n_files ; i++)
        {
            gboolean success = file_open(f[i], FALSE);
            if(!success)
            {
                char* file_basename = g_file_get_basename(f[i]);
                dh_str_array_add_str(&arr, file_basename);
                g_free(file_basename);
            }
        }
    }
    if(n_files > 1 && arr)
    {
        g_critical(_("The following files couldn't be added:\n"));
        for(int i = 0 ; i < arr->num ; i++)
            g_critical("%s", arr->val[i]);
    }
    dh_str_array_free(arr);
    start_point();
}

static void activate(GApplication* self, gpointer user_data)
{
    start_point();
}

int main(int argc, char** argb)
{
    translation_init();
    GApplication* app = g_application_new("cn.dh.dhlrc.cli", G_APPLICATION_HANDLES_OPEN);
    g_signal_connect(app, "startup", G_CALLBACK(startup), NULL);
    g_signal_connect(app, "shutdown", G_CALLBACK(app_shutdown), NULL);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    g_signal_connect(app, "open", G_CALLBACK(app_open), NULL);
    g_application_add_main_option_entries(app, entries);
    g_application_set_option_context_parameter_string(app, _("[FILE] - Read a litematic file."));

    int ret = g_application_run(app, argc, argb);

    g_object_unref(app);
    return ret;
}

static gchar* get_filename()
{
    gchar* filename = NULL;
    gchar* pure_filename = NULL;
    gchar* game_dir = dh_get_game_dir();
    gchar* schematics_dir = NULL;
    if(game_dir)
        schematics_dir = g_strconcat(game_dir, "/schematics", NULL);
    g_free(game_dir);
    while (1 && schematics_dir) 
    {
        GList* file_list = dh_file_list_create(schematics_dir);
        if(file_list)
        {
            GList* file_list_d = file_list;
            DhOut* out = dh_out_new();
            dh_out_no_output_string_while_no_validator(out);
            dh_out_output_match_string_than_arg(out);
            DhArgInfo* arg = dh_arg_info_new();
            for(int i = 0 ; i < g_list_length(file_list_d) ; i++)
            {
                gchar* filename_d = g_strconcat(schematics_dir, "/", g_list_nth_data(file_list_d, i) ,NULL);
                GFile* file = g_file_new_for_path(filename_d);
                GFileType type = g_file_query_file_type(file, G_FILE_QUERY_INFO_NONE , NULL);
                gchar* type_name = NULL;
                if(type == G_FILE_TYPE_DIRECTORY)
                    type_name = N_("directory");
                else type_name = N_("file");
                dh_arg_info_add_arg(arg, 0, g_list_nth_data(file_list_d, i), type_name);
                g_free(filename_d);
                g_object_unref(file);
            }
            dh_arg_info_add_arg(arg, 0, "..", "directory");
            GValue val = {0};
            printf("Current directory is %s.\n", schematics_dir);
            dh_out_read_and_output(out, "Please enter the filename or directory name: ", "dhlrc", arg, NULL, FALSE, &val);
            g_object_unref(out);
            g_object_unref(arg);
            if(G_VALUE_HOLDS_STRING(&val))
            {
                #ifdef GOBJECT_AVAILABLE_IN_2_80
                pure_filename = g_value_steal_string(&val);
                #else
                pure_filename = g_value_get_string(&val);
                #endif
            }
            else 
            {
                /* TODO: key binding */
                break;
            }
            gchar* tmp_filename = NULL;
            if(pure_filename)
                tmp_filename = g_strconcat(schematics_dir, "/", pure_filename, NULL);
            g_free(pure_filename);
            gboolean tmp_file_is_directory = dh_file_is_directory(tmp_filename);
            if(tmp_file_is_directory)
            {
                g_free(schematics_dir);
                schematics_dir = tmp_filename;
            }
            else 
            {
                filename = tmp_filename;
                break;
            }
        }
        else break;
    }
    g_free(schematics_dir);
    return filename;
}
