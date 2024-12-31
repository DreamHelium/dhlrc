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
#include <dhutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dh_validator.h>
#include "translation.h"

int nbtlr_instance(NBT* root, int from_parent, int modify_mode);
int nbtlr_instance_ng(NbtPos* pos, int modify_mode);
int nbtlr_Modifier_instance(NBT* root);

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
#ifdef G_OS_WIN32
        system("cls");
#else
        system("clear");
#endif
        char* key = "";
        if(pos->level == 0 || pos->item != -1)
            key = pos->current->key;
        else
            key = nbtlr_to_next_nbt(pos->tree[pos->level - 1] , pos->child[pos->level - 1])->key;
        printf(_("The detail of NBT \"%s\" is listed below:\n\n"), key);
        int list = 0;

        // Show list (pos->item = -1 represents that it's in the tree, read items)
        list = nbtlr_list(pos->current, (pos->item == -1) );

        // modify_mode option
        GValue ret = {0};
        DhOut* out = NULL;
        if(modify_mode)
        {
            if(pos->level == 0 && list == 1 && pos->current->type == TAG_Compound)
                nbtlr_modifier_start(pos->current, 0, &ret);
            else nbtlr_modifier_start( pos->current, (pos->item == -1), &ret);
        }
        else
        {
            out = dh_out_new();
            DhArgInfo* arg = dh_arg_info_new();
            /* Some same argument */
            dh_arg_info_add_arg(arg, 'q', "quit", N_("Quit application"));
            dh_arg_info_add_arg(arg, 'm', "modify", N_("Enter modification mode"));
            dh_arg_info_add_arg(arg, 's', "save", N_("Save NBT file"));
            dh_arg_info_add_arg(arg, 'e', "upins", N_("Enter upper instance"));
            if(list > 0)
            {
                DhIntValidator* validator = dh_int_validator_new(0, list - 1);
                if(pos->level == 0){
                    dh_out_read_and_output(out, N_("\nPlease enter a number or choose option to continue [Q/m/s/e/?]: "), "dhlrc", arg, DH_VALIDATOR(validator), FALSE, &ret);
                }
                else
                {
                    dh_arg_info_add_arg(arg, 'p', "upper", N_("Enter upper NBT"));
                    dh_arg_info_change_default_arg(arg, 'p');
                    dh_out_read_and_output(out, N_("\nPlease enter a number or choose option to continue [P/q/m/s/e/?]: "), "dhlrc", arg, DH_VALIDATOR(validator), FALSE, &ret);
                }
                g_object_unref(validator);
            }
            else
            {
                dh_arg_info_add_arg(arg, 'p', "upper", N_("Enter upper NBT"));
                dh_arg_info_change_default_arg(arg, 'p');
                dh_out_read_and_output(out, N_("\nNo deeper NBT, please choose option to continue [P/q/m/s/e/?]: "), "dhlrc", arg, NULL, FALSE, &ret);
            }
            g_object_unref(out);
            g_object_unref(arg);
        }
        /* Analyse ret */
        if(G_VALUE_HOLDS_INT64(&ret))
        {
            if(!nbt_pos_add_to_tree(pos, g_value_get_int64(&ret)))
                return -1;
        }
        else if(G_VALUE_HOLDS_CHAR(&ret))
        {
            char opt = g_value_get_schar(&ret);
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
        }
        else if(g_value_get_gtype(&ret) == G_TYPE_NONE)
            return -1;
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

void nbtlr_modifier_start(NBT *root, int modify_list, GValue* value)
{
    while(1)
    {
        if(modify_list) // modify all in the list
        {
            printf(_("\nUnsupported modification mode request, will exit to reading mode."));
            fflush(stdout);
            sleep(5);
            GValue tmp = {0};
            g_value_init(&tmp, G_TYPE_CHAR);
            g_value_set_schar(&tmp, 'b');
            *value = tmp;
            return;
        }
        else // only modify the item
        {
            DhOut* out = dh_out_new();
            gint64 min = 0;
            gint64 max = 0;
            if(root->type == TAG_Byte || root->type == TAG_Short || root->type == TAG_Int || root->type == TAG_Long)
            {
                DhIntValidator* validator = g_object_new(TYPE_DH_INT_VALIDATOR, NULL);
                int byte = 0;
                switch(root->type)
                {
                case TAG_Byte:
                    min = G_MININT8;
                    max = G_MAXINT8;
                    break;
                case TAG_Short:
                    min = G_MININT16;
                    max = G_MAXINT16;
                    break;
                case TAG_Int:
                    min = G_MININT32;
                    max = G_MAXINT32;
                    break;
                case TAG_Long:
                    min = G_MININT64;
                    max = G_MAXINT64;
                    break;
                default: break;
                }
                dh_validator_set_range(DH_VALIDATOR(validator), &min, &max);
                DhArgInfo* arg = dh_arg_info_new();
                dh_arg_info_add_arg(arg, 'p', "upper", N_("Enter upper NBT"));
                dh_arg_info_add_arg(arg, 'q', "quit", N_("Quit application"));
                dh_arg_info_add_arg(arg, 'b', "back", N_("Exit modification mode"));
                /*
                printf(_("\nPlease enter an integer, or enter 'b' to exit modification mode, 'p' to upper NBT, \
'q' to exit the program (p): "));
                dh_LineOut* out = InputLine_Get_OneOpt_WithByte(byte, 0, 1, 3, 'b', 'p', 'q');
                */
                dh_out_read_and_output(out, N_("\nPlease enter an integer or choose an option [P/q/b/?]: "), "dhlrc", arg, DH_VALIDATOR(validator), FALSE, value);
                g_object_unref(arg);
                g_object_unref(validator);
                if(g_value_get_gtype(value) == G_TYPE_NONE)
                {
                    g_object_unref(out);
                    return;
                }
                else
                {
                    if(G_VALUE_HOLDS_INT64(value))
                    {
                        int64_t result = g_value_get_int64(value);

                        gchar* str = g_strdup_printf(_("\nThe integer you input is %ld, please confirm [Y/n/?]:"), result);
                        DhArgInfo* arg = dh_arg_info_new();
                        dh_arg_info_add_arg(arg, 'y', "yes", N_("Yes"));
                        dh_arg_info_add_arg(arg, 'n', "no", N_("No"));
                        GValue tmp;
                        dh_out_read_and_output(out, str, "dhlrc", arg, NULL, FALSE, &tmp);
                        g_free(str);
                        /*
                        dh_LineOut* confirm = InputLine_Get_OneOpt(0, 0, 2, 'y', 'n');
                        if((confirm->type == Character && confirm->val_c == 'y') || confirm->type == Empty )
                        {
                            dh_LineOut_Free(confirm);
                            root->value_i = result;
                        }
                        else
                            dh_LineOut_Free(confirm);
                        */
                        if(G_VALUE_HOLDS_CHAR(&tmp) && g_value_get_schar(&tmp) == 'y')
                        {
                            root->value_i = result;
                            g_object_unref(arg);
                            g_object_unref(out);
                        }
                        else
                        {
                            g_object_unref(arg);
                            g_object_unref(out);
                        }
                    }
                    else
                    {
                        g_object_unref(out);
                        return;
                    }
                }
            }
            else{
                printf(_("\nUnsupported modification mode request, will exit to reading mode."));
                fflush(stdout);
                sleep(5);
                GValue tmp = {0};
                g_value_init(&tmp, G_TYPE_CHAR);
                g_value_set_schar(&tmp, 'b');
                *value = tmp;
                return;
            }
        }
    }
}

int nbtlr_Modifier_instance(NBT* root)
{
    return 0;
}