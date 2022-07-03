/*  dh_string_util - String utils by Dream Helium
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

#include "dh_string_util.h"
#include "file_util.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <cjson/cJSON.h>
#include <locale.h>
#include <errno.h>
#include <float.h>

#define ARR_COPY_TRANS(bit,o_bit,new_array,array,len) for(int i = 0 ; i < len ; i++) \
((int##bit##_t*)(new_array))[i] = ((int##o_bit##_t*)(array))[i];

dh_LineOut* InputLine_Get_OneOpt_va(int byte, int range_check, int need_num, int arg_num, int min, int max, va_list va);
dh_LineOut *InputLine_Get_MoreDigits_va(int byte, int range_check, int need_nums, int arg_num, va_list va);
int char_check(const char* str, char** end, char check_char);
int multi_char_check(const char* str, int args, va_list char_list, char* result);
int search_num(const char* str, char** end, int range_check, int min, int max, int64_t *result);
int64_t *num_array_check(const char* str, int range_check, int need_nums, va_list range);
char* get_locale();
int range_check(int64_t num, int byte);
int float_check(double num);
void* resize_array(int byte, int o_byte, void* array, int len);
char* translation_pos();

char* main_lang = NULL;
char* secondary_lang = NULL;


dh_LineOut *InputLine_Get_OneOpt_va(int byte, int range_check, int need_num, int arg_num, int min, int max, va_list va)
{
    char* input = NULL;
    size_t size = 0;
    int gret = -2;
    va_list va_o;
    va_copy(va_o, va);
    while((gret = getline(&input, &size, stdin)) != -1)
    {
        char char_result;
        char* inputl = input;
        while(isspace(*inputl))
            inputl++;
        if(need_num) // Try to read the num first
        {
            char* end = NULL;
            int64_t result = 0;
            if(search_num(inputl, &end, range_check, min, max, &result))
            {
                if(*end == 0)
                {
                    free(input);
                    dh_LineOut* out = dh_LineOut_CreateNum(result, byte);
                    if(out)
                    {
                        va_end(va_o);
                        return out;
                    }
                    else String_Translate_printfRaw("dh.string.outOfRange");
                }
                else String_Translate_printfRaw("dh.string.unrecognize");
            }
            else //String_Translate_printfRaw("dh.string.outOfRangeOrUnrecognize");
            {
                va_list va_char;
                va_copy(va_char, va_o);
                if( multi_char_check(inputl, arg_num, va_char, &char_result) )
                {
                    free(input);
                    va_end(va_char);
                    va_end(va_o);
                    return dh_LineOut_CreateChar(result);
                }
                else
                {
                    va_end(va_char);
                    if(*inputl == 0)
                    {
                        va_end(va_o);
                        free(input);
                        return dh_LineOut_CreateEmpty();
                    }
                    else String_Translate_printfRaw("dh.string.unrecognize");
                }
            }
        }
    }
    if(gret == -1)
    {
        va_end(va_o);
        printf("Terminated output!\n");
        free(input);
        return NULL;
    }
    return NULL;
}


dh_LineOut *InputLine_Get_MoreDigits_va(int byte, int range_check, int need_nums, int arg_num, va_list va)
{
    char* input = NULL;
    size_t size = 0;
    int gret = -2;
    int n_num = need_nums;
    va_list va_o;
    va_copy(va_o,va);
    while((gret = getline(&input, &size, stdin)) != -1)
    {
        va_list va_num;
        va_copy(va_num, va_o); // Init for num process
        char* inputl = input;
        // ignore blanket.
        while(isspace(*inputl))
              inputl++;
        if(need_nums <= 0)
            n_num = 0;
        int64_t* num_array = NULL;
        // read and check numbers.
        num_array = num_array_check(inputl, range_check, need_nums, va_num);
        va_end(va_num); // num process finish
        if( num_array )
        {
            free(input);
            dh_LineOut* output = dh_LineOut_CreateNumArray(num_array, need_nums, byte, 64/8);
            free(num_array);
            if(output)
            {
                va_end(va_o);
                return output;
            }
            else
                String_Translate_printfRaw("dh.string.outOfRangeOrUnrecognize");
        }
        else{
            // read character
            va_list va_char;
            va_copy(va_char, va_o);
            if(range_check)
            {
                for(int i = 0 ; i < (n_num * 2); i++)
                {
                    va_arg(va_char,int);
                }
            }
            char output_char = 0;
            if(multi_char_check(inputl, arg_num, va_char, &output_char))
            {
                va_end(va_char);
                va_end(va_o);
                free(input);
                return dh_LineOut_CreateChar(output_char);
            }
            else if(*inputl == 0){
                va_end(va_char);
                va_end(va_o);
                free(input);
                return dh_LineOut_CreateEmpty();
            }
            else {
                va_end(va_char);
                String_Translate_printfRaw("dh.string.unrecognize");
            }
        }
    }
    if(gret == -1)
    {
        printf("Terminated output!\n");
        free(input);
        va_end(va_o);
        return NULL;
    }
    return NULL;
}

long *NumArray_From_String(const char *string, int *nums, int char_check)
{
    long* output = NULL;
    int continue_func = 1;
    int num = 0;
    const char* str = string;
    while(continue_func)
    {
        if(char_check)
        {
            while(*str == ' ')
                str++;
            if(*str != '-' || !isdigit(*str) || *str != '\n' || *str != 0)
            {
                *nums = 0;
                free(output);
                return NULL;
            }
        }
        char* end = NULL;
        long value = strtol(str, &end, 10);
        if(str == end)
        {
            *nums = num;
            return output;
        }
        num++;
        long* otemp = realloc(output, num * sizeof(long));
        if(!otemp)
        {
            free(output);
            *nums = 0;
            return NULL;
        }
        output = otemp;
        output[num - 1] = value;
        str = end;
    }
    *nums = num;
    return output;
}

int char_check(const char* str, char** end, char check_char)
{
    const char* str_in = str;
    while( isspace(*str_in) )
        str_in++;
    if(*str_in == check_char)
    {
        if(end)
        {
            const char* end_str = ++str_in;
            while(isspace(*end_str))
                end_str++;
            *end = (char*)end_str ;
        }
        return 1;
    }
    else return 0;
}

int multi_char_check(const char* str, int args, va_list char_list, char* result)
{
    for(int i = 0 ; i < args; i++)
    {
        char check_char = (char)va_arg(char_list, int);
        char* end = NULL;
        if(char_check(str, &end, check_char))
        {
            if(*end != 0)
                return 0;
            else
            {
                *result = check_char;
                return 1;
            }
        }
    }
    return 0;
}

int search_num(const char* str, char** end, int range_check, int min, int max, int64_t* result)
{
    const char* str_in = str;
    while( isspace(*str_in) )
        str_in++;
    if( *str_in == '+' || *str_in == '-' || isdigit(*str_in) ) // It might be a num
    {
        errno = 0;
        if( *str_in == '+' || *str_in == '-' )
        {
            if(!isdigit(str_in[1]))
                return 0; // Not a digit
        }
        long long r = strtoll(str_in, end, 10);
        if(errno == ERANGE)
            return 0;  // Out of range
        if(range_check)
        {
            if( r < min || r > max )
                return 0;    // Out of range
        }
        char* end_str = *end;
        while( isspace(*end_str) )
            end_str++;
        *end = end_str;
        *result = r;
        return 1;
    }
    else return 0;
}

int64_t* num_array_check(const char* str, int range_check, int need_nums, va_list range)
{
    const char* str_in = str;
    char* end = (char*)str_in;
    int64_t* output = NULL;
    int min = -1;
    int max = -1;
    for(int i = 0 ; i < need_nums ; i++)
    {
        if(range_check){
            min = va_arg(range, int);
            max = va_arg(range, int);
        }
        int64_t result;
        if( search_num(end, &end , range_check, min, max, &result) )
        {
            int64_t* output_p = (int64_t*)realloc( output, (i + 1) * sizeof(int64_t) );
            if(!output_p)
            {
                free(output);
                return NULL;
            }
            else
            {
                output = output_p;
                output[i] = result;
            }
        }
        else
        {
            free(output);
            return NULL;
        }
    }
    if(*end != 0)
    {
        free(output);
        return NULL;
    }
    else
        return output;
}

char *String_Copy(const char *o_str)
{
    char* str = malloc( (strlen(o_str) + sizeof("") ) * sizeof(char));
    if(str)
    {
        strcpy(str, o_str);
        return str;
    }
    else return NULL;
}

char* String_Translate(const char* str)
{
    char* filepos = translation_pos();
    cJSON* trans_json = dhlrc_FileToJSON(filepos);
    free(filepos);
    if(trans_json)
    {
        cJSON* trans_item =cJSON_GetObjectItem(trans_json, str);
        if(cJSON_IsString(trans_item))
        {
            char* trans = cJSON_GetStringValue(trans_item);
            char* output = String_Copy(trans);
            cJSON_Delete(trans_json);
            return output;
        }
        else{
            cJSON_Delete(trans_json);
            return String_Copy(str);
        }

    }
    else return String_Copy(str);
}

void dh_LineOut_Free(dh_LineOut *lo)
{
    switch(lo->type){
    case String:
    case NumArray:
    case Empty:
        free(lo->val);
        break;
    default: break;
    }
    free(lo);
}

dh_StrArray *dh_StrArray_Init(const char *str)
{
    dh_StrArray* arr = malloc(sizeof(dh_StrArray));
    if(arr)
    {
        arr->num = 1;
        arr->val = malloc( sizeof(char*) );
        if(arr->val)
        {
            arr->val[0] = String_Copy(str);
            return arr;
        }
        else
        {
            free(arr);
            return NULL;
        }
    }
    else return NULL;
}

int dh_StrArray_AddStr(dh_StrArray **arr, const char *str)
{
    if(*arr == NULL)
    {
        dh_StrArray* arr_init = dh_StrArray_Init(str);
        if(arr_init)
        {
            *arr = arr_init;
            return 0;
        }
        else return -1;
    }
    else
    {
        dh_StrArray* o_arr = *arr;
        char** p_arr = realloc(o_arr->val, (o_arr->num + 1) * sizeof(char*));
        if(p_arr)
        {
            o_arr->val = p_arr;
            o_arr->val[o_arr->num] = String_Copy(str);
            o_arr->num = o_arr->num + 1;
            return 0;
        }
        else
        {
            dh_StrArray_Free(o_arr);
            *arr = NULL;
            return -1;
        }
    }
}

void dh_StrArray_Free(dh_StrArray *arr)
{
    if(arr)
    {
        for(int i = 0; i < arr->num; i++)
            free(arr->val[i]);
        free(arr->val);
        free(arr);
    }
}

char* get_locale()
{
    if(!main_lang)
    {
        char* override_lang = dhlrc_ConfigContent("OverrideLang");
        if(override_lang == NULL || !strcmp(override_lang,""))
        {
            free(override_lang);
            char* lang = setlocale(LC_MESSAGES, "");
            char* lang_copy = lang;
            int point_pos = 0;
            while(*lang_copy != '.' && *lang_copy != '\0' && *lang_copy != '@')
            {
                // For example "zh_CN.UTF-8", when it's "." , pos will be 5, so it's lang[5] = '.'.
                lang_copy++;
                point_pos++;
            }
            char* ret = (char*)malloc((point_pos + sizeof("")) * sizeof(char));
            for(int i = 0 ; i < point_pos; i++)
                ret[i] = lang[i];
            ret[point_pos] = '\0';
            main_lang = String_Copy(ret);
            free(ret);
        }
        else
        {
            main_lang = String_Copy(override_lang);
            free(override_lang);
        }
        // Determine translation file exists or fallback to en_US
        char* filepos = translation_pos();
        cJSON* trans_json = dhlrc_FileToJSON(filepos);
        free(filepos);
        if(trans_json)
        {
            cJSON_Delete(trans_json);
            return main_lang;
        }
        else
        {
            free(main_lang);
            main_lang = String_Copy("en_US");
            return main_lang;
        }
    }
    else return main_lang;
}

int range_check(int64_t num, int byte)
{
    // 8, 16, 32, 64 bit
    if(byte == 1)
    {
        if(num > INT8_MAX || num < INT8_MIN)
            return 0;
        else return 1;
    }
    else if(byte == 2)
    {
        if(num > INT16_MAX || num < INT16_MIN)
            return 0;
        else return 1;
    }
    else if(byte == 4)
    {
        if(num > INT32_MAX || num < INT32_MIN)
            return 0;
        else return 1;
    }
    else if(byte == 8) return 1;
    else return 0;
}

dh_LineOut *dh_LineOut_CreateNum(int64_t num, int byte)
{
    dh_LineOut* out = (dh_LineOut*)malloc(sizeof(dh_LineOut));
    if(out)
    {
        if(range_check(num, byte))
        {
            out->byte = byte;
            out->num_i = num;
            out->type = Integer;
            return out;
        }
        else
        {
            free(out);
            return NULL;
        }
    }
    else return NULL;
}

int float_check(double num)
{
    if(num < (-FLT_MAX) || num > (FLT_MAX))
        return 0;
    else return 1;
}

dh_LineOut *dh_LineOut_CreateFloat(double num)
{
    dh_LineOut* out = (dh_LineOut*)malloc(sizeof(dh_LineOut));
    if(out)
    {
        if(float_check(num))
        {
            out->byte = 4; // 32/8 = 4, float
            out->num_f = num;
            out->type = Float;
            return out;
        }
        else
        {
            free(out);
            return NULL;
        }
    }
    else return NULL;
}

dh_LineOut* dh_LineOut_CreateDouble(double num)
{
    dh_LineOut* out = (dh_LineOut*)malloc(sizeof(dh_LineOut));
    if(out)
    {
        out->byte = 8;
        out->num_f = num;
        out->type = Double;
        return out;
    }
    else return NULL;
}

dh_LineOut *dh_LineOut_CreateChar(char c)
{
    dh_LineOut* out = (dh_LineOut*)malloc(sizeof(dh_LineOut));
    if(out)
    {
        out->byte = 1;
        out->val_c = c;
        out->type = Character;
        return out;
    }
    else return NULL;
}

dh_LineOut *dh_LineOut_CreateNumArray(void *array, int len, int byte, int o_byte)
{
    if((byte == 1 || byte == 2 || byte == 4 || byte == 8)
            && (o_byte == 1 || o_byte == 2 || o_byte == 4 || o_byte == 8))
    {
        dh_LineOut* out = (dh_LineOut*)malloc(sizeof(dh_LineOut));
        if(out)
        {
            void* new_array = resize_array(byte, o_byte, array, len);
            if(new_array)
            {
                out->val = new_array;
                out->len = len;
                out->type = NumArray;
                out->byte = byte;
                return out;
            }
            else
            {
                free(out);
                return NULL;
            }
        }
        else return NULL;
    }
    else return NULL;
}

void* resize_array(int byte, int o_byte, void* array, int len)
{
    if((byte == 1 || byte == 2 || byte == 4 || byte == 8) &&
            (o_byte == 1 || o_byte == 2 || o_byte == 4 || o_byte == 8))
    {
        void* new_array = malloc(len * byte);
        if(new_array)
        {
            if(byte == o_byte) // copy array
            {
                memcpy(new_array, array, len * byte);
            }
            else if(o_byte == 1)
            {
                // o_byte = 1 -> int8_t array
                if(byte == 2){
                    ARR_COPY_TRANS(16, 8,new_array,array,len);
                    // ARR_COPY_TRANS(bit, o_bit, new_array, array, len)
                }
                else if(byte == 4){
                    ARR_COPY_TRANS(32, 8,new_array,array,len);
                }
                else if(byte == 8){
                    ARR_COPY_TRANS(64, 8,new_array,array,len);
                }
            }
            else if(o_byte == 2)
            {
                // o_byte = 2 -> int16_t array
                if(byte == 1)
                {
                    for(int i = 0 ; i < len; i++)
                    {
                        if( !range_check( ((int16_t*)(array))[i] , byte) )
                        {
                            free(new_array);
                            return NULL;
                        }
                    }
                    ARR_COPY_TRANS(8, 16,new_array,array,len);
                }
                else if(byte == 4){
                    ARR_COPY_TRANS(32, 16, new_array,array,len);
                }
                else if(byte == 8){
                    ARR_COPY_TRANS(64, 16,new_array,array,len);
                }
            }
            else if(o_byte == 4)
            {
                // o_byte = 4 -> int32_t array
                if(byte == 1 || byte == 2)
                {
                    for(int i = 0 ; i < len ; i++)
                    {
                        if( !range_check( ((int32_t*)(array))[i] , byte ))
                        {
                            free(new_array);
                            return NULL;
                        }
                    }
                    if(byte == 1){
                        ARR_COPY_TRANS(8, 32 ,new_array,array,len);
                    }
                    else ARR_COPY_TRANS(16, 32, new_array, array,len);
                }
                else if(byte == 8)
                    ARR_COPY_TRANS(64, 32,new_array,array,len);
            }
            else if(o_byte == 8)
            {
                // o_byte = 8 -> int64_t array
                for(int i = 0 ; i < len ; i++)
                {
                    if( !range_check( ((int64_t*)(array))[i] , byte ) )
                    {
                        free(new_array);
                        return NULL;
                    }
                }
                if(byte == 1) {
                    ARR_COPY_TRANS(8, 64 ,new_array,array,len);
                }
                else if(byte == 2) {
                    ARR_COPY_TRANS(16, 64,new_array,array,len);
                }
                else if(byte == 4) {
                    ARR_COPY_TRANS(32, 64,new_array,array,len);
                }
            }
            return new_array;
        }
        else return NULL;
    }
    else return NULL;
}

dh_LineOut *dh_LineOut_CreateString(const char *str)
{
    dh_LineOut* out = (dh_LineOut*)malloc(sizeof(dh_LineOut));
    if(out)
    {
        char* str_copy = String_Copy(str);
        if(str_copy)
        {
            out->val = str_copy;
            out->len = strlen(str_copy) + 1;
            out->type = String;
            out->byte = sizeof(char);
            return out;
        }
        else
        {
            free(out);
            return NULL;
        }
    }
    else return NULL;
}

dh_LineOut *dh_LineOut_CreateEmpty()
{
    dh_LineOut* out = (dh_LineOut*)malloc(sizeof(dh_LineOut));
    if(out)
    {
        out->val = NULL;
        out->len = 0;
        out->type = Empty;
        out->byte = 0;
        return out;
    }
    else return NULL;
}

void String_Translate_printfRaw(const char *str)
{
    char* trans = String_Translate(str);
    printf("%s",trans);
    free(trans);
}

void String_Translate_printfWithArgs(const char *str, ...)
{
    char* trans = String_Translate(str);
    va_list va;
    va_start(va,str);
    vprintf(trans, va);
    va_end(va);
    free(trans);
}

dh_LineOut *InputLine_Get_OneOpt(int range_check, int need_num, int arg_num, int min, int max, ...)
{
    va_list va;
    va_start(va,max);
    dh_LineOut* out = InputLine_Get_OneOpt_va(64/8, range_check, need_num, arg_num, min, max, va);
    va_end(va);
    return out;
}



dh_LineOut *InputLine_Get_OneOpt_WithByte(int byte, int range_check, int need_num, int arg_num, int min, int max, ...)
{
    va_list va;
    va_start(va,max);
    dh_LineOut* out = InputLine_Get_OneOpt_va(byte, range_check, need_num, arg_num, min, max, va);
    va_end(va);
    return out;
}

dh_LineOut *InputLine_Get_MoreDigits(int range_check, int need_nums, int arg_num, ...)
{
    va_list va;
    va_start(va, arg_num);
    dh_LineOut* out = InputLine_Get_MoreDigits_va(64/8, range_check, need_nums, arg_num, va);
    va_end(va);
    return out;
}

dh_LineOut *InputLine_Get_MoreDigits_WithByte(int byte, int range_check, int need_nums, int arg_num, ...)
{
    va_list va;
    va_start(va, arg_num);
    dh_LineOut* out = InputLine_Get_MoreDigits_va(byte, range_check, need_nums, arg_num ,va);
    va_end(va);
    return out;
}

void String_Translate_FreeLocale()
{
    free(main_lang);
    free(secondary_lang);
}

char* translation_pos()
{
    char* lang = get_locale();
    char* dir = dhlrc_ConfigContent("langDir");
    if(!dir || !strcmp(dir,""))
    {
        free(dir);
        dir = String_Copy("lang/");
    }
    int len = strlen(dir) + strlen("/") + strlen(lang) + strlen(".json") + sizeof ("");
    char* filepos = (char*)malloc( len * sizeof(char) );
    // dir + / + lang + .json ( "lang/" "/" "en_US" ".json" )
    filepos[0] = 0;
    strcat( filepos, dir);
    strcat( filepos, "/");
    strcat( filepos, lang );
    strcat( filepos, ".json");
    free(dir);
    return filepos;
}
