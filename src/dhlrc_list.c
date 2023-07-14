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

#include "dhlrc_list.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cjson/cJSON.h>
#include "recipe_util.h"
#include <dhelium/file_util.h>

typedef struct RListData{
    char* o_name;
    dh_StrArray* r_name;
} RListData;

typedef struct IListData{
    gchar* name;
    guint total;
    guint placed;
    guint available;
} IListData;

static int internal_strcmp(gconstpointer a, gconstpointer b)
{
    return strcmp(a,b);
}

static int rlistdata_strcmp(gconstpointer a, gconstpointer b)
{
    /* The first argument is GList Element data */
    return strcmp( ((RListData*)a)->o_name , b );
}

static int ilistdata_strcmp(gconstpointer a, gconstpointer b)
{
    return strcmp( ((IListData*)a)->name, b );
}

static int ilistdata_compare_by_total(gconstpointer a, gconstpointer b)
{
    return (((IListData*)a)->total - ((IListData*)b)->total);
}

static int ilistdata_iszero(gconstpointer a, gconstpointer b)
{
    b = NULL;
    return (((IListData*)a)->total - 0);
}

static IListData* ilistdata_init(const char* name)
{
    IListData* ildata = (IListData*)malloc(sizeof(IListData));
    ildata->name = dh_strdup(name);
    ildata->total = 0;
    ildata->placed = 0;
    ildata->available = 0;
    return ildata;
}

void ItemList_Free(ItemList* target)
{
    GList* gld = target;
    for(; gld ; gld = gld->next)
    {
        IListData* ildata = gld->data;
        free(ildata->name);
        free(ildata);
    }
    g_list_free(target);
}

void ItemList_Sort(ItemList** oBlock)
{
    /* Originally this sort function compares total.
     * After update, this will still sort by total */
    ItemList* iln = ItemList_Sort_ByTotal(*oBlock);
    *oBlock = iln;
}

ItemList * ItemList_Sort_ByTotal(ItemList* il)
{
    return g_list_sort(il, ilistdata_compare_by_total);
}


int ItemList_InitNewItem(ItemList** oBlock,char* block_name)
{
    //fprintf(stderr, "Item created!\n");
    GList* gl = *oBlock;
    IListData* ildata = ilistdata_init(block_name);
    gl = g_list_prepend(gl, ildata);
    *oBlock = gl;
    return 0;
}

ItemList *ItemList_Init(char* block_name)
{
    ItemList* il = NULL;
    ItemList_InitNewItem(&il, block_name);
    return il;
}

int ItemList_AddNum(ItemList** bl,int num,char* block_name)
{
    ItemList* item = g_list_find_custom(*bl, block_name, ilistdata_strcmp);
    if(item)
    {
        IListData* ildata = item->data;
        ildata->total = ildata->total + num;
        return 0;
    }
    else
    {
        /* In this new implement we could create item for it first */
        ItemList* il = *bl;
        ItemList_InitNewItem( &il, block_name);
        *bl = il;
        item = g_list_find_custom(il, block_name, ilistdata_strcmp);
        if(item)
        {
            IListData* ildata = item->data;
            ildata->total = ildata->total + num;
            return 0;
        }
        else return -1;
    }
}

void ItemList_AddNum_ByIndex(ItemList* il, gint num, gint index)
{
    ItemList* item = g_list_nth(il, index);
    if(item)
    {
        IListData* ildata = item->data;
        ildata->total = ildata->total + num;
    }
    else return;
}

gint ItemList_ItemIndex(ItemList* il, const char* item_name)
{
    ItemList* item = g_list_find_custom(il, item_name, ilistdata_strcmp);
    if(item)
        return g_list_position(il, item);
    else return -1;
}


int ItemList_ScanRepeat(ItemList* bl,char* block_name)
{
    ItemList* item = g_list_find_custom(bl, block_name, ilistdata_strcmp);
    if(item) return 1;
    else return 0;
}

int ItemList_DeleteItem(ItemList** bl,char* block_name)
{
    ItemList* il = *bl;
    GList* item = g_list_find_custom(il, block_name, ilistdata_strcmp);
    if(item)
    {
        il = g_list_remove(il, item->data);
        *bl = il;
        return 0;
    }
    else return -1;
}

void ItemList_DeleteZeroItem(ItemList** bl)
{
    ItemList* head = *bl;
    gint have_zero = 1;
    while(have_zero)
    {
        GList* item = g_list_find_custom(head, GINT_TO_POINTER(0), ilistdata_iszero);
        if(item)
        {
            head = g_list_remove(head, item->data);
            *bl = head;
        }
        else have_zero = 0;
    }
}

int ItemList_Combine(ItemList** dest, ItemList* src)
{
    ItemList* s = src;
    while(s)
    {
        IListData* ildata = s->data;
        ItemList_AddNum(dest, ildata->total, ildata->name);
        s = s->next;
    }
    return 0;
}

BlackList* BlackList_Init()
{
    GList* bl = NULL;
    cJSON* black_list = dhlrc_FileToJSON( "config/ignored_blocks.json" );
    if(black_list)
    {
        if(cJSON_IsArray(black_list))
        {
            int n = cJSON_GetArraySize(black_list);
            for(int i = 0 ; i < n ; i++)
            {
                char* item = cJSON_GetStringValue(cJSON_GetArrayItem(black_list,i));
                bl = BlackList_Extend(bl,item);
            }
        }
        cJSON_Delete(black_list);
        return bl;
    }
    else
    {
        bl = BlackList_Extend(bl,"minecraft:air");
        bl = BlackList_Extend(bl,"minecraft:piston_head");
        bl = BlackList_Extend(bl,"minecraft:fire");
        bl = BlackList_Extend(bl,"minecraft:soul_fire");
        bl = BlackList_Extend(bl,"minecraft:bubble_column");
    return bl;
    }
}

BlackList* BlackList_Extend(BlackList* bl,const char* name)
{
    bl = g_list_prepend(bl, dh_strdup(name));
    return bl;
}

void BlackList_Free(BlackList* bl)
{
    g_list_free_full(bl, free);
}

int BlackList_Scan(BlackList* bl,const char* name)
{
    if(!bl)
        return 0;
    else
    {
        GList* gl = g_list_find_custom(bl, name, internal_strcmp);
        if(gl) return 1;
        else return 0;
    }
}

ReplaceList* ReplaceList_Init()
{
    ReplaceList* rl = NULL;
    cJSON* rlist_o = dhlrc_FileToJSON("config/block_items.json");
    if(rlist_o)
    {
        cJSON* rlist = rlist_o->child;
        while(rlist)
        {
            if(cJSON_IsString(rlist))
                rl = ReplaceList_Extend(rl,rlist->string,rlist->valuestring);
            else if(cJSON_IsArray(rlist))
            {
                int size = cJSON_GetArraySize(rlist);
                dh_StrArray* str = NULL;
                for(int i = 0; i < size ; i++)
                {
                    char* r_name = cJSON_GetStringValue( cJSON_GetArrayItem(rlist, i) );
                    dh_StrArray_AddStr( &str, r_name);
                }
                rl = ReplaceList_Extend_StrArray(rl, rlist->string, str);
            }
            rlist = rlist->next;
        }
        cJSON_Delete(rlist_o);
        return rl;
    }
    else
    {
        rl = ReplaceList_Extend(rl,"minecraft:water","minecraft:water_bucket");
        rl = ReplaceList_Extend(rl,"minecraft:lava","minecraft:lava_bucket");
        rl = ReplaceList_Extend(rl,"minecraft:redstone_wall_torch","minecraft:redstone_torch");
        return rl;
    }
}

ReplaceList* ReplaceList_Extend(ReplaceList* rl,const char* o_name,const char* r_name)
{
    RListData* rld = (RListData*) malloc(sizeof(RListData));
    dh_StrArray* str = dh_StrArray_Init( r_name );
    rld->o_name = dh_strdup(o_name);
    rld->r_name = str;
    rl = g_list_prepend(rl, rld);
    return rl;
}

ReplaceList * ReplaceList_Extend_StrArray(ReplaceList* rl, const char* o_name, dh_StrArray* str)
{
    RListData* rld = (RListData*) malloc(sizeof(RListData));
    rld->o_name = dh_strdup(o_name);
    rld->r_name = str;
    rl = g_list_prepend(rl, rld);
    return rl;
}


const char* ReplaceList_Replace(ReplaceList* rl, const char* o_name)
{
    dh_StrArray* str = ReplaceList_Replace_StrArray(rl, o_name);
    if(str)
        return (str->val)[0];
    else return o_name;
}

dh_StrArray * ReplaceList_Replace_StrArray(ReplaceList* rl, const char* o_name)
{
    if(rl)
    {
        GList* gl =  g_list_find_custom(rl, o_name, rlistdata_strcmp);
        if(gl)
        {
            RListData* rld = gl->data;
            return rld->r_name;
        }
        else return NULL;
    }
    else return NULL;
}


void ReplaceList_Free(ReplaceList* rl)
{
    ReplaceList* rld = rl;
    for( ; rld ; rld = rld->next)
    {
        RListData* rldata = rld->data;
        dh_StrArray_Free(rldata->r_name);
        free(rldata->o_name);
        free(rldata);
    }
    g_list_free(rl);
}


int ItemList_GetItemNum(ItemList *il, char *item_name)
{
    GList* item = g_list_find_custom(il, item_name, ilistdata_strcmp);
    if(item)
    {
        IListData* ildata = item->data;
        return ildata->total;
    }
    else return -1;
}

int ItemList_toCSVFile(char* pos,ItemList* il)
{
    FILE* f = fopen(pos,"wb");
    fprintf(f,"\"Item\",\"Total\",\"Missing\",\"Available\"\n");
    ItemList* ild = il;
    while(ild)
    {
        IListData* ildata = ild->data;
        char* trans = Name_BlockTranslate(ildata->name);
        if(trans)
            fprintf(f,"\"%s\",%d,%d,%d\n",trans,ildata->total,ildata->total-ildata->placed,ildata->available);
        else
            fprintf(f,"%s,%d,%d,%d\n",ildata->name,ildata->total,ildata->total-ildata->placed,ildata->available);
        free(trans);
        ild = ild->next;
    }
    fclose(f);
    return 0;
}

const char * ItemList_ItemName(ItemList* il)
{
    if(il)
    {
        IListData* ildata = il->data;
        return ildata->name;
    }
    else return NULL;
}

