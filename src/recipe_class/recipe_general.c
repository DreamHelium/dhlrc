/*  recipe_general - Recipe General Object and class
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

#include "recipe_general.h"
#include "recipe_shaped.h"
#include "recipe_shapeless.h"
#include "recipe_smelting.h"
#include <cjson/cJSON.h>
#include <dhutil.h>
#include "../translation.h"
#include "../json_util.h"

typedef struct{
    char* type;
}RecipeGeneralPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(RecipeGeneral, recipe_general, G_TYPE_OBJECT)

static const char* rp_get_type(RecipeGeneral* general)
{
    RecipeGeneralPrivate* priv = recipe_general_get_instance_private(general);
    return priv->type;
}

static void rp_set_type(RecipeGeneral* general, const char* str)
{
    RecipeGeneralPrivate* priv = recipe_general_get_instance_private(general);
    priv->type = dh_strdup(str);
}

static void recipe_general_finalize(GObject* object)
{
    RecipeGeneralPrivate* priv = recipe_general_get_instance_private(RECIPE_GENERAL(object));
    free(priv->type);
    G_OBJECT_CLASS(recipe_general_parent_class)->finalize(object);
}

static void recipe_general_class_init(RecipeGeneralClass* klass)
{
    klass->get_type = rp_get_type;
    klass->set_type = rp_set_type;
    klass->set_content = NULL;
    klass->get_recipe = NULL;
    klass->get_raw_recipe = NULL;

    GObjectClass* object_klass = G_OBJECT_CLASS(klass);
    object_klass->finalize = recipe_general_finalize;
}

static void recipe_general_init(RecipeGeneral* self)
{
    RecipeGeneralPrivate* priv = recipe_general_get_instance_private(self);
    priv->type = NULL;
}

IngContainer* ing_ctr_new(cJSON* object)
{
    IngContainer* ctr = malloc(sizeof(IngContainer));
    g_return_val_if_fail(ctr != NULL, NULL);
    cJSON* item = cJSON_GetObjectItem(object, "item");
    if(item)
    {
        ctr->ingredient = dh_strdup(cJSON_GetStringValue(item));
        ctr->is_tag = FALSE;
    }
    else
    {
        cJSON* tag = cJSON_GetObjectItem(object, "tag");
        if(tag)
        {
            ctr->ingredient = dh_strdup(cJSON_GetStringValue(tag));
            ctr->is_tag = TRUE;
        }
    }
    return ctr;
}

void ing_ctr_free(gpointer data)
{
    IngContainer* data_d = (IngContainer*)data;
    free(data_d->ingredient);
    free(data_d);
}

void item_list_process_ing_ctr(ItemList** list, IngContainer* ctr, guint num)
{
    item_list_init_new_item_with_tag(list, ctr->ingredient, ctr->is_tag);
    item_list_add_num(list, num, ctr->ingredient);
}

int dh_mod_decide(guint num1, guint num2, DhGeneral* self)
{
    if(num2 == 0) return -1;
    int mod = num1 % num2;
    if(mod)
    {
        dh_new_win(self, FALSE);
        dh_printf(self, _("There is a remainder with %d and %d.\n"), num1, num2);
        int choice = dh_selector(self, _("Enter 'd' to discard the result, or enter 'c' to continue (c): ") ,0, "cd", _("&Continue"), _("&Discard"));
        if(choice == 0 || choice == -1) /* Return division number. */
        {
            return (num1 / num2) + 1;
        }
        else if(choice == -2) return 0;
        else return -1;
    }
    else return (num1 / num2);
}

RecipeGeneral* recipe_general_new(const char* filename)
{
    g_return_val_if_fail(recipe_is_supported(filename), NULL);
    RecipeGeneral* self = NULL;
    cJSON* json = dhlrc_file_to_json(filename);
    char* type = cJSON_GetStringValue(cJSON_GetObjectItem(json, "type"));

    /* Create object */
    if(g_str_has_suffix(type, ":crafting_shaped"))
        self = RECIPE_GENERAL(g_object_new(RECIPE_TYPE_SHAPED, NULL));
    else if(g_str_has_suffix(type, ":crafting_shapeless"))
        self = RECIPE_GENERAL(g_object_new(RECIPE_TYPE_SHAPELESS, NULL));
    else if(g_str_has_suffix(type, ":blasting") || g_str_has_suffix(type, ":smelting"))
        self = RECIPE_GENERAL(g_object_new(RECIPE_TYPE_SMELTING, NULL));
    else
    {
        cJSON_Delete(json);
        return NULL;
    }

    /* Set content */
    RECIPE_GENERAL_GET_CLASS(self)->set_content(self, json);
    return self;
}

gboolean recipe_is_supported(const char* filename)
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
