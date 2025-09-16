#include "dh_module.h"

#include "../common_info.h"
#include "../dhutil/dh_file_util.h"
#include "../dhutil/dh_type.h"
#include "conv_feature.h"
#include "mcdata_feature.h"
#include "recipe_feature.h"
#include "unzip_feature.h"

typedef void (*InitFunc) (DhModule *);

static GList *
dh_module_file_list_create (const char *path)
{
    GList *list = NULL;
    GDir *dir = g_dir_open (path, 0, NULL);
    const char *filename = NULL;
    while ((filename = g_dir_read_name (dir)) != NULL)
        list = g_list_append (list, g_strdup (filename));
    g_dir_close (dir);
    return list;
}

void
dh_search_module (const char *prpath)
{
    char *p = NULL;
#ifdef MODULEDIR
    p = g_strdup (MODULEDIR);
#else
    p = g_build_path (G_DIR_SEPARATOR_S, prpath, "module", NULL);
#endif
    GList *files = dh_module_file_list_create (p);
    GList *files_p = files;
    for (; files_p; files_p = files_p->next)
        {
            char *path
                = g_build_path (G_DIR_SEPARATOR_S, p, files_p->data, NULL);
            GError *err = NULL;
            GModule *module = g_module_open_full (path, 0, &err);
            g_free (path);
            if (err)
                {
                    g_critical ("%s", err->message);
                    g_error_free (err);
                    continue;
                }
            if (!module)
                {
                    g_critical ("???");
                    continue;
                }
            DhModule *new_module = g_new0 (DhModule, 1);
            new_module->module = module;
            InitFunc init_func = NULL;
            g_module_symbol (module, "init", (gpointer *)&init_func);
            if (init_func)
                {
                    init_func (new_module);
                }
            else
                continue;
            dh_info_new (DH_TYPE_MODULE, new_module,
                         g_date_time_new_now_local (), new_module->module_name,
                         NULL, NULL);
        }
    g_free (p);
    g_list_free_full (files, g_free);
    dhlrc_conv_enable ();
    dhlrc_mcdata_enable ();
    dhlrc_recipe_module_init ();
    dhlrc_unzip_enable ();
}

DhModule *
dh_search_inited_module (const char *name)
{
    const DhStrArray *arr = dh_info_get_all_uuid (DH_TYPE_MODULE);
    for (int i = 0; arr && i < arr->num; i++)
        {
            const char *description
                = dh_info_get_description (DH_TYPE_MODULE, arr->val[i]);
            if (g_str_equal (description, name))
                return dh_info_get_data (DH_TYPE_MODULE, arr->val[i]);
        }
    return NULL;
}

void
dh_module_free (gpointer data)
{
    DhModule *module = data;
    g_module_close (module->module);
    g_free (module->module_name);
    g_free (module->module_type);
    g_ptr_array_free (module->module_functions, TRUE);
    g_free (module);
}