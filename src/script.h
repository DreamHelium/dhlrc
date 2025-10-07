#ifndef DHLRC_SCRIPT_H
#define DHLRC_SCRIPT_H

#include <glib.h>

G_BEGIN_DECLS

#include <lua.h>

void dhlrc_script_load_functions (lua_State *L);
void dhlrc_script_register_functions ();

G_END_DECLS

#endif
