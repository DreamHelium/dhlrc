#include "../handler.h"

const char* types[] = {"crafting_shaped", NULL};

const char* get_domain()
{ return NULL; }

const char** get_type()
{ return types; }

DhRecipes* get_recipes(cJSON* json)
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
    DhRecipes* recipes = g_new(DhRecipes, 1);
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

    recipes->pt = g_new0(PatternTransformer, key_num);

    for(int i = 0 ; i < key_num ; i++)
    {
        cJSON* key = cJSON_GetArrayItem(keys, i);
        recipes->pt[i].pattern = *(key->string);
        pattern_translator_writer(&(recipes->pt[i]), key);
    }
    return recipes;
}
