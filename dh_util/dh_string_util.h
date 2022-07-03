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

/** The type of the dh_Line_IO */
typedef enum dh_out_type{
    Integer, Float, Double, Character, NumArray, String, Empty
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


/** Get a line input and return output (1 number or character), return 64bit num by default */
dh_LineOut* InputLine_Get_OneOpt(int range_check, int need_num, int arg_num, int min, int max, ...);
dh_LineOut* InputLine_Get_OneOpt_WithByte(int byte, int range_check, int need_num, int arg_num, int min, int max, ...);
/** Get a line input and return output (n numbers or character)
 *  should pass like this: (nums): min and max, (char): char.
 */
dh_LineOut* InputLine_Get_MoreDigits(int range_check, int need_nums, int arg_num, ...);
dh_LineOut* InputLine_Get_MoreDigits_WithByte(int byte, int range_check, int need_nums, int arg_num, ...);
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

char* String_Translate(const char* str);
void String_Translate_printfRaw(const char* str);
void String_Translate_printfWithArgs(const char* str, ...);
void String_Translate_FreeLocale();

char* String_Copy(const char *o_str);

dh_StrArray* dh_StrArray_Init(const char* str);

int dh_StrArray_AddStr(dh_StrArray** arr ,const char* str);

void dh_StrArray_Free(dh_StrArray* arr);



#ifdef __cplusplus
}
#endif

#endif // DH_STRING_UTIL_H
