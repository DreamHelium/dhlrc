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

#include <stdio.h>
#include <glib.h>
#include <glib/gi18n.h>
/*#include "dhlrc_config.h"*/
#include "main.h"
#include "translation.h"

static gboolean reader_mode = FALSE;
static gboolean block_mode = FALSE;
static gboolean list_mode = FALSE;
static gchar* log_filename = NULL;
static guint mode_num = 0;

static GOptionEntry entries[] =
{
    {"reader", 'r', 0, G_OPTION_ARG_NONE, &reader_mode, N_("Enter NBT reader mode."), NULL},
    {"block", 'b', 0, G_OPTION_ARG_NONE, &block_mode, N_("Enter litematica block reader."), NULL},
    {"list", 'l', 0, G_OPTION_ARG_NONE, &list_mode, N_("Enter litematica material list with recipe combination."), NULL},
    {"log", 0, 0, G_OPTION_ARG_FILENAME, &log_filename, N_("Output log file to FILE"), "FILE"}
};

int main(int argc,char** argb)
{
    translation_init();
    GOptionContext *context = g_option_context_new(_("[FILE] - Read a litematic file."));
    g_option_context_add_main_entries(context, entries, "dhlrc");
    GError *error = NULL;
    gchar **args = NULL;

#ifdef G_OS_WIN32
    args = g_win32_get_command_line();
#else
    args = g_strdupv(argb);
#endif


    if (!g_option_context_parse (context, &argc, &argb, &error))
    {
      g_print ("option parsing failed: %s\n", error->message);
      exit (1);
    }

    if( (reader_mode + block_mode + list_mode) > 1 )
    {
        g_print(_("Only one option can be chosen!\n"));
        exit(1);
    }

    if(argc == 1)
    {
        g_print("%s" ,g_option_context_get_help(context, TRUE, NULL));
    }

    g_strfreev(args);
    g_option_context_free(context);
    return main_isoc(argc, argb);
}
