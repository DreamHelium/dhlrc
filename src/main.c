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
#include "common.h"
#include "common_info.h"
#include "config.h"
#include "dh_string_util.h"
#include "glibconfig.h"
#include "gmodule.h"
#include "dh_file_util.h"
#include "recipe_util.h"
#include "translation.h"
#include <dhutil.h>
#include <ncursesw/ncurses.h>
#include "recipe_handler/handler.h"

#ifdef G_OS_WIN32
#define LINK_PATH "PATH"
#define MIDD_SEP ";"
#else
#define LINK_PATH "LD_LIBRARY_PATH"
#define MIDD_SEP ":"
#endif

static gboolean reader_mode = FALSE;
static gboolean block_mode = FALSE;
static gboolean list_mode = FALSE;
gchar* log_filename = NULL;
static guint mode_num = 0;

static gchar* get_filename();

typedef int (*DhlrcMainFunc)(int argc, char** argv, const char*);
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

static DhlrcModule* get_module(int* module_num, char* arg_zero)
{
    int real_module_num = 0;

    char* prpath = dh_file_get_current_program_dir(arg_zero);
    /* All platforms the module will be in bin directory, sorry */
    char* module_path = g_strconcat(prpath, G_DIR_SEPARATOR_S, "module", NULL);

    g_free(prpath);

    GList* module_files = dh_file_list_create(module_path);
    DhlrcModule* ret = NULL;
    gboolean succeed = TRUE;
    for(int i = 0 ; i < g_list_length(module_files) ; i++)
    {
        GError* err = NULL;
        char* dir = g_strconcat(module_path, G_DIR_SEPARATOR_S, g_list_nth_data(module_files, i), NULL);
        GModule* module = g_module_open_full(dir, G_MODULE_BIND_MASK, &err);
        g_free(dir);

        if(!module) 
        {
            g_critical("%s", err->message);
            g_error_free(err);
            continue; /* Get Module fail */
        }
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
    g_free(module_path);
    g_list_free_full(module_files, free);
    return ret;
}



static void startup(GApplication* self, gpointer user_data)
{
    dhlrc_make_config();
    dhlrc_common_contents_init(user_data);
    common_infos_init();
    char* prname = user_data;
    char* prpath = dh_file_get_current_program_dir(prname);
    char* recipes_module_path = g_strconcat(prpath, G_DIR_SEPARATOR_S, "recipes_module", NULL);
    g_free(prpath);
    recipe_handler_init(recipes_module_path);
    g_free(recipes_module_path);
}

static void app_shutdown(GApplication* self, gpointer user_data)
{
    common_infos_free();
    dhlrc_common_contents_free();
    recipe_handler_free();
    dh_exit1();
    dh_exit();
}

// static gboolean file_open(GFile* file, gboolean single)
// {
//     gboolean ret = FALSE;
//     if(!g_file_query_exists(file, NULL))
//     {
//         if(single) g_critical(_("Not a valid NBT file!"));
//         return ret;
//     }
//     char* filename = g_file_get_basename(file); /* Should be freed */
//     char* description = NULL;
//     if(single)
//     {
//         DhOut* out = dh_out_new();
//         GValue val = {0};
//         dh_out_read_and_output(out, N_("Enter desciption for the NBT file."), "dhlrc", NULL, NULL, FALSE, &val);
//         if(G_VALUE_HOLDS_STRING(&val))
//         {
//             description = g_value_get_string(&val);
//         }
//         else description = g_strdup(filename);
//         g_free(filename);
//     }
//     else description = filename;
//     if(description)
//     {
//         guint8* content = NULL;
//         gsize len;
//         g_file_load_contents(file, NULL, (char**)&content, &len, NULL, NULL);
//         NBT* root = NBT_Parse(content, len);
//         if(root)
//         {
//             nbt_info_new(root, g_date_time_new_now_local(), description);
//             ret = TRUE;
//         }
//         else if(single)
//             g_critical(_("Not a valid NBT file!"));
//         g_free(description);
//         g_free(content);
//     }
//     return ret;
// }

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

static int load_module(DhlrcModule* modules, int len, const char* module_name, const char* prog_name, int argc, char** argv, gboolean* success)
{
    int module_pos = 0;
    int ret = 0;
    if(get_module_pos(modules, len, &module_pos, module_name))
    {
        if(success) *success = TRUE;
        char* prpath = dh_file_get_current_program_dir(prog_name);
        const char* ld_path = g_getenv(LINK_PATH);
        /* prpath/../lib/module */
        char* add_ld_path = g_strconcat(prpath, G_DIR_SEPARATOR_S, "..", G_DIR_SEPARATOR_S, "lib", G_DIR_SEPARATOR_S, module_name,NULL);
        char* new_ld_path = g_strconcat(ld_path ? ld_path : "", MIDD_SEP, add_ld_path ,NULL);
        g_setenv(LINK_PATH, new_ld_path, TRUE);
        g_free(add_ld_path);
        g_free(new_ld_path);
        ret = modules[module_pos].start_point(argc, argv, prpath);
        g_free(prpath);
    }
    else if(success) *success = FALSE;
    return ret;
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
    DhlrcModule* modules = get_module(&len, argv[0]);
    int ret = 0;

    if((argc >= 2 && g_str_equal(argv[1], "--help")))
    {
        if(argc == 2)
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
    else if(argc == 1)
    {
        gboolean success = TRUE;
#if defined G_OS_WIN32 || defined __APPLE__
        load_module(modules, len, "qt", argv[0], argc, argv, &success);
        if(!success) printf("Failed to load qt module!\n");
#else
        if(g_getenv("XDG_SESSION_DESKTOP"))
        {
            load_module(modules, len, "qt", argv[0], argc, argv, &success);
            if(!success) printf("Failed to load qt module!\n");
        }
        else 
        {
            load_module(modules, len, "cli", argv[0], argc, argv, &success);
            if(!success) printf("Failed to load cli module!\n");
        }
#endif
    }
    else if(argc >= 2 && get_module_pos(modules, len, &module_pos, argv[1]))
    {
        gboolean success = TRUE;
        load_module(modules, len, argv[1], argv[0], argc - 1, argv + 1, &success);
        if(!success) printf("Failed to load module!\n");
    }
    else printf("Not supported!\n");

    modules_free(modules, len);
    g_strfreev (argv);

    return ret;
}

// static void app_open(GApplication* self, gpointer files, gint n_files, gchar* hint, gpointer user_data)
// {
//     GFile** f = files;
//     DhStrArray* arr = NULL;
//     if(n_files == 1)
//         file_open(f[0], TRUE);
//     else
//     {
//         for(int i = 0 ; i < n_files ; i++)
//         {
//             gboolean success = file_open(f[i], FALSE);
//             if(!success)
//             {
//                 char* file_basename = g_file_get_basename(f[i]);
//                 dh_str_array_add_str(&arr, file_basename);
//                 g_free(file_basename);
//             }
//         }
//     }
//     if(n_files > 1 && arr)
//     {
//         g_critical(_("The following files couldn't be added:\n"));
//         for(int i = 0 ; i < arr->num ; i++)
//             g_critical("%s", arr->val[i]);
//     }
//     dh_str_array_free(arr);
// }

int main(int argc, char** argb)
{
    /* There's not a good idea to load the library but we will try */
    translation_init(argb[0]);
    GApplication* app = g_application_new("cn.dh.dhlrc.cli", G_APPLICATION_HANDLES_COMMAND_LINE);
    g_signal_connect(app, "startup", G_CALLBACK(startup), argb[0]);
    g_signal_connect(app, "shutdown", G_CALLBACK(app_shutdown), NULL);
    g_signal_connect(app, "command-line", G_CALLBACK(run_app), NULL);
    g_application_set_option_context_parameter_string(app, _("[FILE] - Read a litematic file."));

    int ret = g_application_run(app, argc, argb);

    g_object_unref(app);
    return ret;
}

// static gchar* get_filename()
// {
//     gchar* filename = NULL;
//     gchar* pure_filename = NULL;
//     gchar* game_dir = dh_get_game_dir();
//     gchar* schematics_dir = NULL;
//     if(game_dir)
//         schematics_dir = g_strconcat(game_dir, "/schematics", NULL);
//     g_free(game_dir);
//     while (1 && schematics_dir) 
//     {
//         GList* file_list = dh_file_list_create(schematics_dir);
//         if(file_list)
//         {
//             GList* file_list_d = file_list;
//             DhOut* out = dh_out_new();
//             dh_out_no_output_string_while_no_validator(out);
//             dh_out_output_match_string_than_arg(out);
//             DhArgInfo* arg = dh_arg_info_new();
//             for(int i = 0 ; i < g_list_length(file_list_d) ; i++)
//             {
//                 gchar* filename_d = g_strconcat(schematics_dir, "/", g_list_nth_data(file_list_d, i) ,NULL);
//                 GFile* file = g_file_new_for_path(filename_d);
//                 GFileType type = g_file_query_file_type(file, G_FILE_QUERY_INFO_NONE , NULL);
//                 gchar* type_name = NULL;
//                 if(type == G_FILE_TYPE_DIRECTORY)
//                     type_name = N_("directory");
//                 else type_name = N_("file");
//                 dh_arg_info_add_arg(arg, 0, g_list_nth_data(file_list_d, i), type_name);
//                 g_free(filename_d);
//                 g_object_unref(file);
//             }
//             dh_arg_info_add_arg(arg, 0, "..", "directory");
//             GValue val = {0};
//             printf("Current directory is %s.\n", schematics_dir);
//             dh_out_read_and_output(out, "Please enter the filename or directory name: ", "dhlrc", arg, NULL, FALSE, &val);
//             g_object_unref(out);
//             g_object_unref(arg);
//             if(G_VALUE_HOLDS_STRING(&val))
//             {
//                 #ifdef GOBJECT_AVAILABLE_IN_2_80
//                 pure_filename = g_value_steal_string(&val);
//                 #else
//                 pure_filename = g_value_get_string(&val);
//                 #endif
//             }
//             else 
//             {
//                 /* TODO: key binding */
//                 break;
//             }
//             gchar* tmp_filename = NULL;
//             if(pure_filename)
//                 tmp_filename = g_strconcat(schematics_dir, "/", pure_filename, NULL);
//             g_free(pure_filename);
//             gboolean tmp_file_is_directory = dh_file_is_directory(tmp_filename);
//             if(tmp_file_is_directory)
//             {
//                 g_free(schematics_dir);
//                 schematics_dir = tmp_filename;
//             }
//             else 
//             {
//                 filename = tmp_filename;
//                 break;
//             }
//         }
//         else break;
//     }
//     g_free(schematics_dir);
//     return filename;
// }
