/*  uncompress.h - Uncompression functions
    Copyright (C) 2024 Dream Helium
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

#ifndef UNCOMPRESS_H
#define UNCOMPRESS_H

#include "dh_string_util.h"
#ifdef __cplusplus
extern "C"{
#endif

int dhlrc_extract(const char* path, const char* dest);
int dhlrc_extract_part(const char* path, const char* dest, DhStrArray* arr);

#ifdef __cplusplus
}
#endif


#endif /* UNCOMPRESS_H */
