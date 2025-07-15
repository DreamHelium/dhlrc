/*  common - Common Functions
    Copyright (C) 2024 Dream Helium
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

#include "common.h"

#include "common_info.h"
#include "config.h"
#include "dh_file_util.h"
#include "recipe_handler/handler.h"
#include "recipe_util.h"
#include "translation.h"

static GResource *res = NULL;

gboolean
dhlrc_common_contents_init (const char *prname)
{
    char* res_path = NULL;
    char *prpath = dh_file_get_current_program_dir (prname);
#ifdef RESOURCEDIR
    res_path = g_strdup(RESOURCEDIR);
#else
    res_path
        = g_strconcat (prpath, G_DIR_SEPARATOR_S, "dhlrc_resources", NULL);
#endif
    GError *err = NULL;
    res = g_resource_load (res_path, &err);
    g_free (prpath);
    g_free (res_path);
    if (err)
        {
            g_message ("Load resource error with message: %s.", err->message);
            g_error_free (err);
            return FALSE;
        }
    else
        {
            g_resources_register (res);
            return TRUE;
        }
}

GResource *
dhlrc_get_resource ()
{
    return res;
}
void
dhlrc_init (const char *prname)
{
    translation_init (prname);
    dhlrc_make_config ();
    dhlrc_common_contents_init (prname);
    common_infos_init ();
    char *prpath = dh_file_get_current_program_dir (prname);
    char *recipes_module_path
        = g_build_path (G_DIR_SEPARATOR_S, prpath, "recipes_module", NULL);
    g_free (prpath);
    recipe_handler_init (recipes_module_path);
    g_free (recipes_module_path);
}

void
dhlrc_cleanup ()
{
    common_infos_free ();
    dhlrc_common_contents_free ();
    recipe_handler_free ();
    dhlrc_cleanup_manifest ();
    dh_exit ();
    dh_exit1 ();
}

void
dhlrc_common_contents_free ()
{
    g_resource_unref (res);
}