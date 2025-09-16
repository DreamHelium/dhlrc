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

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
/*#include "dhlrc_config.h"*/
#include "common.h"
#include "common_info.h"
#include "dh_file_util.h"
#include "feature/conv_feature.h"
#include "feature/dh_module.h"
#include "feature/mcdata_feature.h"
#include "feature/recipe_feature.h"
#include "feature/unzip_feature.h"
#include "glibconfig.h"
#include "gmodule.h"
#include "translation.h"

#include <dh_type.h>

#ifdef G_OS_WIN32
#define LINK_PATH "PATH"
#define MIDD_SEP ";"
#else
#define LINK_PATH "LD_LIBRARY_PATH"
#define MIDD_SEP ":"
#endif

gchar *log_filename = NULL;

typedef int (*DhlrcMainFunc) (int argc, char **argv, const char *);

static void
debug (int argc, char **argv)
{
}

// static int load_module(DhlrcModule* modules, int len, const char*
// module_name, const char* prog_name, int argc, char** argv, gboolean*
// success)
// {
//     int module_pos = 0;
//     int ret = 0;
//     if(get_module_pos(modules, len, &module_pos, module_name))
//     {
//         if(success) *success = TRUE;
//         char* prpath = dh_file_get_current_program_dir(prog_name);
//         const char* ld_path = g_getenv(LINK_PATH);
//         /* prpath/../lib/module */
//         char* add_ld_path = g_strconcat(prpath, G_DIR_SEPARATOR_S, "..",
//         G_DIR_SEPARATOR_S, "lib", G_DIR_SEPARATOR_S, module_name,NULL);
//         char* new_ld_path = g_strconcat(ld_path ? ld_path : "", MIDD_SEP,
//         add_ld_path ,NULL); g_setenv(LINK_PATH, new_ld_path, TRUE);
//         g_free(add_ld_path);
//         g_free(new_ld_path);
//         ret = modules[module_pos].start_point(argc, argv, prpath);
//         g_free(prpath);
//     }
//     else if(success) *success = FALSE;
//     return ret;
// }

static gint
dhlrc_run (int argc, char **argv)
{
    if (!g_module_supported ())
        g_error ("The program is not supported!\n");
    char *prpath = dh_file_get_current_program_dir (argv[0]);
    dh_search_module (prpath);
    g_free (prpath);

    int ret = 0;

    DhlrcMainFunc qt_func = NULL;
    DhlrcMainFunc real_main_func = NULL;
    DhlrcMainFunc conv_main_func = NULL;

    if (argc >= 2 && g_str_equal (argv[1], "--help"))
        {
            if (argc == 2)
                {
                    printf (_ ("dhlrc - Program to handle litematic or other "
                               "struct of Minecraft.\n"));
                    printf ("\n");
                    printf (_ ("Modules:\n"));

                    const DhStrArray *uuid_arr
                        = dh_info_get_all_uuid (DH_TYPE_MODULE);
                    for (int i = 0; uuid_arr && i < uuid_arr->num; i++)
                        {
                            const char *uuid = uuid_arr->val[i];
                            DhModule *module
                                = dh_info_get_data (DH_TYPE_MODULE, uuid);
                            printf ("%s(%s) -- %s\n", module->module_name,
                                    module->module_type,
                                    module->module_description);
                        }
                    printf ("\n");
                }
            else
                {
                    printf (_ ("Unrecognized options "));
                    for (int i = 2; i < argc; i++)
                        printf ("\"%s\"", argv[i]);
                    printf (".\n");
                }
        }
    else if (argc == 1)
        {
            const DhStrArray *uuid_arr = dh_info_get_all_uuid (DH_TYPE_MODULE);
            for (int i = 0; uuid_arr && i < uuid_arr->num; i++)
                {
                    const char *uuid = uuid_arr->val[i];
                    DhModule *module = dh_info_get_data (DH_TYPE_MODULE, uuid);
                    if (g_str_equal (module->module_name, "qt"))
                        qt_func = module->module_functions->pdata[0];
                    if (!g_str_equal (module->module_name, "qt")
                        && g_str_equal (module->module_type, "gui"))
                        real_main_func = module->module_functions->pdata[0];
                }
#if defined G_OS_WIN32 || defined __APPLE__
            if (qt_func)
                ret = qt_func (argc, argv, argv[0]);
            else if (real_main_func)
                ret = real_main_func (argc, argv, argv[0]);
            else
                g_error (_ ("No GUI Module!"));
#else
            if (g_getenv ("XDG_SESSION_DESKTOP"))
                {
                    if (qt_func)
                        ret = qt_func (argc, argv, argv[0]);
                    else if (real_main_func)
                        ret = real_main_func (argc, argv, argv[0]);
                    else
                        g_error (_ ("No GUI Module!"));
                }
            else
                g_error (_ ("CLI interface is not supported yet!"));
#endif
        }
    else if (argc >= 2)
        {
            const DhStrArray *uuid_arr = dh_info_get_all_uuid (DH_TYPE_MODULE);
            for (int i = 0; uuid_arr && i < uuid_arr->num; i++)
                {
                    const char *uuid = uuid_arr->val[i];
                    DhModule *module = dh_info_get_data (DH_TYPE_MODULE, uuid);
                    if (g_str_equal (module->module_name, "qt"))
                        qt_func = module->module_functions->pdata[0];
                    if (g_str_equal (module->module_name, argv[1])
                        && g_str_equal (module->module_type, "gui"))
                        real_main_func = module->module_functions->pdata[0];
                    if (g_str_equal (module->module_name, "conv"))
                        conv_main_func = module->module_functions->pdata[0];
                }
            if (g_str_equal (argv[1], "qt"))
                ret = qt_func (argc - 1, argv + 1, argv[0]);
            else if (g_str_equal (argv[1], "conv"))
                ret = conv_main_func (argc - 1, argv + 1, argv[0]);
            else if (real_main_func)
                ret = real_main_func (argc - 1, argv + 1, argv[0]);
            else
                printf ("Not supported!\n");
        }
    else
        printf ("Not supported!\n");

    return ret;
}

int
main (int argc, char **argb)
{
    int ret = dhlrc_run (argc, argb);
    if (dhlrc_is_inited ())
        dhlrc_cleanup ();

    return ret;
}