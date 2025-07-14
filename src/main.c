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
#include "config.h"
#include "conv_feature.h"
#include "dh_file_util.h"
#include "dh_string_util.h"
#include "glibconfig.h"
#include "gmodule.h"
#include "mcdata_feature.h"
#include "recipe_util.h"
#include "translation.h"

#include "recipe_handler/handler.h"

#ifdef G_OS_WIN32
#define LINK_PATH "PATH"
#define MIDD_SEP ";"
#define QT_MODULE_NAME "dhlrc_qt.dll"
#define CONV_MODULE_NAME "libdhlrc_conv.dll"
#define MCDATA_MODULE_NAME "libdhlrc_mcdata.dll"
#else
#define LINK_PATH "LD_LIBRARY_PATH"
#define MIDD_SEP ":"
#define QT_MODULE_NAME "libdhlrc_qt.so"
#define CONV_MODULE_NAME "libdhlrc_conv.so"
#define MCDATA_MODULE_NAME "libdhlrc_mcdata.so"
#endif

gchar *log_filename = NULL;

typedef int (*DhlrcMainFunc) (int argc, char **argv, const char *);

static GModule *qt_module = NULL;
static GModule *conv_module = NULL;
static GModule *mcdata_module = NULL;
static DhlrcMainFunc qt_main = NULL;
static DhlrcMainFunc conv_main = NULL;

static void
debug (int argc, char **argv)
{
}

static gboolean
get_module (const char *arg_zero, const char *module_name)
{
    char* module_path = NULL;
    char *prpath = dh_file_get_current_program_dir (arg_zero);
    /* All platforms the module will be in module directory, sorry */
#ifdef MODULEDIR
    module_path = g_strdup(MODULEDIR);
#else
    module_path
        = g_strconcat (prpath, G_DIR_SEPARATOR_S, "module", NULL);
#endif

    g_free (prpath);
    char *dir
        = g_build_path (G_DIR_SEPARATOR_S, module_path, module_name, NULL);
    g_free(module_path);
    GError *err = NULL;
    GModule *new_module = g_module_open_full (dir, 0, &err);
    if (module_name == QT_MODULE_NAME)
        qt_module = new_module;
    else if (module_name == CONV_MODULE_NAME)
        conv_module = new_module;
    else if (module_name == MCDATA_MODULE_NAME)
        mcdata_module = new_module;
    g_free (dir);

    if (err)
        {
            g_critical ("%s", err->message);
            g_error_free (err);
            return FALSE;
        }

    if (module_name == QT_MODULE_NAME)
        {
            if (!g_module_symbol (new_module, "start_qt",
                                  (gpointer *)&qt_main))
                {
                    g_critical ("Load Qt module failed!");
                    return FALSE;
                }
        }
    else if (module_name == CONV_MODULE_NAME)
        {
            conv_main = dhlrc_conv_enable (conv_module);
            if (!conv_main)
                return FALSE;
        }
    else if (module_name == MCDATA_MODULE_NAME)
            dhlrc_mcdata_enable (mcdata_module);

    return TRUE;
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

    // debug(argc, argv);
    get_module (argv[0], QT_MODULE_NAME);
    get_module (argv[0], CONV_MODULE_NAME);
    get_module (argv[0], MCDATA_MODULE_NAME);
    int ret = 0;

    if (argc >= 2 && g_str_equal (argv[1], "--help"))
        {
            if (argc == 2)
                {
                    printf (_ ("dhlrc - Program to handle litematic or other "
                               "struct of Minecraft.\n"));
                    printf ("\n");
                    printf (_ ("Modules:\n"));
                    if (qt_module)
                        printf (_ ("qt: The Graphical Interface.\n"));
                    if (conv_module)
                        printf (_ ("conv: The Simple Converter.\n"));

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
#if defined G_OS_WIN32 || defined __APPLE__
            ret = qt_main (argc, argv, argv[0]);
#else
            if (g_getenv ("XDG_SESSION_DESKTOP"))
                ret = qt_main (argc, argv, argv[0]);
            else
                g_error (_ ("CLI interface is not supported yet!"));
#endif
        }
    else if (argc >= 2)
        {
            if (g_str_equal (argv[1], "qt"))
                ret = qt_main (argc - 1, argv + 1, argv[0]);
            else if (g_str_equal (argv[1], "conv"))
                ret = conv_main (argc - 1, argv + 1, argv[0]);
        }
    else
        printf ("Not supported!\n");

    return ret;
}

int
main (int argc, char **argb)
{
    int ret = dhlrc_run (argc, argb);
    g_module_close (qt_module);
    g_module_close (conv_module);
    g_module_close (mcdata_module);

    return ret;
}