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

#ifndef DH_STRING_UTIL_H
#define DH_STRING_UTIL_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

/** The type of the dh_Line_IO */
typedef enum dh_out_type{
    Integer, Float, Double, Character, NumArray, String, DoubleArray, FloatArray ,Empty
} dh_out_type;

typedef struct dh_LineOut
{
    /** Return type */
    dh_out_type type;

    union{
        /** Number value of the output */
        int64_t num_i;
        double num_f;

        /** Character value of the output */
        char val_c;

        /** String or NumArray value or Empty */
        struct{
          void* val;
          int len;
        };
    };

    /** byte of the num value */
    int byte;
}
dh_LineOut;

typedef struct dh_StrArray{
    char** val;
    int num;
} dh_StrArray;

typedef struct dh_string_impl{

    /** printf() replacement */
    int (*printf_fn)(const char*, ...);

    /** vprintf() replacement */
    int (*vprintf_fn)(const char*, va_list);

    /** getline() replacement (without FILE*), in some cases you need to code implement by yourself */
    int (*getline_fn)(char**, size_t*);

    /** free memory function used in getline() */
    void (*getline_free)(void*);

    /** internal error print (Default: to stderr) */
    int (*err_print_fn)(const char*);
} dh_string_impl;

typedef struct dh_limit{

    /* type of the desired type */
    dh_out_type type;
    /* range of the desired numbers */
    void** limit;
    /* numbers of range */
    int limit_num;
    /* Only available for array mode, if = 1 is the same range in all controls */
    int same_range;
    /* Only available for array mode, if = 1 shows that it need unlimited numbers */
    int unlimited_lens;
    /* Only available for array mode, it's the numbers of needed numbers (if unlimited_lens = 1 it will be ignored) */
    int lens;
    /* Only available for array mode, if = 1 check for repeated numbers */
    int check_repeated;
} dh_limit;

/** Change implement of this string util, might be needed if using in GUI lib or other cases */
void dh_string_ChangeImpl(dh_string_impl* impl);

/** Get a line input and return output (1 number or character), return 64bit num by default */
dh_LineOut* InputLine_Get_OneOpt(int range_check, int need_num, int arg_num, ...);
dh_LineOut* InputLine_Get_OneOpt_WithByte(int byte, int range_check, int need_num, int arg_num, ...);
/** Get a line input and return output (n numbers or character)
 *  should pass like this: (nums): min and max, (char): char. */
dh_LineOut* InputLine_Get_MoreDigits(int range_check, int need_nums, int arg_num, ...);
dh_LineOut* InputLine_Get_MoreDigits_WithByte(int byte, int range_check, int need_nums, int arg_num, ...);
/** A better way to get output, byte will be ignored in Float/Double(or array) mode */
dh_LineOut* InputLine_General(int byte, dh_limit* limit, int get_string, char* args, int allow_empty);
/** Transform a String to number array */
long* NumArray_From_String(const char* string, int* nums, int char_check);

dh_LineOut* dh_LineOut_CreateNum(int64_t num, int byte);
dh_LineOut* dh_LineOut_CreateFloat(double num);
dh_LineOut* dh_LineOut_CreateDouble(double num);
dh_LineOut* dh_LineOut_CreateChar(char c);
dh_LineOut* dh_LineOut_CreateNumArray(void* array, int len, int byte, int o_byte);
dh_LineOut* dh_LineOut_CreateString(const char* str);
dh_LineOut* dh_LineOut_CreateEmpty();

void dh_LineOut_Free(dh_LineOut* lo);

#ifdef DH_USE_TRANSLATION_DEPRECATED
/* I'm porting this project to gettext, so these functions are of no use. */
char* String_Translate(const char* str);
/** @brief Return Translation and also the err code \n
  *        err -1: no translation file \n
  *        err -2: no corresponding translation
  *        @return Translation
  */
char* String_TranslateWithErrCode(const char* str, int* err);
void String_Translate_printfRaw(const char* str);
void String_Translate_printfWithArgs(const char* str, ...);
void String_Translate_FreeLocale();
#endif

char* String_Copy(const char *o_str);

dh_StrArray* dh_StrArray_Init(const char* str);

int dh_StrArray_AddStr(dh_StrArray** arr ,const char* str);

void dh_StrArray_Free(dh_StrArray* arr);

/** Use getline() if provided, otherwise use another implement */
int dh_string_getline(char** input, size_t* n, FILE* stream);

/** Initize a limit type
 *  WARNING: Recommend that don't edit struct directly */
dh_limit* dh_limit_Init(dh_out_type type);
/** Change conditons of using array, lens = -1 : use unlimited lens
 *  WARNING: You can edit the struct directly, but that's not recommended */
int dh_limit_SetArrayArgs(dh_limit* limit, int lens, int same_range, int check_repeated);
/** Add int num for limit */
int dh_limit_AddInt(dh_limit* limit, int64_t min, int64_t max);
/** Add double num for limit */
int dh_limit_AddDouble(dh_limit* limit, double min, double max);
/** Free dh_limit */
void dh_limit_Free(dh_limit* limit);


#ifdef __cplusplus
}
#endif

#endif // DH_STRING_UTIL_H
