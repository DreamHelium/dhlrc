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

#ifdef __cplusplus
extern "C" {
#endif

#include <gmodule.h>
#include "util/dh_string_util.h"

typedef GList ItemList;
typedef GList BlackList;
typedef GList ReplaceList;


/* ItemList utils */

void ItemList_Free(ItemList* target);
void ItemList_Sort(ItemList **oBlock);
ItemList* ItemList_Sort_ByTotal(ItemList* il);


int ItemList_InitNewItem(ItemList **oBlock, char* block_name);
int ItemList_AddNum(ItemList** bl, int num , char* block_name);
int ItemList_ScanRepeat(ItemList* bl,char* block_name);
int ItemList_DeleteItem(ItemList** bl,char* block_name);
void ItemList_DeleteZeroItem(ItemList** bl);
int ItemList_Combine(ItemList** dest,ItemList* src);
int ItemList_GetItemNum(ItemList* il, char* item_name);
ItemList* ItemList_Init(char* block_name);
int ItemList_toCSVFile(char* pos,ItemList* il);
const char* ItemList_ItemName(ItemList* il);


BlackList* BlackList_Init();
void BlackList_Free(BlackList* bl);
BlackList* BlackList_Extend(BlackList* bl, const char* name);
int BlackList_Scan(BlackList* bl,const char* name);



ReplaceList* ReplaceList_Init();
ReplaceList* ReplaceList_Extend(ReplaceList* rl, const char* o_name, const char* r_name);
ReplaceList* ReplaceList_Extend_StrArray(ReplaceList* rl, const char* o_name, dh_StrArray* str);
/** DEPRECATED */
const char* ReplaceList_Replace(ReplaceList* rl, const char* o_name);
dh_StrArray* ReplaceList_Replace_StrArray(ReplaceList* rl, const char* o_name);
void ReplaceList_Free(ReplaceList* rl);

//only for debug
//int Test();


#ifdef __cplusplus
}
#endif
#endif // DHLRC_LIST_H
