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

#include "dhmcdir/internal_config.h"
#include "recipe_handler/handler.h"

#ifdef G_OS_WIN32
#define LINK_PATH "PATH"
#define MIDD_SEP ";"
#else
#define LINK_PATH "LD_LIBRARY_PATH"
#define MIDD_SEP ":"
#endif

gchar* log_filename = NULL;

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

static DhlrcModule* get_module(int* module_num, const char* arg_zero)
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
    dhmcdir_exit();
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