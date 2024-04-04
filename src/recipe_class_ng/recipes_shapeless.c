/*  recipes_shapeless - Recipes Shapeless Object and class
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

#include "dh_string_util.h"
#include "recipes_general.h"
#include "recipes_shapeless.h"

static Recipes* rspl_get_recipes(RecipesGeneral* self, cJSON* json);

struct _RecipesShapeless
{
    RecipesGeneral parent_class;
};

G_DEFINE_FINAL_TYPE(RecipesShapeless, recipes_shapeless, RECIPES_TYPE_GENERAL)

static void recipes_shapeless_init(RecipesShapeless* self)
{

}

static void recipes_shapeless_class_init(RecipesShapelessClass* klass)
{
    RECIPES_GENERAL_CLASS(klass)->get_recipes = rspl_get_recipes;
}

static Recipes* rspl_get_recipes(RecipesGeneral* self, cJSON* json)
{
    /* Decide type of the json */
    g_return_val_if_fail(json != NULL, NULL);
    char* type = cJSON_GetStringValue(cJSON_GetObjectItem(json, "type"));
    g_return_val_if_fail(g_str_has_suffix(type, ":crafting_shapeless"), NULL);

    /* Set recipes data */
    Recipes* recipes = g_new0(Recipes, 1);
    recipes->x = 1;
    recipes->y = 0;

    /* Get result number */
    cJSON* count = cJSON_GetObjectItem(cJSON_GetObjectItem(json, "result"), "count");
    if(cJSON_IsNumber(count))
        recipes->num = cJSON_GetNumberValue(count);
    else recipes->num = 1;

    /* Get ingredient */
    cJSON* ingredient = cJSON_GetObjectItem(json, "ingredients");
    recipes->pt = g_new0(PatternTranslator, 1);
    pattern_translator_writer(recipes->pt, ingredient);
    recipes->pt_num = recipes->pt->item_string->num;
    DhStrArray* old_pt = recipes->pt->item_string;
    /* Rewrite ingredient */
    recipes->pt->item_string = NULL;
    recipes->pt = g_renew(PatternTranslator, recipes->pt, recipes->pt_num);
    for(int i = 0 ; i < recipes->pt_num ; i++)
    {
        recipes->pt[i].item_string = NULL;
        recipes->pt[i].pattern = 'A' + i;
        (recipes->y)++;
        char p[2] = { 'A'+i , 0};
        dh_str_array_add_str(&(recipes->pattern), p);
        dh_str_array_add_str(&(recipes->pt[i].item_string), old_pt->val[i]);
    }
    dh_str_array_free(old_pt);
    return recipes;
}
