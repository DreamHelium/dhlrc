/*  recipe_util - utilities to process recipes
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

#ifndef RECIPE_UTIL_H
#define RECIPE_UTIL_H

#include "mcdata/mcdata.h"

#include <cjson/cJSON.h>
#include <dhutil.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /** Get recipes and combine to ItemList (discard all the items) */
    // long *      num_array_get_from_input(int *array_num, int max_num);

    G_DEPRECATED
    const char *name_block_translate (const char *block_name);
    gboolean dhlrc_found_transfile ();
    void dhlrc_update_transfile (const char *version, SetFunc set_func,
                                 void *klass, char **large_version);
    int dh_exit ();

#ifdef __cplusplus
}
#endif

#endif // RECIPE_UTIL_H
