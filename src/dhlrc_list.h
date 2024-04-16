/*  dhlrc_list - useful linked lists
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

#ifndef DHLRC_LIST_H
#define DHLRC_LIST_H

#include <gmodule.h>
#include <dhutil.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef GList ItemList;
typedef GList BlackList;
typedef GList ReplaceList;
typedef GList RecipeList;

typedef struct IListData{
    gchar* name;
    guint total;
    guint placed;
    guint available;
    gboolean is_tag;
} IListData;

/* ItemList utils */


void        item_list_read(ItemList* il, DhGeneral* general);
void        item_list_free(ItemList* target);
void        item_list_sort(ItemList **oBlock);
ItemList*   item_list_sort_by_total(ItemList* il);
int         item_list_init_new_item(ItemList **oBlock, const char* block_name);
int         item_list_init_new_item_with_tag(ItemList **oBlock, const char* block_name, gboolean is_tag);
int         item_list_add_num(ItemList** bl, int num , char* block_name);
int         item_list_scan_repeated(ItemList* bl,char* block_name);
int         item_list_delete_item(ItemList** bl,char* block_name);
void        item_list_delete_zero_item(ItemList** bl);
int         item_list_combine(ItemList** dest,ItemList* src);
int         item_list_get_item_num(ItemList* il, char* item_name);
ItemList*   item_list_init(const char* block_name);
ItemList*   item_list_init_with_tag(const char* block_name, gboolean is_tag);
int         item_list_to_csv(char* pos,ItemList* il);
const char* item_list_item_name(ItemList* il);
void        item_list_add_num_by_index(ItemList* il, gint num, gint index);
gint        item_list_item_index(ItemList* il, const char* item_name);
ItemList*   item_list_recipe(RecipeList* rcl, int num, const char* item_name, DhGeneral* general);


BlackList*  black_list_init();
void        black_list_free(BlackList* bl);
BlackList*  black_list_extend(BlackList* bl, const char* name);
int         black_list_scan(BlackList* bl,const char* name);

ReplaceList*    replace_list_init();
ReplaceList*    replace_list_extend(ReplaceList* rl, const char* o_name, const char* r_name);
ReplaceList*    replace_list_extend_str_array(ReplaceList* rl, const char* o_name, DhStrArray* str);
G_DEPRECATED_FOR(replace_list_replace_str_array)
const char*     replace_list_replace(ReplaceList* rl, const char* o_name);
DhStrArray*    replace_list_replace_str_array(ReplaceList* rl, const char* o_name);
void            replace_list_free(ReplaceList* rl);

void            recipe_list_enable_feature(gboolean shaped, gboolean smelting, gboolean shapeless);
RecipeList*     recipe_list_init(const char* dir, ItemList* il);
void            recipe_list_free(RecipeList* rcl);
char*           recipe_list_filename(RecipeList* rcl);
char*           recipe_list_item_name(RecipeList* rcl);
DhStrArray*    recipe_list_item_names(RecipeList* rcl);
DhStrArray*    recipe_list_item_names_with_namespace(RecipeList* rcl);
//only for debug
//int Test();


#ifdef __cplusplus
}
#endif
#endif // DHLRC_LIST_H
