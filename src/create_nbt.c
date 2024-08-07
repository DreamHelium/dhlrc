/*  create_nbt - Create own NBT sturcture
    Copyright (C) 2024 Dream Helium
    This file is part of litematica_reader_c.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>. */

#include "create_nbt.h"
#include "dhutil.h"
#include "glibconfig.h"
#include "libnbt/nbt.h"

NBT* fail_free(NBT* root)
{
    free(root->key);
    free(root);
    return NULL;
}

NBT* nbt_new(NBT_Tags tag, GValue* val, int len, const char* key)
{
    NBT* new_nbt = malloc(sizeof(NBT));
    memset(new_nbt, 0, sizeof(NBT));
    new_nbt->type = tag;
    if(key) new_nbt->key = dh_strdup(key);
    else new_nbt->key = NULL;

    /* Expected int is int64 */
    if(tag >= TAG_Byte && tag <= TAG_Long)
    {
        if(G_VALUE_HOLDS_INT64(val))
        {
            new_nbt->value_i = g_value_get_int64(val);
            return new_nbt;
        }
        else return fail_free(new_nbt);
    }

    /* Expected float is double */
    else if(tag == TAG_Float || tag == TAG_Double)
    {
        if(G_VALUE_HOLDS_DOUBLE(val))
        {
            new_nbt->value_d = g_value_get_double(val);
            return new_nbt;
        }
        else return fail_free(new_nbt);
    }
    else if(tag == TAG_String)
    {
        if(G_VALUE_HOLDS_STRING(val))
        {
            new_nbt->value_a.value = dh_strdup(g_value_get_string(val));
            new_nbt->value_a.len = strlen(new_nbt->value_a.value) + 1;
            return new_nbt;
        }
        else return fail_free(new_nbt);
    }
    else if(tag == TAG_List || TAG_Compound)
    {
        if(G_VALUE_HOLDS_POINTER(val))
        {
            new_nbt->child = g_value_get_pointer(val);
            return new_nbt;
        }
        else return fail_free(new_nbt);
    }
    else if(tag == TAG_Byte_Array || tag == TAG_Int_Array || tag == TAG_Long_Array)
    {
        if(G_VALUE_HOLDS_POINTER(val))
        {
            int byte = 0;
            if(tag == TAG_Byte_Array) byte = sizeof(gint8);
            if(tag == TAG_Int_Array)  byte = sizeof(gint32);
            if(tag == TAG_Long_Array) byte = sizeof(gint64);

            void* new_array = malloc(len * byte);
            memcpy(new_array, g_value_get_pointer(val), len * byte);
            new_nbt->value_a.value = new_array;
            new_nbt->value_a.len = len;
            return new_nbt;
        }
        else return fail_free(new_nbt);
    }
    else return fail_free(new_nbt);
}