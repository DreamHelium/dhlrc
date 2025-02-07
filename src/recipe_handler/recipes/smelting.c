#include "../handler.h"

const char* types[] = {"blasting", "smelting", NULL};

const char* get_domain()
{ return NULL; }

const char** get_type()
{ return types; }

DhRecipes* get_recipes(cJSON* json)
{
    /* Decide type of the json */
    g_return_val_if_fail(json != NULL, NULL);
    char* type = cJSON_GetStringValue(cJSON_GetObjectItem(json, "type"));
    g_return_val_if_fail(g_str_has_suffix(type, ":blasting") || g_str_has_suffix(type, ":smelting"), NULL);

    /* Set recipes data */
    DhRecipes* recipes = g_new0(DhRecipes, 1);
    recipes->x = 1;
    recipes->y = 1;
    dh_str_array_add_str(&(recipes->pattern), "x");

    /* Get result number */
    cJSON* count = cJSON_GetObjectItem(cJSON_GetObjectItem(json, "result"), "count");
    if(cJSON_IsNumber(count))
        recipes->num = cJSON_GetNumberValue(count);
    else recipes->num = 1;

    /* Get ingredient */
    cJSON* ingredient = cJSON_GetObjectItem(json, "ingredient");
    recipes->pt = g_new0(PatternTransformer, 1);
    recipes->pt->pattern = 'x';
    recipes->pt_num = 1;
    pattern_translator_writer(recipes->pt, ingredient);
    return recipes;
}