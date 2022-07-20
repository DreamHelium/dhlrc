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
#include <string.h>
#include <ctype.h>
#include <cjson/cJSON.h>
#include <locale.h>
#include <errno.h>
#include <float.h>

#ifndef DH_DISABLE_TRANSLATION
#include <libintl.h>
#define _(str) dgettext ("dhutil", str)
#else
#define _(str) str
#endif

#define ARR_COPY_TRANS(bit,o_bit,new_array,array,len) for(int i = 0 ; i < len ; i++) \
((int##bit##_t*)(new_array))[i] = ((int##o_bit##_t*)(array))[i];

typedef struct internal_impl{
    int (*printf_fn)(const char*, ...);
    int (*vprintf_fn)(const char*, va_list);
    int (*getline_fn)(char**, size_t*);
    void (*getline_free)(void*);
    int (*err_print_fn)(const char* str);
} internal_impl;

static int default_getline(char** input, size_t* n)
{
    return dh_string_getline(input, n, stdin);
}

static int internal_err_print(const char* str)
{
    return fprintf(stderr, "%s", str);
}

static internal_impl global_impl = { printf, vprintf, default_getline, free, internal_err_print };


static dh_LineOut* InputLine_Get_OneOpt_va(int byte, int range_check, int need_num, int arg_num, va_list va);
static dh_LineOut* InputLine_Get_OneOpt_va_CharArg(int byte, int range_check, int need_num, char* arg, va_list va);
static dh_LineOut* InputLine_Get_MoreDigits_va ( int byte, int range_check, int need_nums, int arg_num, va_list va, int same_range );
static int char_check(const char* str, char** end, char check_char);
static int multi_char_check_CharArg(const char* str, char* args, char* result, int* err);
static int search_num(int byte, const char* str, char** end, int rc, int64_t min, int64_t max, int64_t *result, int* err);
static int64_t *num_array_check(int byte, const char* str, int range_check, int need_nums, va_list range, int same_range, int use_arr, int64_t **arr, int *err);
static int64_t* line_numarray_check(int byte, int* array_len, const char* str, int need_nums, int range_nums,
                                    int64_t** range, int* err, int repeat_check, int same_range,
                                    int unlimited_lens);
static int range_check(int64_t num, int byte);
static int float_check(double num);
static void* resize_array(int byte, int o_byte, void* array, int len);

// err in the case:
// -1 : Not expected character
// -2 : Out of range (min and max)
// -3 : Out of range (int64_t or byte-spec)
// -4 : Not expected character afterwards
// -5 : Memory not enough?
static dh_LineOut* inputline_handler_nummode(const char* str, int byte, int range_check, int* err, va_list va);
/** Number mode (normal) */
static dh_LineOut* inputline_handler_nummode_noVa(const char* str, int byte, int range_check, int* err, int min, int max);
static dh_LineOut* inputline_handler_charmode(const char* str, int skip_va, int arg_num , va_list va, int *err);
/** Character mode (normal) */
static dh_LineOut* inputline_handler_charmode_CharArg(const char* str, char* args, int* err);
/** Integer Array mode (normal) */
static dh_LineOut* inputline_handler_numarray(const char* str, int byte, int need_nums,
                                              int range_nums, int64_t** range, int* err, int repeat_check, int same_range,
                                              int unlimited_lens);
/** Wrapper for integer array mode */
static dh_LineOut* inputline_handler_numarray_limit(const char* str, int byte, dh_limit* limit, int* err);

static void inputline_handler_printerr(int err);

#define DH_NOT_EXPECTED (-1)
#define DH_ERROR_RANGE  (-2)
#define DH_ERROR_BYTE_RANGE (-3)
#define DH_NOT_EXPECTED_AFTER   (-4)
#define DH_NO_MEMORY    (-5)
#define DH_REPEATED (-6)
#define DH_GIVEN_RANGE_ERROR    (-7)
#define DH_LIMIT_ERROR  (-8)

#ifdef DH_USE_TRANSLATION_DEPRECATED
static char* main_lang = NULL;
static char* secondary_lang = NULL;
static char* get_locale();
static char* translation_pos();
#endif

static int translation_inited = 0;

// The thought of implement was from cJSON's cJSON_InitHooks()
void dh_string_ChangeImpl(dh_string_impl* impl)
{
    if(impl == NULL)
    {
        // reset
        global_impl.printf_fn = printf;
        global_impl.vprintf_fn = vprintf;
        global_impl.getline_fn = default_getline;
        global_impl.getline_free = free;
        return;
    }

    if(impl->printf_fn)
        global_impl.printf_fn = impl->printf_fn;
    else global_impl.printf_fn = printf;

    if(impl->vprintf_fn)
        global_impl.vprintf_fn = impl->vprintf_fn;
    else global_impl.vprintf_fn = vprintf;

    if(impl->getline_fn)
        global_impl.getline_fn = impl->getline_fn;
    else global_impl.getline_fn = default_getline;

    if(impl->getline_free)
        global_impl.getline_free = impl->getline_free;
    else global_impl.getline_free = free;

    if(impl->err_print_fn)
        global_impl.err_print_fn = impl->err_print_fn;
    else global_impl.err_print_fn = internal_err_print;
}

/* The only position to output error */
static void inputline_handler_printerr(int err)
{
#ifndef DH_DISABLE_TRANSLATION
    if(!translation_inited) /*  */
    {
        bindtextdomain("dhutil", "locale");
        translation_inited = 1;
    }
#endif
    switch (err) {
    case -1:
        global_impl.printf_fn(_("Unrecognized string, please enter again: "));
        break;
    case -2:
        global_impl.printf_fn(_("Number out of range, please enter again: "));
        break;
    case -3:
        global_impl.printf_fn(_("Number out of range of the integer, please enter again: "));
        break;
    case -4:
        global_impl.printf_fn(_("Unrecognized charater afterwards (or in the middle), or couldn't meet the need of the input. \
please enter again: "));
        break;
    case -5:
        global_impl.printf_fn(_("Out of memory? Maybe you should take some measures to release the memory and \
try to enter again: "));
        break;
    case DH_REPEATED:
        global_impl.printf_fn(_("Repeated number! please enter again: "));
        break;
    case DH_LIMIT_ERROR:
        global_impl.printf_fn(_("Error in format of limit, consult the software developer!\n"));
        break;
    default: break;
    }
}

static dh_LineOut* inputline_handler_nummode(const char* str, int byte, int range_check, int* err, va_list va)
{
    int64_t min = -1;
    int64_t max = -1;
    va_list va_num;
    va_copy(va_num, va);
    if(range_check) // update min and max
    {
        min = va_arg(va_num, int64_t);
        max = va_arg(va_num, int64_t);
    }
    va_end(va_num);
    return inputline_handler_nummode_noVa(str, byte, range_check, err, min, max);
}

static dh_LineOut* inputline_handler_nummode_noVa(const char* str, int byte, int range_check, int* err, int min, int max)
{
    char* end = NULL;
    int64_t result;
    if(search_num(byte, str, &end, range_check, min, max, &result, err))
    {
        while( isspace(*end) )
            end++;
        if(*end == 0)
            return dh_LineOut_CreateNum(result , byte);
        else{
            if(err) *err = DH_NOT_EXPECTED_AFTER;
            return NULL;
        }
    }
    else return NULL;
}


static dh_LineOut* inputline_handler_charmode(const char* str, int skip_va, int arg_num, va_list va, int* err)
{
    va_list va_char;
    va_copy(va_char, va);
    for(int i = 0 ; i < skip_va ; i++)
        va_arg(va_char, int64_t);
    char str_arg[arg_num + 1];
    for(int i = 0 ; i < arg_num ; i++)
        str_arg[i] = (char)va_arg( va_char , int);
    str_arg[arg_num] = 0;
    va_end(va_char);
    return inputline_handler_charmode_CharArg(str, str_arg, err);
}

static
dh_LineOut* inputline_handler_charmode_CharArg(const char* str, char* args, int* err)
{
    char out;
    if(multi_char_check_CharArg(str, args, &out, err))
        return dh_LineOut_CreateChar(out);
    else return NULL;
}

static dh_LineOut* inputline_handler_numarray(const char* str, int byte, int need_nums,
                                              int range_nums, int64_t ** range, int* err, int repeat_check, int same_range,
                                              int unlimited_lens)
{
    int array_len;
    int64_t* out = line_numarray_check(byte, &array_len, str, need_nums, range_nums, range, err, repeat_check, same_range, unlimited_lens);
    if(out){
        dh_LineOut* dout =  dh_LineOut_CreateNumArray(out, array_len, byte, sizeof(int64_t));
        free(out);
        return dout;
    }
    else {
        return NULL;
    }
}

static dh_LineOut* inputline_handler_numarray_limit(const char* str, int byte , dh_limit* limit, int* err)
{
    if(limit->type != NumArray)
    {
        if(err) *err = DH_LIMIT_ERROR;
        return NULL;
    }
    else
    {
        return inputline_handler_numarray( str, byte, limit->lens, limit->limit_num, (int64_t**)(limit->limit),
                                           err, limit->check_repeated, limit->same_range, limit->unlimited_lens);
    }
}

dh_LineOut * InputLine_General(int byte, dh_limit* limit, int get_string, char* args, int allow_empty)
{
    char* input = NULL;
    size_t size = 0;
    int gret = -2;
    while( (gret = global_impl.getline_fn(&input, &size)) != -1 )
    {
        int err = 0;
        /* Number part */
        if(limit)
        {
            dh_out_type type = limit->type;
            if(type == Integer || type == NumArray || type == Float || type == Double || type == FloatArray || type == DoubleArray )
            {
                /* TODO: finish all */
                if(type == Integer)
                {
                    int64_t min = -1;
                    int64_t max = -1;
                    int range_check = 0;
                    if(limit->limit)
                    {
                        range_check = 1;
                        min = ((int64_t**)(limit->limit))[0][0];
                        max = ((int64_t**)(limit->limit))[0][1];
                    }
                    dh_LineOut* out = inputline_handler_nummode_noVa( input, byte, range_check, &err, min, max);
                    if(out)
                    {
                        global_impl.getline_free(input);
                        return out;
                    }
                    else if(err == 0)
                        err = DH_NO_MEMORY;
                }
                else if(type == NumArray)
                {
                    dh_LineOut* out = inputline_handler_numarray_limit(input, byte, limit, &err);
                    if(out)
                    {
                        global_impl.getline_free(input);
                        return out;
                    }
                    else if(err == 0)
                        err = DH_NO_MEMORY;
                }
                else
                    return NULL;
            }
            else
            {
                inputline_handler_printerr(DH_LIMIT_ERROR);
                return NULL;
            }
        }
        /* String part */
        /* Although it's not recommended that using number with string, it's still provided. */
        if(get_string && ( err == DH_NOT_EXPECTED || err == 0 ))
        {
            int str_len = strlen(input);
            /* Determine whether the last character is '\n' */
            if(input[str_len - 1] == '\n')
                input[str_len - 1] = 0;
            dh_LineOut* out = dh_LineOut_CreateString( input );
            if( out ){
                global_impl.getline_free(input);
                return out;
            }
            else err = DH_NO_MEMORY;
        }
        else if(args && ( err == DH_NOT_EXPECTED || err == 0 ))
        {
            err = 0;
            /* Character part */
            dh_LineOut* out = inputline_handler_charmode_CharArg( input, args, &err );
            if( out ){
                global_impl.getline_free( input );
                return out;
            }
            else if(err == 0)
                err = DH_NO_MEMORY;
        }
        /* Empty part */
        if(allow_empty && ( err == DH_NOT_EXPECTED || err == 0 ) )
        {
            char* input_test = input;
            while( isspace(*input_test) )
                input_test++;
            dh_LineOut* out = NULL;
            if( *input_test == 0 )
            {
                out = dh_LineOut_CreateEmpty();
                if(out)
                {
                    global_impl.getline_free(input);
                    return out;
                }
                else err = DH_NO_MEMORY;
            }
        }
        inputline_handler_printerr(err);
    }
    global_impl.printf_fn(_("Terminated input!\n"));
    global_impl.getline_free(input);
    return NULL;
}



static dh_LineOut* InputLine_Get_OneOpt_va_CharArg(int byte, int range_check, int need_num, char* arg, va_list va)
{
    char* input = NULL;
    size_t size = 0;
    int gret = -2;
    while((gret = global_impl.getline_fn(&input, &size)) != -1)
    {
        int err = 0;
        if(need_num) // num process
        {
            dh_LineOut* num = inputline_handler_nummode(input, byte, range_check, &err, va);
            if(num)
            {
                global_impl.getline_free(input);
                return num;
            }
            else if(err == 0)
                err = -5;
        }
        if(err == -1 || !need_num) // char process
        {
            err = 0;
            dh_LineOut* out = inputline_handler_charmode_CharArg(input, arg, &err);
            if(out)
            {
                global_impl.getline_free(input);
                return out;
            }
            else if(err == 0)
                err = -5;
        }
        if(err == -1)
        {
            char* inputl = input;
            while( isspace(*inputl) )
                inputl++;
            if(*inputl == 0)
            {
                dh_LineOut* out = dh_LineOut_CreateEmpty();
                if(out)
                {
                    global_impl.getline_free(input);
                    return out;
                }
                else err = -5;
            }
        }
        inputline_handler_printerr(err);
    }
    if(gret == -1)
    {
        printf(_("Terminated output!\n"));
        global_impl.getline_free(input);
        return NULL;
    }
    return NULL;
}

static dh_LineOut *InputLine_Get_OneOpt_va(int byte, int range_check, int need_num, int arg_num, va_list va)
{
    va_list va_char;
    va_copy(va_char,va);
    dh_limit* limit = dh_limit_Init(Integer);
    if(limit)
    {
        if(need_num && range_check)
        {
            int64_t min = va_arg(va_char,int64_t);
            int64_t max = va_arg(va_char,int64_t);
            if(!dh_limit_AddInt( limit, min, max ))
            {
                dh_limit_Free(limit);
                va_end(va_char);
                return NULL;
            }
        }
    }
    else {
        va_end(va_char);
        return NULL;
    }
    char str_arg[arg_num + 1];
    for(int i = 0 ; i < arg_num ; i++)
        str_arg[i] = (char)va_arg(va_char, int);
    str_arg[arg_num] = 0;
    va_end(va_char);
    dh_LineOut* dout = InputLine_General( byte, limit, 0, str_arg, 1);
    dh_limit_Free(limit);
    return dout;
}


static dh_LineOut *InputLine_Get_MoreDigits_va(int byte, int range_check, int need_nums, int arg_num, va_list va,
                                        int same_range)
{
    va_list va_cp;
    va_copy(va_cp, va);
    dh_limit* limit = dh_limit_Init(NumArray);
    if(limit)
    {
        dh_limit_SetArrayArgs(limit, need_nums, same_range, 0, -1);
        if(range_check)
        {
            if(same_range){
                int64_t min = va_arg(va_cp, int64_t);
                int64_t max = va_arg(va_cp, int64_t);
                if(!dh_limit_AddInt(limit, min, max))
                {
                    va_end(va_cp);
                    dh_limit_Free(limit);
                    return NULL;
                }
            }
            else{
                for(int i = 0 ; i < need_nums ; i++ )
                {
                    int64_t min = va_arg(va_cp, int64_t);
                    int64_t max = va_arg(va_cp, int64_t);
                    if(!dh_limit_AddInt(limit, min, max))
                    {
                        va_end(va_cp);
                        dh_limit_Free(limit);
                        return NULL;
                    }
                }
            }
        }
    }
    else{
        va_end( va_cp );
        return NULL;
    }
    char str_arg[arg_num + 1];
    for(int i = 0 ; i < arg_num ; i++)
        str_arg[i] = (char)va_arg(va_cp, int);
    str_arg[arg_num] = 0;
    va_end(va_cp);
    dh_LineOut* dout = InputLine_General( byte, limit, 0, str_arg, 1);
    dh_limit_Free(limit);
    return dout;
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

static int char_check(const char* str, char** end, char check_char)
{
    const char* str_in = str;
    while( isspace(*str_in) )
        str_in++;
    char lower_char = 0;
    char upper_char = 0;
    if(!isalpha(check_char))
    {
        if(end) *end = (char*)str_in;
        return 0;
    }
    else if(islower(check_char))
    {
        lower_char = check_char;
        upper_char = toupper(lower_char);
    }
    else if(isupper(check_char))
    {
        upper_char = check_char;
        lower_char = tolower(upper_char);
    }
    if(*str_in == lower_char || *str_in == upper_char)
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

static int multi_char_check_CharArg(const char* str, char* args, char* result, int* err)
{
    if(args)
    {
        int args_len = strlen(args);
        for(int i = 0; i < args_len ; i++ )
        {
            char* end = NULL;
            if(char_check(str, &end, args[i]))
            {
                if(*end != 0)
                {
                    if(err) *err = -4;
                    return 0;
                }
                else
                {
                    if(result) *result = args[i];
                        else return 0;
                    return 1;
                }
            }
        }
    }
    if(err) *err = -1;
    return 0;
}

static int search_num(int byte, const char* str, char** end, int rc, int64_t min, int64_t max, int64_t* result, int *err)
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
            {
                if(err) *err = -1;
                return 0; // Not a digit
            }
        }
        long long r = strtoll(str_in, end, 10);
        if(errno == ERANGE)
        {
            if(err) *err = -3;
            return 0;  // Out of range
        }
        if(rc)
        {
            if( r < min || r > max )
            {
                if(err) *err = -2;
                return 0;    // Out of range
            }
        }
        if( !range_check(r, byte) )
        {
            if(err) *err = -3;
            return 0; // Out of range
        }
        *result = r;
        return 1;
    }
    else
    {
        if(err) *err = -1;
        return 0;
    }
}

static int64_t* num_array_check(int byte, const char* str, int range_check, int need_nums, va_list range, int same_range, int use_arr, int64_t **arr, int* err)
{
    if(need_nums > 0)
    {
        const char* str_in = str;
        char* end = (char*)str_in;
        int64_t* output = NULL;
        int64_t min = -1;
        int64_t max = -1;
        va_list va_range;
        va_copy(va_range, range);
        if(range_check)
        {
            if(same_range) // same range should just change min/max once
            {
                if(use_arr)
                {
                    min = arr[0][0];
                    max = arr[1][0];
                }
                else  // va-listed range
                {
                    min = va_arg(va_range, int64_t);
                    max = va_arg(va_range, int64_t);
                }
            }
        }
        for(int i = 0 ; i < need_nums ; i++)
        {
            if(range_check){
                if(!same_range)
                {
                    if(use_arr)
                    {
                        min = arr[0][i];
                        max = arr[1][i];
                    }
                    else
                    {
                        min = va_arg(va_range, int64_t);
                        max = va_arg(va_range, int64_t);
                    }
                }
            }
            int64_t result;
            if( i != 0 )
            {
                if( !isspace(*end) )
                {
                    if(err) *err = -4;
                    free(output);
                    va_end(va_range);
                    return NULL;
                }
            }
            if( search_num(byte, end, &end , range_check, min, max, &result, err) )
            {
                int64_t* output_p = (int64_t*)realloc( output, (i + 1) * sizeof(int64_t) );
                if(!output_p)
                {
                    *err = -5;
                    free(output);
                    va_end(va_range);
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
                va_end(va_range);
                free(output);
                return NULL;
            }
        }
        va_end(va_range);
        while( isspace(*end) )
            end++;
        if(*end != 0)
        {
            if(err) *err = -4;
            free(output);
            return NULL;
        }
        else
            return output;
    }
    else return NULL;
}

static int64_t* line_numarray_check(int byte, int* array_len, const char* str, int need_nums,
                             int range_nums, int64_t ** range, int* err, int repeat_check, int same_range,
                                    int unlimited_lens)
{
    int64_t* out = NULL;
    int arr_len = 0;
    int warning = 0;
    if(need_nums == 0) /* Don't need nums, so do nothing */
        return NULL;
    else
    {
        /* unlimited_lens = 1 : auto mode
         * If not switch to limited mode */
        int range_check = 0;
        int min = -1;
        int max = -1;
        if(range_nums > 0)
            range_check = 1;
        /* Just use one range */
        if(same_range)
        {
            min = range[0][0];
            max = range[0][1];
        }
        char* end = (char*)str;
        /* Make the input skip space */
        while(isspace(*end))
            end++;
        while(*end != 0) /* Break point */
        {
            /* Change range (if check range) */
            if(!same_range && range_check)
            {
                if(arr_len >= range_nums)
                {
                    /* Auto mode or not enough range */
                    if( unlimited_lens == 1 || need_nums == -1 || range_nums < need_nums){
                        if(err)
                            *err = DH_GIVEN_RANGE_ERROR; /* Even if error throwed, program try to process anyway */
                        if(warning == 0){
                            fprintf(stderr, _("An unsuitable range given! Although it's unexpected, the program would still run.\n"));
                            warning = 1;
                        }
                    }
                    range_check = 0;
                }
                else
                {
                    min = range[arr_len][0];
                    max = range[arr_len][1];
                }
            }
            int64_t result;
            if(need_nums != -1)
                if(arr_len >= need_nums)
                {
                    /* Not auto mode or limited nums of unlimited lens */
                    if(err) *err = DH_NOT_EXPECTED_AFTER;
                    goto arr_err;
                }
            if(search_num(byte, end, &end, range_check, min, max, &result, err))
            {
                if(repeat_check)
                {
                    for(int j = 0 ; j < arr_len ; j++)
                    {
                        if(out[j] == result) /* Find repeat */
                        {
                            if(err) *err = DH_REPEATED;
                            goto arr_err;
                        }
                    }
                }
                arr_len++;
                int64_t* new_arr = (int64_t*)realloc(out, arr_len * sizeof(int64_t));
                if(new_arr)
                {
                    out = new_arr;
                    out[arr_len - 1] = result;
                }
                else /* Out of memory */
                {
                    if(err) *err = DH_NO_MEMORY;
                    goto arr_err;
                }
            }
            else /* search num fail */
            {
                goto arr_err;
            }
            /* Check if end meet the needs */
            if(!isspace(*end))
            {
                if(err) *err = DH_NOT_EXPECTED_AFTER;
                goto arr_err;
            }
            while(isspace(*end))
                end++;
        }
        if( !unlimited_lens )
            if(arr_len < need_nums)
            {
                if(err) *err = DH_NOT_EXPECTED;
                goto arr_err;
            }
        *array_len = arr_len;
        if(arr_len == 0)
            if(err) *err = DH_NOT_EXPECTED;
        return out;
    }
arr_err:
    free(out);
    if(array_len) *array_len = 0;
    return NULL;
}


char *String_Copy(const char *o_str)
{
#if (defined __STDC_VERSION__ && __STDC_VERSION__ > 201710L) || _POSIX_C_SOURCE >= 200809L
    return strdup(o_str); // use strdup if provided
#else
    char* str = malloc( (strlen(o_str) + sizeof("") ) * sizeof(char));
    if(str)
    {
        strcpy(str, o_str);
        return str;
    }
    else return NULL;
#endif
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



static int range_check(int64_t num, int byte)
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

static int float_check(double num)
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

static void* resize_array(int byte, int o_byte, void* array, int len)
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



dh_LineOut *InputLine_Get_OneOpt(int range_check, int need_num, int arg_num, ...)
{
    va_list va;
    va_start(va,arg_num);
    dh_LineOut* out = InputLine_Get_OneOpt_va(64/8, range_check, need_num, arg_num, va);
    va_end(va);
    return out;
}



dh_LineOut *InputLine_Get_OneOpt_WithByte(int byte, int range_check, int need_num, int arg_num, ...)
{
    va_list va;
    va_start(va,arg_num);
    dh_LineOut* out = InputLine_Get_OneOpt_va(byte, range_check, need_num, arg_num, va);
    va_end(va);
    return out;
}

dh_LineOut *InputLine_Get_MoreDigits(int range_check, int need_nums, int arg_num, ...)
{
    va_list va;
    va_start(va, arg_num);
    dh_LineOut* out = InputLine_Get_MoreDigits_va(64/8, range_check, need_nums, arg_num, va, 0);
    va_end(va);
    return out;
}

dh_LineOut *InputLine_Get_MoreDigits_WithByte(int byte, int range_check, int need_nums, int arg_num, ...)
{
    va_list va;
    va_start(va, arg_num);
    dh_LineOut* out = InputLine_Get_MoreDigits_va(byte, range_check, need_nums, arg_num ,va, 0);
    va_end(va);
    return out;
}

#ifdef DH_USE_TRANSLATION_DEPRECATED

void String_Translate_FreeLocale()
{
    free(main_lang);
    main_lang = NULL;
    free(secondary_lang);
    secondary_lang = NULL;
}

static char* translation_pos()
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


void String_Translate_printfRaw(const char *str)
{
    char* trans = String_Translate(str);
    global_impl.printf_fn("%s",trans);
    free(trans);
}

void String_Translate_printfWithArgs(const char *str, ...)
{
    int err = 0;
    char* trans = String_TranslateWithErrCode(str, &err);
    va_list va;
    va_start(va,str);
    if(err == 0)
        global_impl.vprintf_fn(trans, va);
    else global_impl.printf_fn("%s", trans);
    va_end(va);
    free(trans);
}

static char* get_locale()
{
    if(!main_lang)
    {
        char* override_lang = dhlrc_ConfigContent("OverrideLang");
        if(override_lang == NULL || !strcmp(override_lang,""))
        {
            free(override_lang);
#if defined LC_MESSAGES
            /* POSIX defined LC_MESSAGES, so when using standard C this is also available */
            char* lang = setlocale(LC_MESSAGES, NULL);
#else
            /* Tested in Windows, the language variable is not the same as POSIX, but just leave this to make it run (hopefully) */
            char* lang = setlocale(LC_ALL, NULL);
#endif
            char* lang_copy = lang;
            if(lang_copy){ /* In case the return value is NULL */
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
            else{
                fprintf(stderr, "Not a valid language variable, will fallback to en_US.\n");
                main_lang = String_Copy("en_US");
            }
        }
        else
        {
            main_lang = String_Copy(override_lang);
            free(override_lang);
        }
        /* Determine translation file exists or fallback to en_US
         * But translation_pos need language variable, so main_lang shouldn't be null */
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


char* String_Translate(const char* str)
{
    return String_TranslateWithErrCode(str, NULL);
}

char* String_TranslateWithErrCode(const char* str, int* err)
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
            if(err) *err = 0;
            return output;
        }
        else{
            if(err) *err = -2; // no corresponding translate
            cJSON_Delete(trans_json);
            return String_Copy(str);
        }

    }
    else {
        if(err) *err = -1; // no translation file
        return String_Copy(str);
    }
}

#endif

int dh_string_getline(char** input, size_t* n, FILE* stream)
{
#if ((defined __STDC_ALLOC_LIB__ && __STDC_WANT_LIB_EXT2__ == 1 ) || (_POSIX_C_SOURCE - 0 ) >= 200809L)
    ssize_t ret =  getline(input, n, stream); // If provide getline, recommend using this.
    if(ret != -1)
        return 1;
    else return -1;
#else // my own getline implement

    if(!input || !n) // determine whether input/n is valid
    {
        errno = EINVAL;
        return -1;
    }

    if(*n <= 0)
        *n = 5; // Initize a 5-length buffer
    char* new_input = (char*)realloc(*input, *n * sizeof(char));

    if(new_input)
    {
        *input = new_input;
        char read_char;
        size_t char_num = 0;
        while( (read_char = fgetc(stream)) != EOF )
        {
            if(char_num == *n) // not enough memory for input
            {
                *n += 5;
                char* re_input = (char*)realloc(*input, *n * sizeof(char));
                if(re_input)
                    *input = re_input;
                else
                {
                    *n -= 5;
                    return -1;
                }
            }
            (*input)[char_num] = read_char; // write the num in
            char_num++;
            if(read_char == '\n')
                break; // Read finish
        }
        if(read_char != EOF)
        {
            if(char_num == *n)
            {
                *n += 5;
                char* final_input = (char*)realloc(*input, *n * sizeof(char));
                if(final_input)
                    *input = final_input;
                else
                {
                    *n -= 5;
                    return -1;
                }
            }
            (*input)[char_num] = '\0';
            return 1;
        }
        else return -1;
    }
    else return -1;

#endif
}

dh_limit * dh_limit_Init(dh_out_type type)
{
    if(type == Integer || type == NumArray || type == Float || type == Double || type == DoubleArray || type == FloatArray )
    {
        dh_limit* out = (dh_limit*)malloc(sizeof(dh_limit));
        if(out)
        {
            out->type = type;
            out->limit = NULL;
            out->limit_num = 0;
            out->same_range = 0;
            out->unlimited_lens = 0;
            out->lens = 0;
            return out;
        }
        else return NULL;
    }
    else return NULL;
}

int dh_limit_AddInt(dh_limit* limit, int64_t min, int64_t max)
{
    if(limit->type == Integer)
    {
        /* Integer only allow one min/max */
        if(limit->limit_num >= 1)
            return 0;
    }
    else if(limit->type == NumArray)
    {
        /* If not unlimited mode, check whether full */
        if(!(limit->unlimited_lens))
            if( limit->limit_num >= limit->lens )
                return 0;
        /* If same range, limit_num should be 1 */
        if(limit->same_range)
            if(limit->limit_num >= 1)
                return 0;
    }
    else return 0;

    int64_t** new_range = (int64_t**)realloc(limit->limit, (limit->limit_num + 1) * sizeof(int64_t*) );
    if(new_range)
    {
        new_range[limit->limit_num] = (int64_t*)malloc(2 * sizeof(int64_t));
        if(new_range[limit->limit_num])
        {
            new_range[limit->limit_num][0] = min;
            new_range[limit->limit_num][1] = max;
            limit->limit = (void**)new_range;
            limit->limit_num = limit->limit_num + 1;
            return 1;
        }
        else
        {
            free(new_range);
            return 0;
        }
    }
    else return 0;
}

int dh_limit_AddDouble(dh_limit* limit, double min, double max)
{
    if(limit->type == Float || limit->type == Double)
    {
        if(limit->limit_num >= 1)
            return 0;
    }
    else if(limit->type == FloatArray || limit->type == DoubleArray)
    {
        /* If not unlimited mode, check whether full */
        if(!(limit->unlimited_lens))
            if( limit->limit_num >= limit->lens )
                return 0;
        /* If same range, limit_num should be 1 */
        if(limit->same_range)
            if(limit->limit_num >= 1)
                return 0;
    }
    else return 0;

    double** new_range = (double**)realloc( limit->limit , (limit->limit_num + 1) * sizeof(double*));
    if(new_range)
    {
        new_range[limit->limit_num] = (double*)malloc( 2 * sizeof(double));
        if(new_range[limit->limit_num])
        {
            new_range[limit->limit_num][0] = min;
            new_range[limit->limit_num][1] = max;
            limit->limit = (void**)new_range;
            limit->limit_num = limit->limit_num + 1;
            return 1;
        }
        else
        {
            free(new_range);
            return 0;
        }
    }
    else return 0;
}

void dh_limit_Free(dh_limit* limit)
{
    for(int i = 0 ; i < limit->limit_num ; i++)
    {
        free( (limit->limit)[i] );
    }
    free(limit->limit);
    free(limit);
}

int dh_limit_SetArrayArgs(dh_limit* limit ,int lens, int same_range, int check_repeated, int unlimited_lens)
{
    /* lens < -1 is not valid */
    if(lens < -1 )
        return 0;
    if(limit->type == NumArray || limit->type == FloatArray || limit->type == DoubleArray )
    {
        /* Set unlimited mode */
        if((lens == -1 && unlimited_lens == -1) || unlimited_lens == 1)
            /* let program decide whether using unlimited lens ( that when len == -1 )
             * or when unlimited_lens = 1 use it */
        {
            limit->unlimited_lens = 1;
        }
        else limit->unlimited_lens = 0;
        /* Set lens */
        limit->lens = lens;
        /* Set same range */
        if(same_range)
            limit->same_range = 1;
        else limit->same_range = 0;
        /* Set repeat check */
        if(check_repeated)
            limit->check_repeated = 1;
        else limit->check_repeated = 0;
        return 1;
    }
    else return 0;
}

