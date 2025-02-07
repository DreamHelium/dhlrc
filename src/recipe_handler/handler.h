/*  handler - Recipes Handler
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

#ifndef DH_RECIPE_HANDLER_H
#define DH_RECIPE_HANDLER_H

#include "../dhutil/dh_string_util.h"
#include <cjson/cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PatternTransformer{
    char pattern;
    DhStrArray* item_string;
} PatternTransformer;

typedef struct DhRecipes{
    guint x;
    guint y;
    DhStrArray* pattern;

    guint num;
    /* This is rare, it's always be 1 and be ignored. */
    double rate;

    /* Pattern Translator */
    PatternTransformer* pt;
    guint pt_num;
}DhRecipes;

void recipe_handler_init(const char* recipelib_path);
void recipe_handler_free();
gboolean recipes_is_supported(const char* filename);
gboolean recipes_is_supported_type(const char* type);
DhRecipes* recipes_get_recipes(const char* filename);
void pattern_translator_writer(PatternTransformer* pt, cJSON* json);
void pattern_translator_free(PatternTransformer* pt);
void recipes_free(DhRecipes* r);

#ifdef __cplusplus
}
#endif

#endif /* DH_RECIPE_HANDLER_H */