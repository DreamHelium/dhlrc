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
#include <unistd.h>

int nbtlr_instance(NBT* root, int from_parent, int modify_mode);
int nbtlr_Modifier_instance(NBT* root);

int nbtlr_Start(NBT* root)
{
    return nbtlr_instance(root, 0, 0);
}

int nbtlr_instance(NBT *root, int from_parent, int modify_mode)
{
    // from_parent shows that it is from a parent instance
    // return : 1 : back to out instance
    //          0 : exit
    //          2 : to modify_mode ( unfinished )
    NBT* read_nbt = root;
    int continue_func = 1;
    while(continue_func){
        system("clear");
        String_Translate_printfWithArgs("dh.nbtlr.detail", root->key);

        /* Two cases:
         * One is no parent NBT, just use the root. (default case)
         * One is reading content in the NBT, still just use the root.
         * If not the cases above, enter child and scan.
         */

        // print nbt list, if into a non-list/compound nbt, goto child automatically

        int list = 0;
        if(from_parent)
        {
            if(root->type == TAG_List || root->type == TAG_Compound)
            {
                read_nbt = root->child;
                list = nbtlr_List(read_nbt, 1); // reading all content
            }
            else list = nbtlr_List(read_nbt, 0); // reading content
        }
        else list = nbtlr_List(read_nbt, 0); // default

        /** after listing, if modify_mode, should enter modify mode
         *
         *  \todo return code
         */

        dh_LineOut* input = NULL; // compatable with the process
        if( modify_mode )
        {
            if(root != read_nbt)
                input = nbtlr_Modifier_Start(read_nbt, 1); // could modify the lists in the nbt
            else if( from_parent )
                input = nbtlr_Modifier_Start(read_nbt, 0); // don't need to modify lists in the nbt
            else if( list == 1 && root->type == TAG_Compound )
                input = nbtlr_Modifier_Start(read_nbt, 0); // safety protection
            else input = nbtlr_Modifier_Start(read_nbt, 1); // considered as a normal NBT, use the default mode
        }

        /** if the list is empty the situation will be slightly different, if modify_mode is true it could
         *  refresh the value
         */

//        if(list <= 0) // also return to origin func
//        {
//            String_Translate_printfRaw("dh.nbtlr.noDeeperNBT");
//            char gchar;
//            while( (gchar = getchar()) )
//                if(gchar == '\n' || gchar == EOF)
//                    break;
//            return 1;
//        }


        if(!modify_mode) // if modify_mode, the input is handled by Modifier instance
        {
            if(list > 0)
            {
                if(!from_parent){
                    String_Translate_printfRaw("dh.nbtlr.askRequest");
                    input = InputLine_Get_OneOpt(1, 1, 2, 0, list - 1, 'q', 'm');
                }
                else
                {
                    String_Translate_printfRaw("dh.nbtlr.askRequestTwice");
                    input = InputLine_Get_OneOpt(1, 1, 3, 0, list - 1, 'q', 'p', 'm');
                }
            }
            else
            {
                String_Translate_printfRaw("dh.nbtlr.returnOrModify");
                input = InputLine_Get_OneOpt(0, 0, 3, 'p', 'm', 'q');
            }
        }
        if(input)
        {
            switch(input->type)
            {
            case Integer:
            {
                NBT* next_root = nbtlr_ToNextNBT(read_nbt, input->num_i);
                dh_LineOut_Free(input);
                int ret = nbtlr_instance(next_root, 1, modify_mode);
                if(ret == 0)
                    return 0; // end the instance
                if(ret == 2)
                    modify_mode = 1;
                if(ret == 1)
                    modify_mode = 0;
                break; // jump out of switch (if = 1/2)
            }
            case Character:
            {
                char result = input->val_c;
                dh_LineOut_Free(input);
                if(result == 'q')
                    return 0;
                if(result == 'p')
                {
                    if(modify_mode) // return 2 to enter modify_mode in parent
                        return 2;
                    else return 1;
                }
                if(result == 'm')
                    modify_mode = 1;
                if(result == 'b')
                    modify_mode = 0; // exit modify mode and refresh
                break;
            }
            default:
                dh_LineOut_Free(input);
                if(from_parent)
                {
                    if(modify_mode) return 2;
                    else return from_parent;
                }
                else return 0;
            }
        }
        else return 0;
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

int nbtlr_List(NBT *given_nbt, int read_next)
{
    NBT* list_nbt = given_nbt;
    if(read_next)
//        if(parent->type == TAG_Compound || parent->type == TAG_List)
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

NBT *nbtlr_ToNextNBT(NBT *root, int n)
{
    NBT* next_nbt = root;
    for(int i = 0 ; i < n ; i++)
        next_nbt = next_nbt->next;
    return next_nbt;
}

dh_LineOut* nbtlr_Modifier_Start(NBT *root, int modify_list)
{
    while(1)
    {
        if(modify_list) // modify all in the list
        {
            String_Translate_printfRaw("dh.nbtlr.modifier.notSupported");
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
                String_Translate_printfRaw("dh.nbtlr.modifier.inputInteger");
                dh_LineOut* out = InputLine_Get_OneOpt_WithByte(byte, 0, 1, 3, 'b', 'p', 'q');
                if(out)
                {
                    if(out->type == Integer)
                    {
                        int64_t result = out->num_i;
                        dh_LineOut_Free(out);
                        out = NULL;

                        String_Translate_printfWithArgs("dh.nbtlr.modifier.confirmInteger", result);
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
                String_Translate_printfRaw("dh.nbtlr.modifier.notSupported");
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
