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
#include "util/file_util.h"

typedef struct RListData{
    char* o_name;
    dh_StrArray* r_name;
} RListData;

static int internal_strcmp(gconstpointer a, gconstpointer b)
{
    return strcmp(a,b);
}

static int rlistdata_strcmp(gconstpointer a, gconstpointer b)
{
    /* The first argument is GList Element data */
    return strcmp( ((RListData*)a)->o_name , b );
}


void ItemList_Free(ItemList* target)
{
    if(target->next)
        ItemList_Free(target->next);
    target->next = NULL;
    free(target->name);
    target->name = NULL;
    free(target);
    target = NULL;
}

void ItemList_Sort(ItemList** oBlock)
{
    ItemList* head = *oBlock;
    ItemList* prev = NULL;
    ItemList* before_head = NULL;
    while(head)
    {
        ItemList* head_next = head->next;
        ItemList* temp = NULL;             // store bl before max num
        ItemList* temp_next = NULL;        // store bl on max num, then exchange to max num
        int max = head->num;
        ItemList* forward = head;  // staging forward

        while(forward->next)        // scan all
        {
            if(forward->next->num > max)
            {
                max = forward->next->num;
                temp = forward;
            }
            forward = forward->next;
        }
        // temp stores bl before max num, then a before head is created
        // it would be the before of the origin head
        if(temp)
        {
            if(temp == head) // two are besides
            {
                ItemList* temp_next_next = temp->next->next; //actually it's the head's next
                before_head = temp->next;   //actually head next, the max pos
                before_head->next = head;   //head is smaller one
                head->next = temp_next_next;
            }
            else
            {
                ItemList* temp_next_next = temp->next->next;
                temp_next = temp->next;
                before_head = temp_next; //it's on max bl.
                before_head->next = head_next;  // max num next point to the head's next since head would be moved
                temp->next = head; // head exchange to temp's next
                head->next = temp_next_next; // head's next is origin temp's next next
            }
            if(!prev)
            {
                *oBlock = before_head;
            }
            head = before_head;
            if(prev)
                prev->next = head;
        }
        prev = head;
        head = head->next;
    }
}

int ItemList_InitNewItem(ItemList** oBlock,char* block_name)
{
    if(*oBlock == NULL)
    {
        *oBlock = ItemList_Init(block_name);
        if(*oBlock) return 0;
        else return -1;
    }
    ItemList* next_item = *oBlock;
    while(next_item->next)
        next_item = next_item->next;
    next_item->next = (ItemList*)malloc(sizeof(ItemList));
    if(next_item->next)
    {
        next_item = next_item->next;
        next_item->len = strlen(block_name) + 1;
        next_item->name = (char*)malloc(next_item->len * sizeof(char));
        strcpy(next_item->name,block_name);
        next_item->num = 0;
        next_item->placed = 0;
        next_item->available = 0;
        next_item->next = NULL;
        return 0;
    }
    else
    {
        ItemList_Free(*oBlock);
        *oBlock = NULL;
        return -1;
    }

}

ItemList *ItemList_Init(char* block_name)
{
    ItemList* bl = (ItemList*)malloc(sizeof(ItemList));
    if(bl)
    {
        bl->len = strlen(block_name) + 1;
        bl->name = (char*)malloc(bl->len * sizeof(char));
        strcpy(bl->name,block_name);
        bl->num = 0;
        bl->placed = 0;
        bl->available = 0;
        bl->next = NULL;
        return bl;
    }
    else
    {
        free(bl);
        return NULL;
    }
}

int ItemList_AddNum(ItemList* bl,int num,char* block_name)
{
    while(bl)
    {
        if(!strcmp(bl->name,block_name))
        {
            bl->num += num;
            return 0;
        }
        bl = bl->next;
    }
    return -1;
}

int ItemList_ScanRepeat(ItemList* bl,char* block_name)
{
    while(bl)
    {
        if(!strcmp(bl->name,block_name))
            return 1;
        bl = bl->next;
    }
    return 0;
}

int ItemList_DeleteItem(ItemList** bl,char* block_name)
{
    ItemList* head = *bl;
    while(head)
    {
        if(head == *bl && !strcmp(head->name,block_name))
        {
            free(head->name);
            head->name = NULL;
            ItemList* head_next = head->next;
            free(head);
            head = NULL;
            *bl = head_next;
            return 0;
        }
        if(head->next)
        {
            if(!strcmp(head->next->name,block_name))
            {
                ItemList* head_next = head->next;
                ItemList* head_next_next = head->next->next;
                free(head_next->name);
                head_next->name = NULL;
                free(head_next);
                head_next = NULL;
                head->next = head_next_next;
                return 0;
            }
        }
        head = head->next;
    }
    return -1;
}

void ItemList_DeleteZeroItem(ItemList** bl)
{
    ItemList* head = *bl;
    ItemList* prev = NULL;
    while(head)
    {
        if(head->num == 0)
        {
            free(head->name);
            head->name = NULL;
            ItemList* head_next = head->next;
            free(head);
            head = NULL;
            if(!prev)
            {
                head = head_next;
                *bl = head;
            }
            else
            {
                head = head_next;
                prev->next = head;
            }
        }
        else
        {
            prev = head;
            head = head->next;
        }
    }
}

int ItemList_Combine(ItemList** dest, ItemList* src)
{
    ItemList* s = src;
    while(s)
    {
        if(!ItemList_ScanRepeat(*dest,s->name))
            if(ItemList_InitNewItem(dest,s->name))
                return -1;
        ItemList_AddNum(*dest,s->num,s->name);
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
    bl = g_list_prepend(bl, String_Copy(name));
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
    rld->o_name = String_Copy(o_name);
    rld->r_name = str;
    rl = g_list_prepend(rl, rld);
    return rl;
}

ReplaceList * ReplaceList_Extend_StrArray(ReplaceList* rl, const char* o_name, dh_StrArray* str)
{
    RListData* rld = (RListData*) malloc(sizeof(RListData));
    rld->o_name = String_Copy(o_name);
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
    ItemList* ild = il;
    while(ild)
    {
        if(!strcmp(item_name,ild->name))
            return ild->num;
        ild = ild->next;
    }
    return -1;
}

int ItemList_toCSVFile(char* pos,ItemList* il)
{
    FILE* f = fopen(pos,"wb");
    fprintf(f,"\"Item\",\"Total\",\"Missing\",\"Available\"\n");
    ItemList* ild = il;
    while(ild)
    {
        char* trans = Name_BlockTranslate(ild->name);
        if(trans)
            fprintf(f,"\"%s\",%d,%d,%d\n",trans,ild->num,ild->num-ild->placed,ild->available);
        else
            fprintf(f,"%s,%d,%d,%d\n",ild->name,ild->num,ild->num-ild->placed,ild->available);
        free(trans);
        ild = ild->next;
    }
    fclose(f);
    return 0;
}
