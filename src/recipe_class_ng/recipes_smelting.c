/*  recipes_smelting - Recipes Smelting Object and class
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

#include "recipes_general.h"
#include "recipes_smelting.h"

static Recipes* rsmt_get_recipes(RecipesGeneral* self, cJSON* json);

struct _RecipesSmelting
{
    RecipesGeneral parent_class;
};

G_DEFINE_FINAL_TYPE(RecipesSmelting, recipes_smelting, RECIPES_TYPE_GENERAL)

static void recipes_smelting_init(RecipesSmelting* self)
{

}

static void recipes_smelting_class_init(RecipesSmeltingClass* klass)
{
    RECIPES_GENERAL_CLASS(klass)->get_recipes = rsmt_get_recipes;
}

static Recipes* rsmt_get_recipes(RecipesGeneral* self, cJSON* json)
{
    /* Decide type of the json */
    g_return_val_if_fail(json != NULL, NULL);
    char* type = cJSON_GetStringValue(cJSON_GetObjectItem(json, "type"));
    g_return_val_if_fail(g_str_has_suffix(type, ":blasting") || g_str_has_suffix(type, ":smelting"), NULL);

    /* Set recipes data */
    Recipes* recipes = g_new0(Recipes, 1);
    recipes->x = 1;
    recipes->y = 1;
    dh_str_array_add_str(&(recipes->pattern), "x");

    /**
    * Get result number *
    cJSON* count = cJSON_GetObjectItem(cJSON_GetObjectItem(json, "result"), "count");
    if(cJSON_IsNumber(count))
        recipes->num = cJSON_GetNumberValue(count);
    else recipes->num = 1;
    **/

    /* Get ingredient */
    cJSON* ingredient = cJSON_GetObjectItem(json, "ingredients");
    recipes->pt = g_new0(PatternTranslator, 1);
    recipes->pt->pattern = 'x';
    pattern_translator_writer(recipes->pt, ingredient);
    return recipes;
}
