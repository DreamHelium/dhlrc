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

#ifndef MT_TABLE_H
#define MT_TABLE_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct DHMTTable {
    GHashTable* table;
    GRWLock lock;
} DhMTTable;

DhMTTable* dh_mt_table_new(GHashFunc hash_func, GEqualFunc key_equal_func, GDestroyNotify key_destroy_func, GDestroyNotify value_destroy_func);
gboolean dh_mt_table_insert(DhMTTable* table, gpointer key, gpointer value);
gboolean dh_mt_table_replace(DhMTTable* table, gpointer key, gpointer value);
GList* dh_mt_table_get_keys(DhMTTable* table);
gpointer dh_mt_table_lookup(DhMTTable* table, gconstpointer key);
gboolean dh_mt_table_remove(DhMTTable* table, gconstpointer key);
gboolean dh_mt_table_destroy(DhMTTable* table);

G_END_DECLS

#endif /* MT_TABLE_H */