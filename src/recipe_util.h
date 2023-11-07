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

#ifdef __cplusplus
extern "C"{
#endif

#include "dhlrc_list.h"


#include <cjson/cJSON.h>
#include <dh/dh_generaliface.h>


/** Get recipes and combine to ItemList (discard all the items) */
int         item_list_combine_recipe(ItemList** o_bl, const char* dirpos, DhGeneral* general);
long *      num_array_get_from_input(int *array_num, int max_num);

const char* name_block_translate(const char* block_name);
int         dh_exit();

#ifdef __cplusplus
}
#endif

#endif // RECIPE_UTIL_H
