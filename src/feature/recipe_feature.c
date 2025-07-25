#include "recipe_feature.h"
#include "glib.h"
#include "gmodule.h"

typedef void (*AllFree)();
typedef void (*FreeFn)(DhRecipes*);
typedef gboolean (*Supported)(const char*);
typedef DhRecipes* (*GetRecipes)(const char*);
typedef void (*Writer)(PatternTransformer*, cJSON*);
typedef void (*PtFree)(PatternTransformer*);

static RecipeInitFn init_fn = NULL;
static AllFree all_free = NULL;
static Supported supported = NULL;
static Supported supported_type = NULL;
static GetRecipes get_recipes = NULL;
static Writer writer = NULL;
static PtFree pt_free = NULL;
static FreeFn free_fn = NULL;
static gboolean enabled = FALSE;

gboolean dhlrc_recipe_module_enabled()
{
    return enabled;
}

void dhlrc_recipe_module_init(GModule* module)
{
    if(module && g_module_symbol(module, "recipe_handler_init", (gpointer*)&init_fn)
    && g_module_symbol(module, "recipe_handler_free", (gpointer*)&all_free)
    && g_module_symbol(module, "recipes_is_supported", (gpointer*)&supported)
    && g_module_symbol(module, "recipes_is_supported_type", (gpointer*)&supported_type)
    && g_module_symbol(module, "recipes_gey_recipes", (gpointer*)&get_recipes)
    && g_module_symbol(module, "pattern_translator_writer", (gpointer*)&writer)
    && g_module_symbol(module, "pattern_translator_free", (gpointer*)&pt_free)
    && g_module_symbol(module, "recipes_free", (gpointer*)&free_fn))
        enabled = TRUE;
}

void dhlrc_recipe_init(const char* libpath)
{
    if(init_fn) init_fn(libpath);
}


void dhlrc_recipe_module_clean()
{
    if(all_free) all_free();
}

gboolean dhlrc_recipe_is_supported(const char* filename)
{
    if(supported) return supported(filename);
    return FALSE;
}

gboolean dhlrc_recipe_is_supported_type(const char *type)
{
    if(supported_type) return supported_type(type);
    return FALSE;
}

DhRecipes* dhlrc_get_recipes(const char* filename)
{
    if(get_recipes) return get_recipes(filename);
    return NULL;
}

void dhlrc_pattern_translator_writer(PatternTransformer* pt, cJSON* json)
{
    if(writer) writer(pt, json);
}

void dhlrc_pattern_translator_free(PatternTransformer* pt)
{
    if(pt_free) pt_free(pt);
}

void dhlrc_recipes_free(DhRecipes* r)
{
    if(free_fn) free_fn(r);
}