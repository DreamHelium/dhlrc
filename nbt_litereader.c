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
#include <stdio.h>
#include <stdlib.h>

int nbtlr_Start(NBT *root, NBT *parent)
{
    // The start entry is from main or "input number"
    // So parent should be NULL at first, then
    // passed nbtlr_Start(root[i],root).
    // After calling "parent" it should return to the old func. so how?
    // And which is the parent? Just try to pass the same as parent.
    // Firstly parent is null and then go to child.
    // The parent should be the entry and the child is done in func.
    // So parent is not needed in this func but in "List" since it will
    // determine whether to read list or read value. With an exception,
    // call in the main.


    NBT* read_nbt = root;
    int continue_func = 1;
    while(continue_func){
        system("clear");
        printf("The NBT details are below:\n\n");

        /* Two cases:
         * One is no parent NBT, just use the root.
         * One is reading content in the NBT, still just use the root.
         * If not the cases above, enter child and scan.
         */

        if(parent)
            if(root->type == TAG_List || root->type == TAG_Compound)
                read_nbt = root->child;
        int list = nbtlr_List(read_nbt,parent);
        if(list <= 0) // also return to origin func
        {
            printf("\nNo deeper NBT, press any key to continue.");
            getchar();
            return 1;
        }
        printf("\nInput the number to continue, or p to parent NBT, q to quit: ");
        char* input = NULL;
        size_t size = 0;
        while(getline(&input, &size, stdin) != -1)
        {
            char* inputl = input;
            while(inputl[0] == ' ')
                inputl++;
            if(*inputl == 'p')   // return to origin func, return 1
            {
                inputl++;
                while(*inputl == ' ')
                    inputl++;
                if(*inputl != '\n'){
                    printf("Unrecognized string! Please enter again: ");
                    continue;
                }
                if(!parent){
                    printf("You are in root NBT, please choose again: ");
                    continue;
                }
                free(input);
                return 1;
            }
            else if(*inputl == 'q')   // finish the func, return 0
            {
                inputl++;
                while(*inputl == ' ')
                    inputl++;
                if(*inputl != '\n'){
                    printf("Unrecognized string! Please enter again: ");
                    continue;
                }
                free(input);
                return 0;
            }
            else if(*inputl >= '0' && *inputl <= '9')
            {
                char* end = NULL;
                long value = strtol(inputl,&end,10);
                if(inputl == end){
                    printf("Unexpected string! Please enter again: ");
                    continue;
                }
                while(*end == ' ')
                    end++;
                if(*end != '\n'){
                    printf("Please just enter one num or 'p' or 'q': ");
                    continue;
                }
                else if(value >= list){
                    printf("Out of range! Please enter again: ");
                    continue;
                }
                else
                {
                    free(input);
                    NBT* next_root = nbtlr_ToNextNBT(read_nbt,value);
                    int ret = 0;
                    ret = nbtlr_Start(next_root,next_root);
                    if(ret == 0) // break the program run
                        continue_func = 0; // use this to break
                    break; // should be done otherwise you need to input again
                }
            }
        }
    }
    return 0;
}

int nbtlr_ListItem(NBT *given_nbt)
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

int nbtlr_List(NBT *given_nbt, NBT* parent)
{
    NBT* list_nbt = given_nbt;
    if(parent)
        if(parent->type == TAG_Compound || parent->type == TAG_List)
            return nbtlr_ListItem(list_nbt);  // It's the first element in the child so pass to scan it
    switch(list_nbt->type){
    case TAG_Compound:
    case TAG_List:
        return nbtlr_ListItem(list_nbt);
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
    case TAG_Int_Array:
    case TAG_Long_Array:
        for(int i = 0 ; i < list_nbt->value_a.len; i++)
            printf("%ld\n",((int64_t*)list_nbt->value_a.value)[i]);
        return -1;
    default: return -3;
    }
}

NBT *nbtlr_ToNextNBT(NBT *root, int n)
{
    NBT* next_nbt = root;
    for(int i = 0 ; i < n ; i++)
        next_nbt = next_nbt->next;
    return next_nbt;
}
