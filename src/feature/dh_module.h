#ifndef DHLRC_DH_MODULE_H
#define DHLRC_DH_MODULE_H

#include <gmodule.h>

G_BEGIN_DECLS

typedef struct DhModule
{
    GModule *module;
    char *module_name;
    char *module_type;
    char *module_description;
    GPtrArray *module_functions;
} DhModule;

void dh_search_module (const char *prpath);
DhModule *dh_search_inited_module (const char *name);
void dh_module_free (gpointer data);

G_END_DECLS

#endif
