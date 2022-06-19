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

#ifndef NBT_LITEREADER_H
#define NBT_LITEREADER_H
#ifdef __cplusplus
extern "C"{
#endif

#include "libnbt/nbt.h"

int nbtlr_Start(NBT* root, NBT* parent);
int nbtlr_List(NBT* given_nbt, NBT *parent);
int nbtlr_ListItem(NBT* given_nbt);
NBT* nbtlr_ToNextNBT(NBT* root, int n);

#ifdef __cplusplus
}
#endif

#endif // NBT_LITEREADER_H
