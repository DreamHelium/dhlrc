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
#include "dh_string_util.h"
#include "glib-object.h"
#include "libnbt/nbt.h"
#include "region.h"

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

NBT* nbt_dup(NBT* root)
{
    NBT* ret = NULL;
    GValue val = {0};

    if(!root)
        return ret;
    else if(root->type >= TAG_Byte && root->type <= TAG_Long)
    {
        g_value_init(&val, G_TYPE_INT64);
        g_value_set_int64(&val, root->value_i);
        ret = nbt_new(root->type, &val, 0, root->key);
    }
    else if(root->type == TAG_Float || root->type == TAG_Double)
    {
        g_value_init(&val, G_TYPE_DOUBLE);
        g_value_set_double(&val, root->value_d);
        ret = nbt_new(root->type, &val, 0, root->key);
    }
    else if(root->type == TAG_String)
    {
        g_value_init(&val, G_TYPE_STRING);
        g_value_set_string(&val, root->value_a.value);
        ret = nbt_new(root->type, &val, root->value_a.len, root->key);
    }
    else if(root->type == TAG_Byte_Array || root->type == TAG_Int_Array || root->type == TAG_Long_Array)
    {
        g_value_init(&val, G_TYPE_POINTER);
        g_value_set_pointer(&val, root->value_a.value);
        ret = nbt_new(root->type, &val, root->value_a.len, root->key);
    }
    else if(root->type == TAG_List || root->type == TAG_Compound)
    {
        NBT* head = NULL;
        NBT* prev = NULL;
        NBT* cur = NULL;
        NBT* origin_child = root->child;
        for(; origin_child ; origin_child = origin_child->next)
        {
            cur = nbt_dup(origin_child);
            if(origin_child == root->child) head = cur;
            cur->prev = prev;
            if(prev) prev->next = cur;
            prev = cur;
        }
        /* Then head is the new Child */
        g_value_init(&val, G_TYPE_POINTER);
        g_value_set_pointer(&val, head);
        ret = nbt_new(root->type, &val, 0, root->key);
    }
    g_value_unset(&val);
    return ret;
}

NBT* nbt_rm(NBT* root, const char* node)
{
    NBT* node_nbt = NBT_GetChild(root, node);
    /* First we get the prev and next of the node */
    if(node_nbt)
    {
        NBT* prev = node_nbt->prev;
        NBT* next = node_nbt->next;

        node_nbt->prev = NULL;
        node_nbt->next = NULL;
        NBT_Free(node_nbt);

        if(prev) prev->next = next;
        next->prev = prev;
        if(!prev) root->child = next;
    }
    return root;
}