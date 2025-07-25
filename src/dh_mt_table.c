/*  mt_table - MT Safe Table
    Copyright (C) 2022 Dream Helium
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

#include "dh_mt_table.h"
#include "glib.h"

DhMTTable* dh_mt_table_new(GHashFunc hash_func, GEqualFunc key_equal_func, GDestroyNotify key_destroy_func, GDestroyNotify value_destroy_func)
{
    DhMTTable* new_table = g_new0(DhMTTable, 1);
    g_rw_lock_init(&(new_table->lock));
    new_table->table = g_hash_table_new_full(hash_func, key_equal_func, key_destroy_func, value_destroy_func);
    return new_table;
}

gboolean dh_mt_table_insert(DhMTTable* table, gpointer key, gpointer value)
{
    g_return_val_if_fail(table, FALSE);
    if(g_rw_lock_writer_trylock(&(table->lock)))
    {
        gboolean ret = g_hash_table_insert(table->table, key, value);
        g_rw_lock_writer_unlock(&(table->lock));
        return ret;
    }
    else return FALSE;
}

gboolean dh_mt_table_replace(DhMTTable* table, gpointer key, gpointer value)
{
    g_return_val_if_fail(table, FALSE);
    if(g_rw_lock_writer_trylock(&(table->lock)))
    {
        gboolean ret = g_hash_table_replace(table->table, key, value);
        g_rw_lock_writer_unlock(&(table->lock));
        return ret;
    }
    else return FALSE;
}

GList* dh_mt_table_get_keys(DhMTTable* table)
{
    g_return_val_if_fail(table, NULL);
    if(g_rw_lock_reader_trylock(&(table->lock)))
    {
        GList* ret = g_hash_table_get_keys(table->table);
        g_rw_lock_reader_unlock(&(table->lock));
        return ret;
    }
    else return NULL;
}

gpointer dh_mt_table_lookup(DhMTTable* table, gconstpointer key)
{
    g_return_val_if_fail(table, NULL);
    if(g_rw_lock_reader_trylock(&(table->lock)))
    {
        gpointer ret = g_hash_table_lookup(table->table, key);
        g_rw_lock_reader_unlock(&(table->lock));
        return ret;
    }
    else return NULL;
}

gboolean dh_mt_table_remove(DhMTTable* table, gconstpointer key)
{
    g_return_val_if_fail(table, FALSE);
    if(g_rw_lock_writer_trylock(&(table->lock)))
    {
        gboolean ret = g_hash_table_remove(table->table, key);
        g_rw_lock_writer_unlock(&(table->lock));
        return ret;
    }
    else return FALSE;
}

gboolean dh_mt_table_destroy(DhMTTable* table)
{
    g_return_val_if_fail(table, FALSE);
    if(table && g_rw_lock_writer_trylock(&(table->lock)))
    {
        g_hash_table_destroy(table->table);
        g_rw_lock_writer_unlock(&(table->lock));
        g_rw_lock_clear(&(table->lock));
        g_free(table);
        return TRUE;
    }
    else return FALSE;
}