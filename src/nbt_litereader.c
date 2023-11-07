/*  nbt_litereader - nbt lite reader
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

#include "nbt_litereader.h"
#include <dh/dhutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "translation.h"

int nbtlr_instance(NBT* root, int from_parent, int modify_mode);
int nbtlr_instance_ng(NbtPos* pos, int modify_mode);
int nbtlr_Modifier_instance(NBT* root);
int nbtlr_save(NBT* root);



int nbtlr_start(NBT* root)
{
    NbtPos* pos = nbt_pos_init(root);
    int ret = nbtlr_instance_ng(pos, 0);
    nbt_pos_free(pos);
    return ret;
}

int nbtlr_start_pos(NbtPos* pos)
{
    return nbtlr_instance_ng(pos, 0);
}

int nbtlr_instance_ng(NbtPos* pos, int modify_mode)
{
    while(1)
    {
        system("clear");
        char* key;
        if(pos->level == 0 || pos->item != -1)
            key = pos->current->key;
        else
            key = nbtlr_to_next_nbt( pos->tree[pos->level - 1] , pos->child[pos->level - 1])->key;
        printf(_("The detail of NBT \"%s\" is listed below:\n\n"), key);
        int list = 0;

        // Show list (pos->item = -1 represents that it's in the tree, read items)
        list = nbtlr_list(pos->current, (pos->item == -1) );

        // modify_mode option
        dh_LineOut* input = NULL;
        if(modify_mode)
        {
            if(pos->level == 0 && list == 1 && pos->current->type == TAG_Compound)
                input = nbtlr_modifier_start(pos->current, 0);
            else input = nbtlr_modifier_start( pos->current, (pos->item == -1));
        }
        else
        {
            if(list > 0)
            {
                dh_limit* limit = dh_limit_Init(Integer);
                if(limit)
                {
                    dh_limit_AddInt(limit, 0, list-1);
                }
                else return -1;
                if(pos->level == 0){
                    printf(_("\nPlease enter a number to continue, or enter 'm' to modification mode, \
's' to save NBT file, 'q' to exit the program, 'e' to upper instance (q): "));
                    input = InputLine_General(sizeof(int64_t), limit, 0, "qmse", 1);
                }
                else
                {
                    printf(_("\nPlease enter a number to continue, or enter 'm' to modification mode, \
'p' to upper NBT, 's' to save NBT file, 'q' to exit the program, 'e' to upper instance (p): "));
                    input = InputLine_General(sizeof(int64_t), limit, 0, "pqmse", 1);
                }
                dh_limit_Free(limit);
            }
            else
            {
                printf(_("\nNo deeper NBT, please enter 'm' to modification mode, 'p' to upper NBT, \
's' to save NBT file, 'q' to exit the program, 'e' to upper instance (p): "));
                input = InputLine_General(0, NULL, 0, "pqmse", 1);
            }
        }
        if(input)
        {
            switch(input->type)
            {
            case Integer:
            {
                if(!nbt_pos_add_to_tree(pos, input->num_i))
                {
                    dh_LineOut_Free(input);
                    return -1;
                }
                dh_LineOut_Free(input);
                break;
            }
            case Character:
            {
                char opt = input->val_c;
                dh_LineOut_Free(input);
                if(opt == 'q')
                    return 0;
                if(opt == 'e')
                    return 1;
                if(opt == 'm')
                    modify_mode = 1;
                if(opt == 'b')
                    modify_mode = 0;
                if(opt == 'p')
                    if(!nbt_pos_delete_last(pos)) return -1;
                if(opt == 's')
                {
                    if(nbtlr_save(pos->tree[0]))
                        return 0;
                    else return -1;
                }
                break;
            }
            default:
            {
                dh_LineOut_Free(input);
                if( pos->level == 0 )
                    return 0;
                else nbt_pos_delete_last(pos);
                break;
            }
            }
        }
        else return -1;
    }
}

int nbtlr_list_item(NBT *given_nbt)
{
    NBT* list_nbt = given_nbt;
    int item = 0;
    while(list_nbt)
    {
        printf("[%2d] ",item);
        switch(list_nbt->type){
        case TAG_Compound:
            printf("(Compound) %s\n",list_nbt->key);
            break;

        case TAG_Byte:
            printf("(Byte) ");
            printf("%s %ld\n",list_nbt->key,list_nbt->value_i);
            break;
        case TAG_Short:
            printf("(Short) ");
            printf("%s %ld\n",list_nbt->key,list_nbt->value_i);
            break;
        case TAG_Int:
            printf("(Int) ");
            printf("%s %ld\n",list_nbt->key,list_nbt->value_i);
            break;
        case TAG_Long:
            printf("(Long) ");
            printf("%s %ld\n",list_nbt->key,list_nbt->value_i);
            break;

        case TAG_Float:
            printf("(Float) ");
            printf("%s %f\n",list_nbt->key,list_nbt->value_d);
            break;
        case TAG_Double:
            printf("(Double) ");
            printf("%s %f\n",list_nbt->key,list_nbt->value_d);
            break;

        case TAG_End:
            printf("(End) %s\n",list_nbt->key);
            break;

        case TAG_String:
            printf("(String) %s %s\n",list_nbt->key,(char*)list_nbt->value_a.value);
            break;
        case TAG_List:
            printf("(List) %s\n",list_nbt->key);
            break;
        case TAG_Byte_Array:
            printf("(Byte Array) ");
            printf("%s %d numbers.\n",list_nbt->key,list_nbt->value_a.len);
            break;
        case TAG_Int_Array:
            printf("(Int Array) ");
            printf("%s %d numbers.\n",list_nbt->key,list_nbt->value_a.len);
            break;
        case TAG_Long_Array:
            printf("(Long Array) ");
            printf("%s %d numbers.\n",list_nbt->key,list_nbt->value_a.len);
            break;
        default:
            printf("(?)\n");
            break;
        }
        item++;
        list_nbt = list_nbt->next;
    }
    return item;
}

/* The func's intention is to handle output of the main program.
 * If into a compound/list, the intention is to scan all items in the list.
 * But if ->child in, it would be the first.
 * So still to determine the parent then list the item.
 *
 */

int nbtlr_list(NBT *given_nbt, int read_next)
{
    NBT* list_nbt = given_nbt;
    if(read_next)
//        if(parent->type == TAG_Compound || parent->type == TAG_List)
            return nbtlr_list_item(list_nbt);  // It's the first element in the child so pass to scan it
    switch(list_nbt->type){
    case TAG_Compound:
    case TAG_List:
        return nbtlr_list_item(list_nbt);
    case TAG_End:
        printf("?\n");
        return -2;
    case TAG_Byte:
    case TAG_Short:
    case TAG_Int:
    case TAG_Long:
        printf("%ld\n",list_nbt->value_i);
        return -1;
    case TAG_Float:
    case TAG_Double:
        printf("%f\n",list_nbt->value_d);
        return -1;
    case TAG_String:
        printf("%s\n",(char*)list_nbt->value_a.value);
        return -1;
    case TAG_Byte_Array:
        for(int i = 0 ; i < list_nbt->value_a.len; i++)
            printf("[%d] %d\n",i,((int8_t*)list_nbt->value_a.value)[i]);
        return -1;
    case TAG_Int_Array:
        for(int i = 0 ; i < list_nbt->value_a.len; i++)
            printf("[%d] %d\n",i,((int32_t*)list_nbt->value_a.value)[i]);
        return -1;
    case TAG_Long_Array:
        for(int i = 0 ; i < list_nbt->value_a.len; i++)
            printf("[%d] %ld\n",i,((int64_t*)list_nbt->value_a.value)[i]);
        return -1;
    default: return -3;
    }
}

NBT *nbtlr_to_next_nbt(NBT *root, int n)
{
    NBT* next_nbt = root;
    /* Protection for "NULL" situation */
    for(int i = 0 ; i < n && next_nbt ; i++)
        next_nbt = next_nbt->next;
    return next_nbt;
}

dh_LineOut* nbtlr_modifier_start(NBT *root, int modify_list)
{
    while(1)
    {
        if(modify_list) // modify all in the list
        {
            printf(_("\nUnsupported modification mode request, will exit to reading mode."));
            fflush(stdout);
            sleep(5);
            return dh_LineOut_CreateChar('b');
        }
        else // only modify the item
        {
            if(root->type == TAG_Byte || root->type == TAG_Short || root->type == TAG_Int || root->type == TAG_Long)
            {
                int byte = 0;
                switch(root->type)
                {
                case TAG_Byte:
                    byte = 1;
                    break;
                case TAG_Short:
                    byte = 2;
                    break;
                case TAG_Int:
                    byte = 4;
                    break;
                case TAG_Long:
                    byte = 8;
                    break;
                default: break;
                }
                printf(_("\nPlease enter an integer, or enter 'b' to exit modification mode, 'p' to upper NBT, \
'q' to exit the program (p): "));
                dh_LineOut* out = InputLine_Get_OneOpt_WithByte(byte, 0, 1, 3, 'b', 'p', 'q');
                if(out)
                {
                    if(out->type == Integer)
                    {
                        int64_t result = out->num_i;
                        dh_LineOut_Free(out);
                        out = NULL;

                        printf(_("\nThe integer you input is %ld, please confirm [Y/n] (Y): "), result);
                        dh_LineOut* confirm = InputLine_Get_OneOpt(0, 0, 2, 'y', 'n');
                        if((confirm->type == Character && confirm->val_c == 'y') || confirm->type == Empty )
                        {
                            dh_LineOut_Free(confirm);
                            root->value_i = result;
                        }
                        else
                            dh_LineOut_Free(confirm);
                    }
                    else return out;
                }
                else return NULL;
            }
            else{
                printf(_("\nUnsupported modification mode request, will exit to reading mode."));
                fflush(stdout);
                sleep(5);
                return dh_LineOut_CreateChar('b');
            }
        }
    }
}

int nbtlr_Modifier_instance(NBT* root)
{
    return 0;
}

int nbtlr_save(NBT* root)
{
    int bit = 1;
    size_t len = 0;
#ifndef LIBNBT_USE_LIBDEFLATE
    size_t old_len = 0;
#endif
    uint8_t* data = NULL;
    while(1)
    {
        len = 1 << bit;
        data = (uint8_t*)malloc(len * sizeof(uint8_t));
        int ret = NBT_Pack(root, data, &len);
        if(ret == 0)
        {
#ifndef LIBNBT_USE_LIBDEFLATE
            if(old_len != len) // compress not finish due to a bug in old libnbt (in submodule)
            {
                old_len = len;
                free(data);
                bit++;
                continue;
            }
#endif
            char* input = NULL;
            size_t size = 0;
            if(dh_string_getline(&input, &size, stdin) != -1)
            {
                int str_len = strlen(input);
                input[str_len - 1] = 0;
                dhlrc_WriteFile(input, data, len);
                free(data);
                free(input);
                return 1;
            }
            else
            {
                free(input);
                return 0;
            }
        }
        else if(bit < 25)
        {
            free(data);
            bit++; // It might be not enough space
        }
        else
        {
            free(data);
            return 0;
        }
    }
    return 0;
}
