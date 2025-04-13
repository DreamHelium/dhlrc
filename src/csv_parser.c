/*  csv_parser - csv parser
    Copyright (C) 2025 Dream Helium
    This file is part of dhlrc.

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

#include "csv_parser.h"

DhCsvArray* dh_csv_parse(const char* data)
{
    char** split_line = g_strsplit(data, "\n", -1);
    char** sld = split_line;
    
    DhCsvArray* ret = g_ptr_array_new_with_free_func((GDestroyNotify)g_strfreev);
    for( ; *sld ; sld++)
    {
        if(**sld != 0)
        {
            char** val = g_strsplit(*sld, ",", -1);
            g_ptr_array_add(ret, val);
        }
    }
    g_strfreev(split_line);
    return ret;
}