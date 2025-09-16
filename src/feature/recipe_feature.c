#include "recipe_feature.h"

#include "dh_module.h"
#include "glib.h"
#include "gmodule.h"

typedef void (*AllFree) ();
typedef void (*FreeFn) (DhRecipes *);
typedef gboolean (*Supported) (const char *);
typedef DhRecipes *(*GetRecipes) (const char *);
typedef void (*Writer) (PatternTransformer *, cJSON *);
typedef void (*PtFree) (PatternTransformer *);

static RecipeInitFn init_fn = NULL;
static AllFree all_free = NULL;
static Supported supported = NULL;
static Supported supported_type = NULL;
static GetRecipes get_recipes = NULL;
static Writer writer = NULL;
static PtFree pt_free = NULL;
static FreeFn free_fn = NULL;
static gboolean enabled = FALSE;

gboolean
dhlrc_recipe_module_enabled ()
{
    return enabled;
}

void
dhlrc_recipe_module_init ()
{
    DhModule *module = dh_search_inited_module ("recipe");
    if (module)
        {
            init_fn = module->module_functions->pdata[0];
            all_free = module->module_functions->pdata[1];
            supported = module->module_functions->pdata[2];
            supported_type = module->module_functions->pdata[3];
            get_recipes = module->module_functions->pdata[4];
            writer = module->module_functions->pdata[5];
            pt_free = module->module_functions->pdata[6];
            free_fn = module->module_functions->pdata[7];
            enabled = TRUE;
        }
}

void
dhlrc_recipe_init (const char *libpath)
{
    if (init_fn)
        init_fn (libpath);
}

void
dhlrc_recipe_module_clean ()
{
    if (all_free)
        all_free ();
}

gboolean
dhlrc_recipe_is_supported (const char *filename)
{
    if (supported)
        return supported (filename);
    return FALSE;
}

gboolean
dhlrc_recipe_is_supported_type (const char *type)
{
    if (supported_type)
        return supported_type (type);
    return FALSE;
}

DhRecipes *
dhlrc_get_recipes (const char *filename)
{
    if (get_recipes)
        return get_recipes (filename);
    return NULL;
}

void
dhlrc_pattern_translator_writer (PatternTransformer *pt, cJSON *json)
{
    if (writer)
        writer (pt, json);
}

void
dhlrc_pattern_translator_free (PatternTransformer *pt)
{
    if (pt_free)
        pt_free (pt);
}

void
dhlrc_recipes_free (DhRecipes *r)
{
    if (free_fn)
        free_fn (r);
}