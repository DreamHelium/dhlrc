/*  recipes_general - Recipes General Objects and Classes
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
#include "recipes_shaped.h"
#include "recipes_shapeless.h"
#include "recipes_smelting.h"
#include <cjson/cJSON.h>

static void pt_writer_item_tag(PatternTranslator* pt, cJSON* json);

G_DEFINE_TYPE(RecipesGeneral, recipes_general, G_TYPE_OBJECT)

static void recipes_general_init(RecipesGeneral* self)
{

}

static void recipes_general_class_init(RecipesGeneralClass* klass)
{
    klass->get_recipes = NULL;
}

gboolean recipes_is_supported(const char* filename)
{
    cJSON* json = dhlrc_file_to_json(filename);
    g_return_val_if_fail(json != NULL, FALSE);
    char* type = cJSON_GetStringValue(cJSON_GetObjectItem(json, "type"));
    if(
        g_str_has_suffix(type, ":crafting_shaped") || g_str_has_suffix(type, ":crafting_shapeless") || g_str_has_suffix(type, ":blasting") || g_str_has_suffix(type, ":smelting"))
    {
        cJSON_Delete(json);
        return TRUE;
    }
    else
    {
        cJSON_Delete(json);
        return FALSE;
    }
}

RecipesGeneral* recipes_general_new(const char* filename)
{
    g_return_val_if_fail(recipes_is_supported(filename), NULL);
    RecipesGeneral* self = NULL;

    cJSON* json = dhlrc_file_to_json(filename);
    char* type = cJSON_GetStringValue(cJSON_GetObjectItem(json, "type"));

    /* Create object */
    if(g_str_has_suffix(type, ":crafting_shaped"))
        self = RECIPES_GENERAL(g_object_new(RECIPES_TYPE_SHAPED, NULL));
    else if(g_str_has_suffix(type, ":crafting_shapeless"))
        self = RECIPES_GENERAL(g_object_new(RECIPES_TYPE_SHAPELESS, NULL));
    else if(g_str_has_suffix(type, ":blasting") || g_str_has_suffix(type, ":smelting"))
        self = RECIPES_GENERAL(g_object_new(RECIPES_TYPE_SMELTING, NULL));
    else
    {
        cJSON_Delete(json);
        return NULL;
    }
    cJSON_Delete(json);
    return self;
}

void pattern_translator_writer(PatternTranslator* pt, cJSON* json)
{
    if(cJSON_IsObject(json))
        pt_writer_item_tag(pt, json);
    else if(cJSON_IsArray(json))
    {
        guint arr_num = cJSON_GetArraySize(json);
        for(int i = 0 ; i < arr_num ; i++)
            pt_writer_item_tag(pt, cJSON_GetArrayItem(json, i));
    }
}

static void pt_writer_item_tag(PatternTranslator* pt, cJSON* json)
{
    cJSON* item = cJSON_GetObjectItem(json, "item");
    if(item)
        dh_str_array_add_str(&(pt->item_string), cJSON_GetStringValue(item));
    else
    {
        cJSON* tag = cJSON_GetObjectItem(json, "tag");
        if(tag)
            dh_str_array_add_str(&(pt->item_string), cJSON_GetStringValue(tag));
    }
}

void pattern_translator_free(PatternTranslator* pt)
{
    dh_str_array_free(pt->item_string);
    g_free(pt);
}

void recipes_free(Recipes* r)
{
    dh_str_array_free(r->pattern);
    pattern_translator_free(r->pt);
    g_free(r);
}

Recipes* recipes_get_recipes(const char* filename)
{
    RecipesGeneral* rg = recipes_general_new(filename);
    cJSON* json = dhlrc_file_to_json(filename);
    Recipes* r = RECIPES_GENERAL_GET_CLASS(rg)->get_recipes(rg, json);
    cJSON_Delete(json);
    g_object_unref(rg);
    return r;
}
