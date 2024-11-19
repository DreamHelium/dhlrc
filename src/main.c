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
#include "dh_nc_rl.h"
#include "dh_string_util.h"
#include "glibconfig.h"
#include "gmodule.h"
#include "il_info.h"
#include "libnbt/nbt.h"
#include "dh_file_util.h"
#include "dh_validator.h"
#include "nbt_info.h"
#include "region_info.h"
#include "translation.h"
#include <dhutil.h>
#include <ncursesw/ncurses.h>

static gboolean reader_mode = FALSE;
static gboolean block_mode = FALSE;
static gboolean list_mode = FALSE;
gchar* log_filename = NULL;
static guint mode_num = 0;

static gchar* get_filename();

typedef int (*DhlrcMainFunc)(int argc, char** argv);
typedef const char* (*DhlrcGetModuleName)();
typedef DhStrArray* (*DhlrcGetModuleNameArray)();
typedef const char* (*DhlrcGetModuleDescription)();
typedef const char* (*DhlrcGetModuleHelpDescription)();

typedef struct DhlrcModule{
    const char* module_name;
    DhStrArray* module_name_full;

    const char* module_description;
    const char* help_description;
    DhlrcMainFunc start_point;
    GModule* module;
} DhlrcModule;

static void debug(int argc, char** argv)
{
    
}

static void modules_free(DhlrcModule* modules, int len)
{
    for(int i = 0 ; i < len ; i++)
    {
        dh_str_array_free(modules[i].module_name_full);
        g_module_close(modules[i].module);
    }
    g_free(modules);
}

static DhlrcModule* get_module(int* module_num)
{
    int real_module_num = 0;
    GList* module_files = dh_file_list_create("module");
    DhlrcModule* ret = NULL;
    gboolean succeed = TRUE;
    for(int i = 0 ; i < g_list_length(module_files) ; i++)
    {
        char* dir = g_strconcat("module", G_DIR_SEPARATOR_S, g_list_nth_data(module_files, i), NULL);
        GModule* module = g_module_open(dir, G_MODULE_BIND_MASK);
        g_free(dir);

        if(!module) continue; /* Get Module fail */
        DhlrcGetModuleName name;
        DhlrcGetModuleNameArray arr;
        DhlrcGetModuleDescription des;
        DhlrcGetModuleHelpDescription helpdes;
        DhlrcMainFunc main_func;

        if(succeed) succeed = g_module_symbol(module, "module_name", (gpointer*)&name);
        if(succeed) succeed = g_module_symbol(module, "module_name_array", (gpointer*)&arr);
        if(succeed) succeed = g_module_symbol(module, "module_description", (gpointer*)&des);
        if(succeed) succeed = g_module_symbol(module, "help_description", (gpointer*)&helpdes);
        if(succeed) succeed = g_module_symbol(module, "start_point", (gpointer*)&main_func);

        if(succeed)
        {
            ret = g_realloc(ret, (real_module_num + 1) * sizeof(DhlrcModule));
            ret[real_module_num].module_name = name();
            ret[real_module_num].module_name_full = arr();
            ret[real_module_num].module_description = des();
            ret[real_module_num].help_description = helpdes();
            ret[real_module_num].start_point = main_func;
            ret[real_module_num].module = module;
            real_module_num++;
        }
        succeed = TRUE; /* Reset succeed. */
    }
    *module_num = real_module_num;
    g_list_free_full(module_files, free);
    return ret;
}

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

static DhStrArray* modules(DhlrcModule* module, int len)
{
    DhStrArray* arr = NULL;
    for(int i = 0 ; i < len ; i++)
        dh_str_array_add_str(&arr, module[i].module_name);
    return arr;
}

static gboolean get_module_pos(DhlrcModule* modules, int len, int* pos, const char* module)
{
    for(int i = 0 ; i < len ; i++)
    {
        const char* module_name = modules[i].module_name;
        DhStrArray* module_arr = modules[i].module_name_full;
        if(module_name && g_str_equal(module_name, module))
        {
            *pos = i;
            return TRUE;
        }
        else if(module_arr && dh_str_array_find_repeated(module_arr, module))
        {
            *pos = i;
            return TRUE;
        }
    }
    return FALSE;
}

static gint run_app (GApplication* self, GApplicationCommandLine* command_line, gpointer user_data)
{
    if(!g_module_supported())
    {
        printf("The program is not supported!\n");
        return -1;
    }

    gchar **argv;
    gint argc;
    gint i;

    argv = g_application_command_line_get_arguments (command_line, &argc);

    // debug(argc, argv);
    int len = 0;
    int module_pos = 0;
    DhlrcModule* modules = get_module(&len);
    int ret = 0;

    if((argc >= 2 && g_str_equal(argv[1], "--help")) || argc == 1)
    {
        if(argc == 2 || argc == 1)
        {
            printf(_("dhlrc - Program to handle litematic or other struct of Minecraft.\n"));
            printf("\n");
            printf(_("Modules:\n"));
            
            for(int i = 0 ; i < len ; i++)
            {
                if(modules[i].module_name) 
                {   
                    printf("%s\t", modules[i].module_name);
                    printf("%s\n", modules[i].module_description);
                }
                else
                {
                    DhStrArray* arr = modules[i].module_name_full;
                    for(int j = 0 ; j < arr->num ; j++)
                    {
                        printf("%s\t%s\n", arr->val[j], modules[i].module_description);
                    }
                }
            }
            printf("\n");
        }
        else
        {
            printf(_("Unrecognized options "));
            for(int i = 2 ; i < argc; i++)
                printf("\"%s\"", argv[i]);
            printf(".\n");
        }
    }
    else if(argc >= 2 && get_module_pos(modules, len, &module_pos, argv[1]))
    {
        ret = modules[module_pos].start_point(argc - 1 , argv + 1);
    }
    else printf("Not supported!\n");


    g_strfreev (argv);

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
    GApplication* app = g_application_new("cn.dh.dhlrc.cli", G_APPLICATION_HANDLES_COMMAND_LINE);
    g_signal_connect(app, "startup", G_CALLBACK(startup), NULL);
    g_signal_connect(app, "shutdown", G_CALLBACK(app_shutdown), NULL);
    g_signal_connect(app, "command-line", G_CALLBACK(run_app), NULL);
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
