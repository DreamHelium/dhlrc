#include "script.h"
#include "common_info.h"
#include "global_variant.h"
#include "nbt_interface_cpp/nbt_interface.hpp"
extern "C"
{
#include <lauxlib.h>
#include <lualib.h>
}

static int
add_file (lua_State *L)
{
    int n = lua_gettop (L);
    lua_newtable (L);
    int num = 1;
    for (int i = 1; i <= n; i++)
        {
            if (lua_isstring (L, i))
                {
                    const char *filename = lua_tostring (L, i);
                    char *real_base_name = g_path_get_basename (filename);
                    auto instance = new DhNbtInstance (filename);
                    if (dhlrc_get_ignore_extension_name ())
                        {
                            char *base_name
                                = dh_get_filename_without_extension (
                                    real_base_name);
                            g_free (real_base_name);
                            real_base_name = base_name;
                        }
                    if ((*instance) ())
                        {
                            const char *uuid
                                = dh_info_new_short (DH_TYPE_NBT_INTERFACE_CPP,
                                                     instance, real_base_name);
                            lua_pushstring (L, uuid);
                            lua_seti (L, -2, num);
                            num++;
                        }
                    else
                        delete instance;
                    g_free (real_base_name);
                }
        }
    return 1;
}

static int
set_global (lua_State *L)
{
    int n = lua_gettop (L);
    if (n != 2 || !lua_isstring (L, 1))
        return 0;
    const char *key = lua_tostring (L, 1);
    if (g_str_equal (key, "IgnoreExtension"))
        {
            gboolean val = lua_toboolean (L, 2);
            dhlrc_set_ignore_extension_name (val);
            return 0;
        }
    else if (g_str_equal (key, "IgnoreAirList"))
        {
            lua_len (L, 2); /* The number is in 3 */
            int len = 0;
            lua_numbertointeger (lua_tonumber (L, 3), &len);
            GPtrArray *new_array = g_ptr_array_new_with_free_func (g_free);
            for (int i = 0; i < len; i++)
                {
                    lua_geti (L, 2, i + 1);
                    const char *str = lua_tostring (L, -1);
                    g_ptr_array_add (new_array, g_strdup (str));
                }
            dhlrc_set_ignore_air_list (new_array);
            return 0;
        }
    else
        return 0;
}

static int
get_all_uuid (lua_State *L)
{
    int n = lua_gettop (L);
    if (n == 0 || n > 1 || (n == 1 && !lua_isstring (L, 1)))
        {
            luaL_pushfail (L);
            return 1;
        }
    int type = dh_type_get_type (lua_tostring (L, 1));
    if (type == -1)
        {
            luaL_pushfail (L);
            return 1;
        }
    const DhStrArray *arr = dh_info_get_all_uuid (type);
    lua_newtable (L);
    for (int i = 0; i < arr->num; i++)
        {
            lua_pushstring (L, arr->val[i]);
            lua_seti (L, -2, i + 1);
        }
    return 1;
}

static int
get_all_description (lua_State *L)
{
    int n = lua_gettop (L);
    if (n != 2 || (!lua_istable (L, 2) && !lua_isstring (L, 1)))
        {
            luaL_pushfail (L);
            return 1;
        }
    const char *type_name = lua_tostring (L, 1);
    int type = dh_type_get_type (type_name);
    lua_len (L, 2);
    int len = 0;
    /* The length of table will push to top which is 3 */
    lua_numbertointeger (lua_tointeger (L, -1), &len);
    lua_newtable (L); /* The table is in 4 */
    for (int i = 0; i < len; i++)
        {
            lua_geti (L, 2, i + 1);
            const char *uuid = lua_tostring (L, -1);
            const char *description = dh_info_get_description (type, uuid);
            lua_pushstring (L, description);
            lua_seti (L, 4, i + 1);
        }
    lua_pushvalue (L, 4);
    return 1;
}

static int
save_nbt (lua_State *L)
{
    int n = lua_gettop (L);
    if (n != 2 || (!lua_isstring (L, 1) && !lua_isstring (L, 2)))
        {
            luaL_pushfail (L);
            return 1;
        }
    const char *uuid = lua_tostring (L, 1);
    const char *description
        = dh_info_get_description (DH_TYPE_NBT_INTERFACE_CPP, uuid);
    const char *path = lua_tostring (L, 2);
    char *real_path
        = g_build_path (G_DIR_SEPARATOR_S, path, description, NULL);
    auto instance = static_cast<DhNbtInstance *> (
        dh_info_get_data (DH_TYPE_NBT_INTERFACE_CPP, uuid));
    bool success = false;
    if (instance)
        success = instance->save_to_file (real_path);
    g_free (real_path);
    lua_pushboolean (L, success);
    return 1;
}

static int
modify_litematic_version (lua_State *L)
{
    int n = lua_gettop (L);
    if (n != 2 || (!lua_isstring (L, 1) && !lua_isnumber (L, 2)))
        {
            luaL_pushfail (L);
            return 1;
        }
    const char *uuid = lua_tostring (L, 1);
    auto instance = static_cast<DhNbtInstance *> (
        dh_info_get_data (DH_TYPE_NBT_INTERFACE_CPP, uuid));
    int version = lua_tointeger (L, 2);
    bool success = false;
    if (instance)
        {
            instance->goto_root ();
            if (g_str_equal (instance->get_key (), "Schematic"))
                {
                    luaL_pushfail (L);
                    return 1;
                }
            success = instance->child ("Version");
            if (success)
                {
                    NbtNode *node = instance->get_current_nbt ();
                    if (node)
                        {
                            auto data = static_cast<NbtData *> (node->data);
                            data->value_i = version;
                        }
                    else
                        success = false;
                }
            instance->goto_root ();
        }
    lua_pushboolean (L, success);
    return 1;
}

void
dhlrc_script_load_functions (lua_State *L)
{
    const DhStrArray *uuid_str = dh_info_get_all_uuid (DH_TYPE_FUNC);
    int len = uuid_str->num;
    for (int i = 0; i < len; i++)
        {
            char *uuid = uuid_str->val[i];
            const char *description
                = dh_info_get_description (DH_TYPE_FUNC, uuid);
            auto func = reinterpret_cast<lua_CFunction> (
                dh_info_get_data (DH_TYPE_FUNC, uuid));
            lua_register (L, description, func);
        }
}

void
test ()
{
    lua_State *L = luaL_newstate ();
    luaL_openlibs (L);
    dhlrc_script_load_functions (L);
    luaL_dofile (L, "script.lua");
    lua_close (L);
}

void
dhlrc_script_register_functions ()
{
    dh_info_new_short (DH_TYPE_FUNC, (gpointer)add_file, "add_file");
    dh_info_new_short (DH_TYPE_FUNC, (gpointer)get_all_uuid, "get_all_uuid");
    dh_info_new_short (DH_TYPE_FUNC, (gpointer)get_all_description,
                       "get_all_description");
    dh_info_new_short (DH_TYPE_FUNC, (gpointer)save_nbt, "save_nbt");
    dh_info_new_short (DH_TYPE_FUNC, (gpointer)modify_litematic_version,
                       "modify_litematic_version");
    dh_info_new_short (DH_TYPE_FUNC, (gpointer)set_global, "set_global");
}