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

/** Get a line input and return output (1 number or character) */
char* InputLine_Get_OneOpt(int need_num, int min, int max, int arg_num, ...);
/** Get a line input and return output (n numbers or character)
 *  should pass like this: (nums): min and max, (char): char.
 */
char* InputLine_Get_MoreDigits(int need_nums, int arg_num, ...);
/** Transform a String to number array */
long* NumArray_From_String(const char* string, int* nums, int char_check);


#ifdef __cplusplus
}
#endif

#endif // DH_STRING_UTIL_H
