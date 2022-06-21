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
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

char *InputLine_Get_OneOpt(int need_num, int min, int max, int arg_num, ...)
{
    char* input = NULL;
    size_t size = 0;
    int gret = -2;
    while((gret = getline(&input, &size, stdin)) != -1)
    {
        char* inputl = input;
        while(*inputl == ' ')
            inputl++;
        if(need_num) // Try to read the num first
        {
            if(*inputl >= '0' && *inputl <= '9')
            {
                char* end = NULL;
                long value = strtol(inputl, &end, 10);
                if(inputl == end)
                {
                    printf("Unexpected string, please enter again: ");
                    continue;
                }
                while(*end == ' ')
                    end++;
                if(*end != '\n')
                {
                    printf("Please enter expected string or number: ");
                    continue;
                }
                else if(value < min || value > max)
                {
                    printf("Unexpected number, please enter again: ");
                    continue;
                }
                else
                    return input;
            }
        }
        {
            va_list va;
            va_start(va,arg_num);
            for(int i = 0 ; i < arg_num ; i++)
            {
                char* input_after_blank = inputl;
                char need_process = va_arg(va,int);
                if(*input_after_blank == need_process)
                {
                    input_after_blank++;
                    while(*input_after_blank == ' ')
                        input_after_blank++;
                    if(*input_after_blank != '\n')
                    {
                        //printf("Unrecognized string, please enter again: ");
                        //va_end(va);
                        break;
                    }
                    else
                    {
                        char* output = (char*)malloc(2* sizeof(char));
                        strncpy(output,inputl,1);
                        output[1] = '\0';
                        va_end(va);
                        free(input);
                        return output;
                    }
                }
            }
            printf("Unrecognized string, please enter again: ");
            va_end(va);
        }
    }
    if(gret == -1)
    {
        printf("Terminated output!\n");
        free(input);
        return NULL;
    }
    return NULL;
}


char *InputLine_Get_MoreDigits(int need_nums, int arg_num, ...)
{
    char* input = NULL;
    size_t size = 0;
    int gret = -2;
    int n_num = need_nums;
    while((gret = getline(&input, &size, stdin)) != -1)
    {
        char* inputl = input;
        // ignore blanket.
        while(*inputl == ' ')
              inputl++;

        if(need_nums <= 0)
            n_num = 0;
        {
            // read and check numbers.
            if(*inputl == '-' || isdigit(*inputl))
            {
                va_list va;
                va_start(va, arg_num);
                int i = 0;
                for(i = 0 ; i < need_nums; i++)
                {
                    if(*inputl == '-' || isdigit(*inputl))
                    {
                        // it's a num, step in.
                        int min = va_arg(va,int);
                        int max = va_arg(va,int);
                        char* end = NULL;
                        long value = strtol(inputl, &end, 10);
                        if(inputl == end)
                        {
                            printf("Unexpected string! Please enter again: ");
                            break;
                        }
                        if(value < min || value > max)
                        {
                            printf("Unexpected number! Please enter again: ");
                            break;
                        }
                        while(*end == ' ')
                            end++;
                        inputl = end;
                    }
                    else
                    {
                        printf("Unexpected character! Please enter again: ");
                        break;
                    }
                }
                va_end(va);
                if(i == need_nums)
                {
                    while(*inputl == ' ')
                        inputl++;
                    if(*inputl != '\n')
                        printf("Unexpected character! Please enter again: ");
                    else
                        return input;
                }
            }
            else // read character
            {
                char* output = NULL;
                char output_char = inputl[0];

                inputl++;
                while(*inputl == ' ')
                    inputl++;
                if(*inputl != '\n')
                {
                    printf("Unexpected character, please enter again: ");
                    continue;
                }

                va_list va;
                va_start(va,arg_num);

                for(int i = 0 ; i < (n_num * 2); i++)
                {
                    va_arg(va,int);
                }
                for(int i = 0 ; i < arg_num ; i++)
                {
                    char arg_char = va_arg(va,int);
                    if(output_char == arg_char)
                    {
                        free(input);
                        va_end(va);
                        output = (char*)malloc(2 * sizeof(char));
                        output[0] = output_char;
                        output[1] = '\0';
                        return output;
                    }
                }
                printf("Unexpected character, please enter again: ");
                va_end(va);
            }
        }
    }
    if(gret == -1)
    {
        printf("Terminated output!\n");
        free(input);
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
