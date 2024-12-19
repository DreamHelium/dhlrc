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
#include "dh_file_util.h"

static GResource* res = NULL;

gboolean dhlrc_common_contents_init(const char* prname)
{
    char* prpath = dh_file_get_current_program_dir(prname);
    char* res_path = g_strconcat(prpath, G_DIR_SEPARATOR_S, "dhlrc_resources", NULL);
    GError* err = NULL;
    res = g_resource_load(res_path, &err);
    g_free(prpath);
    g_free(res_path);
    if(err)
    {
        g_message("Load resource error with message: %s.", err->message);
        g_error_free(err);
        return FALSE;
    }
    else
    {
        g_resources_register(res);
        return TRUE;
    }
}

GResource* dhlrc_get_resource()
{
    return res;
}

void dhlrc_common_contents_free()
{
    g_resource_unref(res);
}