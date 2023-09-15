/*  recipe_shapeless - Recipe Shapeless Object and class
    Copyright (C) 2023 Dream Helium
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

#include "recipe_shapeless.h"

struct _RecipeShapeless
{
    RecipeGeneral parent_instance;

    guint result;
    GPtrArray* ingredient;
};

G_DEFINE_FINAL_TYPE(RecipeShapeless, recipe_shapeless, RECIPE_TYPE_GENERAL)

static void recipe_shapeless_finalize(GObject* object)
{
    RecipeShapeless* spls = RECIPE_SHAPELESS(object);
    g_ptr_array_unref(spls->ingredient);
    G_OBJECT_CLASS(recipe_shapeless_parent_class)->finalize(object);
}

static void rspl_set_content(RecipeGeneral* self, cJSON* json)
{
    /* Decide type of the json */
    g_return_if_fail(json != NULL);
    char* type = cJSON_GetStringValue(cJSON_GetObjectItem(json, "type"));
    g_return_if_fail(g_str_has_suffix(type, ":crafting_shapeless"));

    /* Get RecipeSmelting instance and set its type */
    RecipeShapeless* shapeless_recipe = RECIPE_SHAPELESS(self);
    RECIPE_GENERAL_GET_CLASS(self)->set_type(self, type);

    /* Get result number */
    cJSON* count = cJSON_GetObjectItem(cJSON_GetObjectItem(json, "result"), "count");
    if(cJSON_IsNumber(count))
        shapeless_recipe->result = cJSON_GetNumberValue(count);
    else shapeless_recipe->result = 1;

    /* Get ingredient */
    cJSON* ingredient = cJSON_GetObjectItem(json, "ingredients");
    if(cJSON_IsObject(ingredient))
    {
        shapeless_recipe->ingredient = g_ptr_array_new_full(1, IngCtr_free);
        IngContainer* ctr = IngCtr_new(ingredient);
        g_ptr_array_add(shapeless_recipe->ingredient, ctr);
    }
    else if(cJSON_IsArray(ingredient))
    {
        guint arr_num = cJSON_GetArraySize(ingredient);
        shapeless_recipe->ingredient = g_ptr_array_new_full(arr_num, IngCtr_free);
        for(int i = 0 ; i < arr_num ; i++)
        {
            cJSON* object = cJSON_GetArrayItem(ingredient, i);
            IngContainer* ctr = IngCtr_new(object);
            g_ptr_array_add(shapeless_recipe->ingredient, ctr);
        }
    }
}

static void recipe_shapeless_class_init(RecipeShapelessClass* klass)
{
    RECIPE_GENERAL_CLASS(klass)->set_content = rspl_set_content;
    G_OBJECT_CLASS(klass)->finalize = recipe_shapeless_finalize;
}

static void recipe_shapeless_init(RecipeShapeless* self)
{
    self->ingredient = NULL;
}
