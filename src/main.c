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
#include "dh_file_util.h"
#include "dh_string_util.h"
#include "glibconfig.h"
#include "gmodule.h"
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

gchar *log_filename = NULL;

typedef int (*DhlrcMainFunc) (int argc, char **argv, const char *);

static GModule *module = NULL;
static DhlrcMainFunc qt_main = NULL;
static DhlrcMainFunc conv_main = NULL;

static void
debug (int argc, char **argv)
{
}

static gboolean
get_module (const char *arg_zero)
{
    char *prpath = dh_file_get_current_program_dir (arg_zero);
    /* All platforms the module will be in module directory, sorry */
    char *module_path
        = g_strconcat (prpath, G_DIR_SEPARATOR_S, "module", NULL);

    g_free (prpath);
    char *dir = g_build_path (G_DIR_SEPARATOR_S, module_path, "libdhlrc_qt.so",
                              NULL);
    GError *err = NULL;
    GModule *new_module = g_module_open_full (dir, G_MODULE_BIND_MASK, &err);
    g_free (dir);

    if (err)
        {
            g_critical ("%s", err->message);
            g_error_free (err);
            return FALSE;
        }

    if (!g_module_symbol (new_module, "start_qt", (gpointer *)&qt_main))
        {
            g_critical ("Load Qt module failed!");
            return FALSE;
        }

    return TRUE;
}

static void
dhlrc_startup (const char *prname)
{
    dhlrc_common_contents_init (prname);
    common_infos_init ();
    char *prpath = dh_file_get_current_program_dir (prname);
    char *recipes_module_path
        = g_build_path (G_DIR_SEPARATOR_S, prpath, "recipes_module", NULL);
    g_free (prpath);
    recipe_handler_init (recipes_module_path);
    g_free (recipes_module_path);
}

static void
dhlrc_shutdown ()
{
    common_infos_free ();
    dhlrc_common_contents_free ();
    recipe_handler_free ();
    dh_exit ();
    dhmcdir_exit ();
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
    gboolean success = get_module (argv[0]);
    int ret = 0;

    if ((argc >= 2 && g_str_equal (argv[1], "--help")))
        {
            if (argc == 2)
                {
                    printf (_ ("dhlrc - Program to handle litematic or other "
                               "struct of Minecraft.\n"));
                    printf ("\n");
                    printf (_ ("Modules:\n"));

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
    else if (argc == 1 && success)
        {
#if defined G_OS_WIN32 || defined __APPLE__
            load_module (modules, len, "qt", argv[0], argc, argv, &success);
            if (!success)
                printf ("Failed to load qt module!\n");
#else
            if (g_getenv ("XDG_SESSION_DESKTOP"))
                {
                    char *prpath = dh_file_get_current_program_dir (argv[0]);
                    ret = qt_main (argc, argv, prpath);
                    g_free (prpath);
                }
            else
                {
                }
#endif
        }
    else if (argc >= 2)
        {
        }
    else
        printf ("Not supported!\n");

    return ret;
}

int
main (int argc, char **argb)
{
    /* There's not a good idea to load the library, but we will try */
    translation_init (argb[0]);

    dhlrc_startup (argb[0]);

    int ret = dhlrc_run (argc, argb);
    dhlrc_shutdown ();

    return ret;
}