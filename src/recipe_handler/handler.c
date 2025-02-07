/*  handler - Recipes Handler
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

#include "handler.h"
#include "dh_file_util.h"
#include "dh_string_util.h"
#include "glib.h"
#include "glibconfig.h"
#include "../json_util.h"
#include <cjson/cJSON.h>
#include <gmodule.h>

typedef GPtrArray RecipesArray;
typedef DhRecipes* (*DhGetRecipes)(cJSON* json);
typedef const char* (*DhGetRecipeDomain)();
typedef const char** (*DhGetRecipeType)();

typedef struct Handler
{
    GModule* module;
    const char* recipe_domain;
    const char** recipe_type;
    DhGetRecipes get_recipes;
} Handler;

static RecipesArray* all_handlers = NULL;

void handler_free(gpointer mem)
{
    Handler* h = mem;
    g_module_close(h->module);
    g_free(h);
}

void recipe_handler_init(const char* recipelib_path)
{
    GList* filenames = dh_file_list_create(recipelib_path);
    if(filenames)
    {
        all_handlers = g_ptr_array_new_with_free_func(handler_free);
        for(int i = 0 ; i < g_list_length(filenames) ; i++)
        {
            gboolean success = TRUE;
            GError* err = NULL;
            char* dir = g_strconcat(recipelib_path, G_DIR_SEPARATOR_S, g_list_nth_data(filenames, i) ,NULL);
            GModule* module = g_module_open_full(dir, G_MODULE_BIND_MASK, &err);
            g_free(dir);

            if(!module)
            {
                g_critical("%s", err->message);
                g_error_free(err);
                continue; /* Get a module fail */
            }

            DhGetRecipeDomain get_domain;
            DhGetRecipeType get_type;
            DhGetRecipes get_recipes;

            if(success) success = g_module_symbol(module, "get_domain", (gpointer*)&get_domain);
            if(success) success = g_module_symbol(module, "get_type", (gpointer*)&get_type);
            if(success) success = g_module_symbol(module, "get_recipes", (gpointer*)&get_recipes);

            if(success)
            {
                Handler* handler = g_new0(Handler, 1);
                handler->module = module;
                handler->recipe_type = get_type();
                handler->recipe_domain = get_domain();
                handler->get_recipes = get_recipes;
                g_ptr_array_add(all_handlers, handler);
            }
            else
            {
                g_critical("Load failed!");    
            }
        }
        g_list_free_full(filenames, free);
    }
}

void recipe_handler_free()
{
    g_ptr_array_free(all_handlers, TRUE);
}

gboolean recipes_is_supported(const char* filename)
{
    cJSON* json = dhlrc_file_to_json(filename);
    g_return_val_if_fail(json, FALSE);
    char* type = cJSON_GetStringValue(cJSON_GetObjectItem(json, "type"));
    gboolean ret = recipes_is_supported_type(type);
    cJSON_Delete(json);
    return ret;
}

gboolean recipes_is_supported_type(const char* type)
{
    char** type_sep = g_strsplit(type, ":", 2);
    gboolean ret = FALSE;
    for(int i = 0 ; i < all_handlers->len ; i++)
    {
        Handler* handler = all_handlers->pdata[i];
        if(handler->recipe_domain == NULL || g_str_equal(type_sep[0], handler->recipe_domain))
        {
            const char** type = handler->recipe_type;
            for(; *type ; type++)
            {
                if(g_str_equal(*type, type_sep[1]))
                    ret = TRUE;
            }
        }
    }
    g_strfreev(type_sep);
    return ret;
}

DhRecipes* recipes_get_recipes(const char* filename)
{
    cJSON* json = dhlrc_file_to_json(filename);
    g_return_val_if_fail(json, FALSE);
    char* type = cJSON_GetStringValue(cJSON_GetObjectItem(json, "type"));
    char** type_sep = g_strsplit(type, ":", 2);
    DhRecipes* ret = NULL;
    for(int i = 0 ; i < all_handlers->len ; i++)
    {
        Handler* handler = all_handlers->pdata[i];
        if(handler->recipe_domain == NULL || g_str_equal(type_sep[0], handler->recipe_domain))
        {
            const char** type = handler->recipe_type;
            for(; *type ; type++)
            {
                if(g_str_equal(*type, type_sep[1]))
                {
                    ret = handler->get_recipes(json);
                }
            }
        }
    }
    cJSON_Delete(json);
    g_strfreev(type_sep);
    return ret;
}

static void pt_writer_item_tag(PatternTransformer* pt, cJSON* json)
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

void pattern_translator_writer(PatternTransformer* pt, cJSON* json)
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

void pattern_translator_free(PatternTransformer* pt)
{
    dh_str_array_free(pt->item_string);
}

void recipes_free(DhRecipes* r)
{
    dh_str_array_free(r->pattern);
    for(int i = 0 ; i < r->pt_num ; i++)
    {
        pattern_translator_free(&(r->pt[i]));
    }
    g_free(r->pt);
    g_free(r);
}