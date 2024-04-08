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
#include "mz.h"
#include "mz_os.h"
#include "mz_strm.h"
#include "mz_strm_buf.h"
#include "mz_strm_split.h"
#include "mz_zip.h"
#include "mz_zip_rw.h"

/* The source code below is from "minizip.c" in "minizip-ng"
 * See the source in the submodule for more information */

int dhlrc_extract(const char* path, const char* dest)
{
    void* reader = NULL;
    int err = MZ_OK;
    int err_close = MZ_OK;

    reader = mz_zip_reader_create();
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
