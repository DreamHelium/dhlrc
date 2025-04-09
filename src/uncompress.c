/*  uncompress.c - Uncompression functions
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

#include "uncompress.h"
#include <mz.h>
#include <time.h>
#include "glib.h"
#include "glibconfig.h"
#include "mz_os.h"
#include "mz_strm.h"
#include "mz_strm_buf.h"
#include "mz_strm_split.h"
#include "mz_zip.h"
#include "mz_zip_rw.h"

static char* pattern = NULL;

static void set_to_path(void* handler, const char* path)
{
    mz_zip_reader_goto_first_entry(handler);
    char* pattern = g_strconcat(path, "/*", NULL);
    mz_zip_reader_set_pattern(handler, pattern, FALSE);
    int err = mz_zip_reader_goto_first_entry(handler);
}

/* The source code below is from "minizip.c" in "minizip-ng"
 * See the source in the submodule for more information */

int dhlrc_extract(const char* path, const char* dest)
{
    void* reader = NULL;
    int err = MZ_OK;
    int err_close = MZ_OK;

#ifdef DHLRC_MZ_VERSION_LESS_THAN_FOUR
    reader = mz_zip_reader_create(NULL);
#else
    reader = mz_zip_reader_create();
#endif

    if(!reader)
        return MZ_MEM_ERROR;

    err = mz_zip_reader_open_file(reader, path);

    if(err == MZ_OK)
    {
        err = mz_zip_reader_save_all(reader, dest);
    }

    err_close = mz_zip_reader_close(reader);
    if(err_close != MZ_OK)
        err = err_close;

    mz_zip_reader_delete(&reader);
    return err;
}

int dhlrc_extract_part(const char* path, const char* dest, DhStrArray* arr)
{
    void* reader = NULL;
    int err = MZ_OK;
    int err_close = MZ_OK;

#ifdef DHLRC_MZ_VERSION_LESS_THAN_FOUR
    reader = mz_zip_reader_create(NULL);
#else
    reader = mz_zip_reader_create();
#endif

    if(!reader)
        return MZ_MEM_ERROR;

    err = mz_zip_reader_open_file(reader, path);

    if(err == MZ_OK)
    {
        for(int i = 0 ; i < arr->num ; i++)
        {
            set_to_path(reader, arr->val[i]);
            do {
                mz_zip_file* info = NULL;
                err = mz_zip_reader_entry_get_info(reader, &info);
                const char* filename = info->filename;
                gboolean is_file = mz_zip_reader_entry_is_dir(reader);
                gchar* dir = g_build_path(G_DIR_SEPARATOR_S, dest, filename, NULL);
                err = mz_zip_reader_entry_save_file(reader, dir);
                g_free(dir);
            }
            while (mz_zip_reader_goto_next_entry(reader) == MZ_OK);
            g_free(pattern);
        }
    }
    mz_zip_reader_close(reader);
    mz_zip_reader_delete(&reader);
    return err;
}