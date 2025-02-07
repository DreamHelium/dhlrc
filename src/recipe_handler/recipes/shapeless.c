#include "../handler.h"

const char* types[] = {"crafting_shapeless", NULL};

const char* get_domain()
{ return NULL; }

const char** get_type()
{ return types; }

DhRecipes* get_recipes(cJSON* json)
{
    /* Decide type of the json */
    g_return_val_if_fail(json != NULL, NULL);
    char* type = cJSON_GetStringValue(cJSON_GetObjectItem(json, "type"));
    g_return_val_if_fail(g_str_has_suffix(type, ":crafting_shapeless"), NULL);

    /* Set recipes data */
    DhRecipes* recipes = g_new0(DhRecipes, 1);
    recipes->x = 1;
    recipes->y = 0;

    /* Get result number */
    cJSON* count = cJSON_GetObjectItem(cJSON_GetObjectItem(json, "result"), "count");
    if(cJSON_IsNumber(count))
        recipes->num = cJSON_GetNumberValue(count);
    else recipes->num = 1;

    /* Get ingredient */
    cJSON* ingredient = cJSON_GetObjectItem(json, "ingredients");
    recipes->pt = g_new0(PatternTransformer, 1);
    pattern_translator_writer(recipes->pt, ingredient);
    recipes->pt_num = recipes->pt->item_string->num;
    DhStrArray* old_pt = recipes->pt->item_string;
    /* Rewrite ingredient */
    recipes->pt->item_string = NULL;
    recipes->pt = g_renew(PatternTransformer, recipes->pt, recipes->pt_num);
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