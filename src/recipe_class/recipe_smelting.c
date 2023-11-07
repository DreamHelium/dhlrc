/*  recipe_smelting - Recipe smelting and blasting Object and class
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

#include "recipe_smelting.h"
#include "recipe_general.h"

struct _RecipeSmelting
{
    RecipeGeneral parent_instance;

    GPtrArray* ingredient;
};

G_DEFINE_FINAL_TYPE(RecipeSmelting, recipe_smelting, RECIPE_TYPE_GENERAL)

static void rsmt_set_content(RecipeGeneral* self, cJSON* json)
{
    /* Decide type of the json */
    g_return_if_fail(json != NULL);
    char* type = cJSON_GetStringValue(cJSON_GetObjectItem(json, "type"));
    g_return_if_fail(g_str_has_suffix(type, ":blasting") || g_str_has_suffix(type, ":smelting"));

    /* Get RecipeSmelting instance and set its type */
    RecipeSmelting* smelting_recipe = RECIPE_SMELTING(self);
    RECIPE_GENERAL_GET_CLASS(self)->set_type(self, type);

    /* Analyse ingredient */
    cJSON* ingredient = cJSON_GetObjectItem(json, "ingredient");
    if(cJSON_IsObject(ingredient))
    {
        smelting_recipe->ingredient = g_ptr_array_new_full(1, ing_ctr_free);
        IngContainer* ctr = ing_ctr_new(ingredient);
        g_ptr_array_add(smelting_recipe->ingredient, ctr);
    }
    else if(cJSON_IsArray(ingredient))
    {
        guint arr_num = cJSON_GetArraySize(ingredient);
        smelting_recipe->ingredient = g_ptr_array_new_full(arr_num, ing_ctr_free);
        for(int i = 0 ; i < arr_num ; i++)
        {
            cJSON* object = cJSON_GetArrayItem(ingredient, i);
            IngContainer* ctr = ing_ctr_new(object);
            g_ptr_array_add(smelting_recipe->ingredient, ctr);
        }
    }
}

static ItemList* rsmt_get_recipe(RecipeGeneral* self, guint num, DhGeneral* dh_general)
{
    RecipeSmelting* smelting_recipe = RECIPE_SMELTING(self);
    ItemList* recipe = NULL;
    for(int i = 0 ; i < (smelting_recipe->ingredient->len);i++)
    {
        IngContainer* ctr = (smelting_recipe->ingredient->pdata)[i];
        item_list_process_ing_ctr(&recipe, ctr, num);
    }
    return recipe;
}

static Recipe* rsmt_get_raw_recipe(RecipeGeneral* self)
{
    RecipeSmelting* rsmt = RECIPE_SMELTING(self);
    Recipe* recipe = malloc(sizeof(Recipe));
    recipe->result = 1;
    recipe->arr = rsmt->ingredient;
    recipe->pattern = NULL;
    return recipe;
}

static void recipe_smelting_finalize(GObject* object)
{
    RecipeSmelting* smlt = RECIPE_SMELTING(object);
    g_ptr_array_unref(smlt->ingredient);
    G_OBJECT_CLASS(recipe_smelting_parent_class)->finalize(object);
}

static void recipe_smelting_class_init(RecipeSmeltingClass* klass)
{
    RecipeGeneralClass* general_klass = RECIPE_GENERAL_CLASS(klass);
    general_klass->set_content = rsmt_set_content;
    general_klass->get_recipe = rsmt_get_recipe;
    general_klass->get_raw_recipe = rsmt_get_raw_recipe;

    GObjectClass* object_klass = G_OBJECT_CLASS(klass);
    object_klass->finalize = recipe_smelting_finalize;
}

static void recipe_smelting_init(RecipeSmelting* self)
{
    self->ingredient = NULL;
}


