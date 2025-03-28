/*  nbt_pos - NBT_Pos struct
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

#include "nbt_pos.h"
#include "nbt_util.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

NbtPos* nbt_pos_init(NBT* root)
{
    if(root)
    {
        NbtPos* out = (NbtPos*)malloc(sizeof(NbtPos));
        if(out)
        {
            out->level = 0;
            out->tree = (NBT**)malloc(sizeof(NBT*));
            out->tree[0] = root;
            out->child = NULL;
            out->current = root;
            out->item = -1;
            return out;
        }
        else return NULL;
    }
    else return NULL;
}

int nbt_pos_add_to_tree(NbtPos* pos, int n)
{
    NBT* current = nbtlr_to_next_nbt(pos->tree[pos->level], n); // Analyse what's next
    if(current)
    {
        if(current->type == TAG_Compound || current->type == TAG_List)
        {
            current = current->child; // into the child, the root of the next tree.
            pos->current = current;
            pos->level++;
            int* new_child = (int*)realloc(pos->child, pos->level * sizeof(int));
            if(new_child)
            {
                pos->child = new_child;
                pos->child[pos->level - 1] = n;
                NBT** new_tree = (NBT**)realloc(pos->tree, (pos->level + 1) * sizeof(NBT*));
                if(new_tree)
                {
                    pos->tree = new_tree;
                    pos->tree[pos->level] = current;
                    /* Hmmm, pos->item should be -1 before but maybe a bug encountered before */
                    pos->item = -1;
                    return 1;
                }
                else return 0;
            }
            else return 0;
        }
        else
        {
            // It's not tree
            pos->current = current;
            pos->item = n;
            return 1;
        }
    }
    else return 0; // current is null, which is unexpected
}

int nbt_pos_delete_last(NbtPos* pos)
{
    if(pos->item != -1)
    {
        pos->item = -1;
        pos->current = pos->tree[pos->level];
        return 1;
    }
    else
    {
        pos->level--;
        if(pos->level < 0)
        {
            return 0; // or libclang complain about garbage data
        }
        else
        {
            if(pos->level == 0)
            {
                free(pos->child);
                pos->child = NULL;
            }
            else
            {
                int* new_child = (int*)realloc(pos->child, pos->level * sizeof(int));
                if(new_child)
                {
                    pos->child = new_child;
                }
                else return 0;
            }
            NBT** new_tree = (NBT**)realloc(pos->tree, (pos->level + 1) * sizeof(NBT*));
            if(new_tree)
            {
                pos->tree = new_tree;
                pos->current = pos->tree[pos->level];
                return 1;
            }
            else return 0;
        }
    }
}

int nbt_pos_get_child(NbtPos* pos, const char* key)
{
    if(pos)
    {
        if(key == NULL) return 1;
        else{
            NBT* current = pos->current;
            /* Try to scan the list first */
            int item = 0;
            int success = 0;
            while(current)
            {
                if(current->key){
                    if(!strcmp(current->key, key))
                    {
                        success = 1;
                        break;
                    }
                }
                item++;
                current = current->next;
            }
            if(success)
                return nbt_pos_add_to_tree( pos, item );
            else
            {
                /* Try to enter and scan */
                int ret = nbt_pos_add_to_tree(pos, (pos->item == -1)? 0: pos->item);
                if(ret)
                {
                    NBT* current = pos->current;
                    int item = 0;
                    int success = 0;
                    while(current)
                    {
                        if(current->key){
                            if(!strcmp(current->key, key))
                            {
                                success = 1;
                                break;
                            }
                        }
                        item++;
                        current = current->next;
                    }
                    if(success)
                        return nbt_pos_add_to_tree(pos, item);
                    else return 0;
                }
                else return 0;
            }
        }
    }
    else return 0;
}

int nbt_pos_get_child_deep(NbtPos* pos, ...)
{
    if(pos)
    {
        va_list va;
        va_start(va, pos);
        char* temp = NULL;
        while( (temp = va_arg(va, char*)) != NULL )
        {
            if(!nbt_pos_get_child(pos, temp))
            {
                va_end(va);
                return 0;
            }
        }
        va_end(va);
        return 1;
    }
    else return 0;
}

NbtPos * nbt_pos_copy(NbtPos* pos)
{
    NbtPos* new_pos = (NbtPos*)malloc(sizeof(NbtPos));
    if(new_pos)
    {
        NBT** new_tree = (NBT**)malloc( (pos->level + 1) * sizeof(NBT*) );
        if(new_tree)
        {
            int* new_child = (int*)malloc( (pos->level) * sizeof(int) );
            if(new_child)
            {
                memcpy( new_tree, pos->tree, (pos->level + 1) * sizeof(NBT*) );
                memcpy( new_child , pos->child, (pos->level) * sizeof(int) );
                new_pos->child = new_child;
                new_pos->tree = new_tree;
                new_pos->current = pos->current;
                new_pos->item = pos->item;
                new_pos->level = pos->level;
                return new_pos;
            }
            else
            {
                free(new_tree);
                free(new_pos);
                return NULL;
            }
        }
        else{
            free(new_pos);
            return NULL;
        }
    }
    else return NULL;
}

NBT * nbt_pos_get_item_nbt(NbtPos* pos, const char* key)
{
    if(key && pos)
    {
        NBT* current = pos->current;
        while(current)
        {
            if(current->key)
                if(!strcmp(current->key, key))
                    return current;
            current = current->next;
        }
        return NULL;
    }
    else return NULL;
}


void nbt_pos_free(NbtPos* pos)
{
    free(pos->child);
    free(pos->tree);
    free(pos);
}
