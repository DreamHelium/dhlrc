/*  recipes_general - Recipes General Object and Class
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

#ifndef RECIPES_GENERAL_H
#define RECIPES_GENERAL_H

#undef signals
#include <glib-object.h>
#include <cjson/cJSON.h>
#include <dh/dhutil.h>

G_BEGIN_DECLS

#define RECIPES_TYPE_GENERAL recipes_general_get_type()
G_DECLARE_DERIVABLE_TYPE(RecipesGeneral, recipes_general, RECIPES, GENERAL, GObject)

typedef struct PatternTranslator{
    char pattern;
    DhStrArray* item_string;
} PatternTranslator;

typedef struct Recipes{
    guint x;
    guint y;
    DhStrArray* pattern;

    guint num;

    /* Pattern Translator */
    PatternTranslator* pt;
    guint pt_num;
}Recipes;

struct _RecipesGeneralClass{
    GObjectClass parent_class;

    Recipes* (*get_recipes)(RecipesGeneral* self, cJSON* json);
};

gboolean recipes_is_supported(const char* filename);
RecipesGeneral* recipes_general_new(const char* filename);
Recipes* recipes_get_recipes(const char* filename);
void pattern_translator_writer(PatternTranslator* pt, cJSON* json);
void pattern_translator_free(PatternTranslator* pt);
void recipes_free(Recipes* r);

G_END_DECLS

#endif
