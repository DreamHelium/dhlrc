/*  recipe_shaped - Recipe Shaped Object and class
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

#include "recipe_shaped.h"
#include "recipe_general.h"
#include <cjson/cJSON.h>
#include <dhutil.h>
#include "../translation.h"


static void KeyIng_free(gpointer data)
{
    KeyIng* data_d = data;
    g_ptr_array_unref(data_d->ingredients);
    free(data_d);
}

static KeyIng* KeyIng_new(char key, GPtrArray* ingredients)
{
    KeyIng* keying = malloc(sizeof(KeyIng));
    keying->key = key;
    keying->ingredients = ingredients;
    return keying;
}

struct _RecipeShaped
{
    RecipeGeneral parent_instance;

    guint result;
    DhStrArray* pattern;
    GPtrArray* key;
};

static void rpsd_set_content(RecipeGeneral* self, cJSON* json)
{
    /* Decide type of the json */
    g_return_if_fail(json != NULL);
    char* type = cJSON_GetStringValue(cJSON_GetObjectItem(json, "type"));
    g_return_if_fail(g_str_has_suffix(type, ":crafting_shaped"));

    /* Get RecipeSmelting instance and set its type */
    RecipeShaped* shaped_recipe = RECIPE_SHAPED(self);
    RECIPE_GENERAL_GET_CLASS(self)->set_type(self, type);

    /* Get result number */
    cJSON* count = cJSON_GetObjectItem(cJSON_GetObjectItem(json, "result"), "count");
    if(cJSON_IsNumber(count))
        shaped_recipe->result = cJSON_GetNumberValue(count);
    else shaped_recipe->result = 1;

    /* Get pattern */
    cJSON* pattern = cJSON_GetObjectItem(json, "pattern");
    guint pattern_size = cJSON_GetArraySize(pattern);
    DhStrArray* pattern_strv = NULL;
    for(int i = 0 ; i < pattern_size; i++)
    {
        char* str = cJSON_GetStringValue(cJSON_GetArrayItem(pattern, i));
        dh_str_array_add_str(&pattern_strv, str);
    }
    shaped_recipe->pattern = pattern_strv;

    /* Get key numbers and assign memory to the array */
    cJSON* keys = cJSON_GetObjectItem(json, "key");
    guint key_num = cJSON_GetArraySize(keys);
    GPtrArray* key_arr = g_ptr_array_new_full(key_num, KeyIng_free);

    /* Get key
     * Method: Create *IngCtr* struct first because the key might be
     * an array. Then add IngCtr with key to the *KeyIng* struct, add
     * to the @key_arr.
     */
    for(int i = 0 ; i < key_num ; i++)
    {
        cJSON* key = cJSON_GetArrayItem(keys, i);
        GPtrArray* intctr_arr = NULL;
        if(cJSON_IsObject(key))
        {
            intctr_arr = g_ptr_array_new_full(1, ing_ctr_free);
            IngContainer* ctr = ing_ctr_new(key);
            g_ptr_array_add(intctr_arr, ctr);
        }
        else if(cJSON_IsArray(key))
        {
            guint key_size = cJSON_GetArraySize(key);
            intctr_arr = g_ptr_array_new_full(key_size, ing_ctr_free);
            for(int j = 0 ; j < key_size; j++)
            {
                IngContainer* ctr = ing_ctr_new(cJSON_GetArrayItem(key, j));
                g_ptr_array_add(intctr_arr, ctr);
            }
        }
        char key_char = *(key->string);
        KeyIng* keying = KeyIng_new(key_char, intctr_arr);
        g_ptr_array_add(key_arr, keying);
    }
    shaped_recipe->key = key_arr;
}

static ItemList* rpsd_get_recipe(RecipeGeneral* self, guint num, DhGeneral* dh_general)
{
    RecipeShaped* shaped_recipe = RECIPE_SHAPED(self);
    int division_num = dh_mod_decide(num, shaped_recipe->result, dh_general);
    if(division_num == 0 || division_num == -1)
        return NULL;

    ItemList* recipe = NULL;
    for(int i = 0 ; i < shaped_recipe->key->len ; i++)
    {
        KeyIng* keying = shaped_recipe->key->pdata[i];
        guint key_num = dh_str_array_find_char(shaped_recipe->pattern, keying->key);

        GPtrArray* ingredient = keying->ingredients;
        if(ingredient->len == 1)
            item_list_process_ing_ctr(&recipe, ingredient->pdata[0], division_num * key_num);
        else{
            dh_new_win(dh_general, FALSE);
            dh_printf(dh_general, _("There are some ingredients to choose:\n"));
            guint size = ingredient->len;
            for(int j = 0 ; j < size ; j++)
            {
                char* name = ((IngContainer*)(ingredient->pdata[j]))->ingredient;
                dh_option_printer(dh_general, j, name);
            }
            int option = dh_selector(dh_general, _("Please select an item/tag, or enter 'a' to give up selecting (a): "), size, "a", _("&Abort"));
            if(option == -1 || option == -100)
                return NULL;
            else
                item_list_process_ing_ctr(&recipe, ingredient->pdata[option], division_num * key_num);
        }
    }
    return recipe;
}

static Recipe* rpsd_get_raw_recipe(RecipeGeneral* self)
{
    RecipeShaped* rpsd = RECIPE_SHAPED(self);
    Recipe* recipe = malloc(sizeof(Recipe));
    recipe->result = rpsd->result;
    recipe->arr = rpsd->key;
    recipe->pattern = rpsd->pattern;
    return recipe;
}

G_DEFINE_FINAL_TYPE(RecipeShaped, recipe_shaped, RECIPE_TYPE_GENERAL)

static void recipe_shaped_finalize(GObject* object)
{
    RecipeShaped* spd = RECIPE_SHAPED(object);
    g_ptr_array_unref(spd->key);
    dh_str_array_free(spd->pattern);
    G_OBJECT_CLASS(recipe_shaped_parent_class)->finalize(object);
}

static void recipe_shaped_class_init(RecipeShapedClass* klass)
{
    RECIPE_GENERAL_CLASS(klass)->set_content = rpsd_set_content;
    RECIPE_GENERAL_CLASS(klass)->get_recipe = rpsd_get_recipe;
    RECIPE_GENERAL_CLASS(klass)->get_raw_recipe = rpsd_get_raw_recipe;
    G_OBJECT_CLASS(klass)->finalize = recipe_shaped_finalize;
}

static void recipe_shaped_init(RecipeShaped* self)
{
    self->key = NULL;
    self->pattern = NULL;
}
