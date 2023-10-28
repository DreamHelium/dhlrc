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

#ifndef RECIPE_GENERAL_H
#define RECIPE_GENERAL_H

#include <glib-object.h>
#include <cjson/cJSON.h>
#include <dh/dhutil.h>
#include "../dhlrc_list.h"

G_BEGIN_DECLS

#define RECIPE_TYPE_GENERAL recipe_general_get_type()
G_DECLARE_DERIVABLE_TYPE(RecipeGeneral, recipe_general, RECIPE, GENERAL, GObject)

typedef struct IngredientContainer{
    char* ingredient;
    gboolean is_tag;
} IngContainer;

typedef struct Recipe{
    guint result;
    dh_StrArray* pattern;
    GPtrArray* arr;
}Recipe;

struct _RecipeGeneralClass{
    GObjectClass parent_class;

    const char* (*get_type)(RecipeGeneral* general);
    void (*set_type)(RecipeGeneral* general, const char* str);
    void (*set_content)(RecipeGeneral* self, cJSON* json);
    ItemList* (*get_recipe)(RecipeGeneral* self, guint num, DhGeneral* dh_general);
    Recipe* (*get_raw_recipe)(RecipeGeneral* self);
};

IngContainer* IngCtr_new(cJSON* object);
void IngCtr_free(gpointer data);
void ItemList_ProcessIngCtr(ItemList** list, IngContainer* ctr, guint num);
int dh_mod_decide(guint num1, guint num2, DhGeneral* self);
RecipeGeneral* recipe_general_new(const char* filename);
gboolean recipe_is_supported(const char* filename);

G_END_DECLS

#endif
