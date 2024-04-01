/*  recipes_shaped - Recipes Shaped Object and class
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

#include "recipes_shaped.h"
#include "recipes_general.h"
#include <cjson/cJSON.h>

static Recipes* rpsd_get_recipes(RecipesGeneral* self, cJSON* json);

struct _RecipesShaped
{
    RecipesGeneral parent_instance;
};

G_DEFINE_FINAL_TYPE(RecipesShaped, recipes_shaped, RECIPES_TYPE_GENERAL)

static void recipes_shaped_class_init(RecipesShapedClass* klass)
{
    RECIPES_GENERAL_CLASS(klass)->get_recipes = rpsd_get_recipes;
}

static void recipes_shaped_init(RecipesShaped* self)
{

}

static Recipes* rpsd_get_recipes(RecipesGeneral* self, cJSON* json)
{
    /* Decide type of the json */
    g_return_val_if_fail(json != NULL, NULL);
    char* type = cJSON_GetStringValue(cJSON_GetObjectItem(json, "type"));
    g_return_val_if_fail(g_str_has_suffix(type, ":crafting_shaped"), NULL);

    /* Get pattern */
    cJSON* pattern = cJSON_GetObjectItem(json, "pattern");
    guint pattern_size = cJSON_GetArraySize(pattern);
    guint pattern_length = 1;
    DhStrArray* pattern_strv = NULL;
    for(int i = 0 ; i < pattern_size; i++)
    {
        char* str = cJSON_GetStringValue(cJSON_GetArrayItem(pattern, i));
        dh_str_array_add_str(&pattern_strv, str);
        pattern_length = strlen(str);
    }

    /* Set recipes data */
    Recipes* recipes = g_new(Recipes, 1);
    recipes->x = pattern_length;
    recipes->y = pattern_size;
    recipes->pattern = pattern_strv;

    /* Get result number */
    cJSON* count = cJSON_GetObjectItem(cJSON_GetObjectItem(json, "result"), "count");
    if(cJSON_IsNumber(count))
        recipes->num = cJSON_GetNumberValue(count);
    else recipes->num = 1;

    /* Get key numbers */
    cJSON* keys = cJSON_GetObjectItem(json, "key");
    guint key_num = cJSON_GetArraySize(keys);
    recipes->pt_num = key_num;

    recipes->pt = g_new0(PatternTranslator, key_num);

    for(int i = 0 ; i < key_num ; i++)
    {
        cJSON* key = cJSON_GetArrayItem(keys, i);
        recipes->pt[i].pattern = *(key->string);
        pattern_translator_writer(&(recipes->pt[i]), key);
    }
    return recipes;
}
