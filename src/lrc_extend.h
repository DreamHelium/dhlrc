/*  lrc_extend - Litematica Reader Extension
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

#ifndef LRC_EXTEND_H
#define LRC_EXTEND_H

#ifdef __cplusplus
extern "C"{
#endif

#include "libnbt/nbt.h"

void start_lrc_extend(NBT* root);

#ifdef __cplusplus
}
#endif
#endif // LRC_EXTEND_H
