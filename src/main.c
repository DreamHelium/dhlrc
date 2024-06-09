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
#include "main.h"
#include "dh_file_util.h"
#include "dh_validator.h"
#include "translation.h"
#include <dhutil.h>

static gboolean reader_mode = FALSE;
static gboolean block_mode = FALSE;
static gboolean list_mode = FALSE;
gchar* log_filename = NULL;
static guint mode_num = 0;
int verbose_level = 0;

static gchar* get_filename();

static GOptionEntry entries[] =
{
    {"reader", 'r', 0, G_OPTION_ARG_NONE, &reader_mode, N_("Enter NBT reader mode."), NULL},
    {"block", 'b', 0, G_OPTION_ARG_NONE, &block_mode, N_("Enter litematica block reader."), NULL},
    {"list", 'l', 0, G_OPTION_ARG_NONE, &list_mode, N_("Enter litematica material list with recipe combination."), NULL},
    {"log", 0, 0, G_OPTION_ARG_FILENAME, &log_filename, N_("Output log file to FILE"), "FILE"},
    {"verbose", 'v', 0, G_OPTION_ARG_INT, &verbose_level, N_("Set verbose level to N.\n""\t\t\t""Level 1: See process.\n""\t\t\t""Level 2: See details except block processing.\n""\t\t\t""Level 3: See all the details (Not recommended!)."), "N"}
};

int main(int argc, char** argb)
{
    dhlrc_make_config();
    translation_init();
    GOptionContext *context = g_option_context_new(_("[FILE] - Read a litematic file."));
    //GOptionGroup* verbose_group = g_option_group_new("v", "Unfinished", "See level", NULL, NULL);

    g_option_context_add_main_entries(context, entries, "dhlrc");
    //g_option_context_add_group(context, verbose_group);
    GError *error = NULL;
    gchar **args = NULL;

#ifdef G_OS_WIN32
    args = g_win32_get_command_line();
#else
    args = g_strdupv(argb);
#endif


    if (!g_option_context_parse (context, &argc, &args, &error))
    {
        g_print ("option parsing failed: %s\n", error->message);
        return EXIT_FAILURE;
    }

    if( (reader_mode + block_mode + list_mode) > 1 )
    {
        g_print(_("Only one option can be chosen!\n"));
        return EXIT_FAILURE;
    }

    if( verbose_level > 3 )
    {
        g_print(_("A level below 3 is allowed!\n"));
        return EXIT_FAILURE;
    }

    if(argc == 1)
    {
        /* Enter interactive mode */
        gchar* filename = get_filename();
        if(filename)
        {
            args = g_realloc(args, 3 * sizeof(gchar*));
            argc = 2;
            args[1] = filename;
            args[2] = NULL;
        }
    }

    //g_option_group_unref(verbose_group);
    g_option_context_free(context);
    return main_isoc(argc, args);
}

static gchar* get_filename()
{
    gchar* filename = NULL;
    gchar* game_dir = dh_get_game_dir();
    gchar* schematics_dir = g_strconcat(game_dir, "/schematics", NULL);
    g_free(game_dir);
    GList* file_list = dh_file_list_create(schematics_dir);
    if(file_list)
    {
        GList* file_list_d = file_list;
        DhOut* out = dh_out_new();
        
    }
    g_free(schematics_dir);
    return filename;
}
