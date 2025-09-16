#ifndef RECIPE_FEATURE_H
#define RECIPE_FEATURE_H

#include <glib.h>
#include <gmodule.h>
#include "dh_string_util.h"
#include <cjson/cJSON.h>

G_BEGIN_DECLS

typedef struct PatternTransformer{
    char pattern;
    DhStrArray* item_string;
} PatternTransformer;

typedef struct DhRecipes{
    guint x;
    guint y;
    DhStrArray* pattern;

    guint num;
    /* This is rare, it's always be 1 and be ignored. */
    double rate;

    /* Pattern Translator */
    PatternTransformer* pt;
    guint pt_num;
}DhRecipes;

typedef void (*RecipeInitFn)(const char*);

gboolean dhlrc_recipe_module_enabled();
void dhlrc_recipe_module_init();
void dhlrc_recipe_init(const char* libpath);
void dhlrc_recipe_module_clean();
gboolean dhlrc_recipe_is_supported(const char* filename);
gboolean dhlrc_recipe_is_supported_type(const char *type);
DhRecipes* dhlrc_get_recipes(const char* filename);
void dhlrc_pattern_translator_writer(PatternTransformer* pt, cJSON* json);
void dhlrc_pattern_translator_free(PatternTransformer* pt);
void dhlrc_recipes_free(DhRecipes* r);

G_END_DECLS

#endif