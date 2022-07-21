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
#include "file_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef DH_DISABLE_TRANSLATION
#include <libintl.h>
#define _(str) gettext (str)
#else
#define _(str) str
#endif

int nbtlr_instance(NBT* root, int from_parent, int modify_mode);
int nbtlr_instance_ng(NBT_Pos* pos, int modify_mode);
int nbtlr_Modifier_instance(NBT* root);
int nbtlr_save(NBT* root);

NBT_Pos* NBT_Pos_init(NBT* root)
{
    if(root)
    {
        NBT_Pos* out = (NBT_Pos*)malloc(sizeof(NBT_Pos));
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

int NBT_Pos_AddToTree(NBT_Pos* pos, int n)
{
    NBT* current = nbtlr_ToNextNBT(pos->tree[pos->level], n); // Analyse what's next
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

int NBT_Pos_DeleteLast(NBT_Pos* pos)
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

int NBT_Pos_GetChild(NBT_Pos* pos, const char* key)
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
                return NBT_Pos_AddToTree( pos, item );
            else
            {
                /* Try to enter and scan */
                int ret = NBT_Pos_AddToTree(pos, (pos->item == -1)? 0: pos->item);
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
                        return NBT_Pos_AddToTree(pos, item);
                    else return 0;
                }
                else return 0;
            }
        }
    }
    else return 0;
}

int NBT_Pos_GetChild_Deep(NBT_Pos* pos, ...)
{
    if(pos)
    {
        va_list va;
        va_start(va, pos);
        char* temp = NULL;
        while( (temp = va_arg(va, char*)) != NULL )
        {
            if(!NBT_Pos_GetChild(pos, temp))
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

NBT_Pos * NBT_Pos_Copy(NBT_Pos* pos)
{
    NBT_Pos* new_pos = (NBT_Pos*)malloc(sizeof(NBT_Pos));
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

NBT * NBT_Pos_GetItem_NBT(NBT_Pos* pos, const char* key)
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


void NBT_Pos_Free(NBT_Pos* pos)
{
    free(pos->child);
    free(pos->tree);
    free(pos);
}

int nbtlr_Start(NBT* root)
{
    NBT_Pos* pos = NBT_Pos_init(root);
    int ret = nbtlr_instance_ng(pos, 0);
    NBT_Pos_Free(pos);
    return ret;
}

int nbtlr_Start_Pos(NBT_Pos* pos)
{
    return nbtlr_instance_ng(pos, 0);
}

int nbtlr_instance_ng(NBT_Pos* pos, int modify_mode)
{
    while(1)
    {
        system("clear");
        char* key;
        if(pos->level == 0 || pos->item != -1)
            key = pos->current->key;
        else
            key = nbtlr_ToNextNBT( pos->tree[pos->level - 1] , pos->child[pos->level - 1])->key;
        printf(_("The detail of NBT \"%s\" is listed below:\n\n"), key);
        int list = 0;

        // Show list (pos->item = -1 represents that it's in the tree, read items)
        list = nbtlr_List(pos->current, (pos->item == -1) );

        // modify_mode option
        dh_LineOut* input = NULL;
        if(modify_mode)
        {
            if(pos->level == 0 && list == 1 && pos->current->type == TAG_Compound)
                input = nbtlr_Modifier_Start(pos->current, 0);
            else input = nbtlr_Modifier_Start( pos->current, (pos->item == -1));
//            if(pos->level == 0){
//                if(pos->tree[0]->type == TAG_Compound)
//                    input = nbtlr_Modifier_Start( pos->tree[0] , 0 ); // it's the root, so don't allow modifying list
//                else input = nbtlr_Modifier_Start( pos->tree[0], 1 ); // but if it's not compound, considered as a normal NBT (Unexpected)
//            }
//            else if(read_nbt != o_nbt)
//                input = nbtlr_Modifier_Start( read_nbt, 1); // enter child and edit the compound/list
//            else input = nbtlr_Modifier_Start( read_nbt, 0);
        }
        else
        {
            if(list > 0)
            {
                if(pos->level == 0){
                    printf(_("\nPlease enter a number to continue, or enter 'm' to modification mode, \
's' to save NBT file, 'q' to exit the program (q): "));
                    input = InputLine_Get_OneOpt(1, 1, 3, 0, list - 1, 'q', 'm', 's');
                }
                else
                {
                    printf(_("\nPlease enter a number to continue, or enter 'm' to modification mode, \
'p' to upper NBT, 's' to save NBT file, 'q' to exit the program (p): "));
                    input = InputLine_Get_OneOpt(1, 1, 4, 0, list - 1, 'q', 'p', 'm', 's');
                }
            }
            else
            {
                printf(_("\nNo deeper NBT, please enter 'm' to modification mode, 'p' to upper NBT, \
's' to save NBT file, 'q' to exit the program (p): "));
                input = InputLine_Get_OneOpt(0, 0, 4, 'p', 'm', 'q', 's');
            }
        }
        if(input)
        {
            switch(input->type)
            {
            case Integer:
            {
                if(!NBT_Pos_AddToTree(pos, input->num_i))
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
                if(opt == 'm')
                    modify_mode = 1;
                if(opt == 'b')
                    modify_mode = 0;
                if(opt == 'p')
                    if(!NBT_Pos_DeleteLast(pos)) return -1;
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
                else NBT_Pos_DeleteLast(pos);
                break;
            }
            }
        }
        else return -1;
    }
}

// old nbtlr instance, if you like you could use it (save mode not supported in it)

//int nbtlr_instance(NBT *root, int from_parent, int modify_mode)
//{
//    // from_parent shows that it is from a parent instance
//    // return : 1 : back to out instance
//    //          0 : exit
//    //          2 : to modify_mode ( unfinished )
//    NBT* read_nbt = root;
//    int continue_func = 1;
//    while(continue_func){
//        system("clear");
//        String_Translate_printfWithArgs("dh.nbtlr.detail", root->key);

//        /* Two cases:
//         * One is no parent NBT, just use the root. (default case)
//         * One is reading content in the NBT, still just use the root.
//         * If not the cases above, enter child and scan.
//         */

//        // print nbt list, if into a non-list/compound nbt, goto child automatically

//        int list = 0;
//        if(from_parent)
//        {
//            if(root->type == TAG_List || root->type == TAG_Compound)
//            {
//                read_nbt = root->child;
//                list = nbtlr_List(read_nbt, 1); // reading all content
//            }
//            else list = nbtlr_List(read_nbt, 0); // reading content
//        }
//        else list = nbtlr_List(read_nbt, 0); // default

//        /** after listing, if modify_mode, should enter modify mode
//         *
//         *  \todo return code
//         */

//        dh_LineOut* input = NULL; // compatable with the process
//        if( modify_mode )
//        {
//            if(root != read_nbt)
//                input = nbtlr_Modifier_Start(read_nbt, 1); // could modify the lists in the nbt
//            else if( from_parent )
//                input = nbtlr_Modifier_Start(read_nbt, 0); // don't need to modify lists in the nbt
//            else if( list == 1 && root->type == TAG_Compound )
//                input = nbtlr_Modifier_Start(read_nbt, 0); // safety protection
//            else input = nbtlr_Modifier_Start(read_nbt, 1); // considered as a normal NBT, use the default mode
//        }

//        /** if the list is empty the situation will be slightly different, if modify_mode is true it could
//         *  refresh the value
//         */

////        if(list <= 0) // also return to origin func
////        {
////            String_Translate_printfRaw("dh.nbtlr.noDeeperNBT");
////            char gchar;
////            while( (gchar = getchar()) )
////                if(gchar == '\n' || gchar == EOF)
////                    break;
////            return 1;
////        }


//        if(!modify_mode) // if modify_mode, the input is handled by Modifier instance
//        {
//            if(list > 0)
//            {
//                if(!from_parent){
//                    String_Translate_printfRaw("dh.nbtlr.askRequest");
//                    input = InputLine_Get_OneOpt(1, 1, 2, 0, list - 1, 'q', 'm');
//                }
//                else
//                {
//                    String_Translate_printfRaw("dh.nbtlr.askRequestTwice");
//                    input = InputLine_Get_OneOpt(1, 1, 3, 0, list - 1, 'q', 'p', 'm');
//                }
//            }
//            else
//            {
//                String_Translate_printfRaw("dh.nbtlr.returnOrModify");
//                input = InputLine_Get_OneOpt(0, 0, 3, 'p', 'm', 'q');
//            }
//        }
//        if(input)
//        {
//            switch(input->type)
//            {
//            case Integer:
//            {
//                NBT* next_root = nbtlr_ToNextNBT(read_nbt, input->num_i);
//                dh_LineOut_Free(input);
//                int ret = nbtlr_instance(next_root, 1, modify_mode);
//                if(ret == 0)
//                    return 0; // end the instance
//                if(ret == 2)
//                    modify_mode = 1;
//                if(ret == 1)
//                    modify_mode = 0;
//                break; // jump out of switch (if = 1/2)
//            }
//            case Character:
//            {
//                char result = input->val_c;
//                dh_LineOut_Free(input);
//                if(result == 'q')
//                    return 0;
//                if(result == 'p')
//                {
//                    if(modify_mode) // return 2 to enter modify_mode in parent
//                        return 2;
//                    else return 1;
//                }
//                if(result == 'm')
//                    modify_mode = 1;
//                if(result == 'b')
//                    modify_mode = 0; // exit modify mode and refresh
//                break;
//            }
//            default:
//                dh_LineOut_Free(input);
//                if(from_parent)
//                {
//                    if(modify_mode) return 2;
//                    else return from_parent;
//                }
//                else return 0;
//            }
//        }
//        else return 0;
//    }
//    return 0;
//}


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
