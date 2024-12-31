/*  nbt_util - nbt utility
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

#include "nbt_util.h"
#include "dh_string_util.h"
#include "dh_file_util.h"

int nbtlr_save(NBT* root)
{
    int bit = 1;
    size_t len = 0;
#ifndef LIBNBT_USE_LIBDEFLATE
    size_t old_len = 0;
#endif
    uint8_t* data = NULL;
    while(1)
    {
        len = 1 << bit;
        data = (uint8_t*)malloc(len * sizeof(uint8_t));
        int ret = NBT_Pack(root, data, &len);
        if(ret == 0)
        {
#ifndef LIBNBT_USE_LIBDEFLATE
            if(old_len != len) // compress not finish due to a bug in old libnbt (in submodule)
            {
                old_len = len;
                free(data);
                bit++;
                continue;
            }
#endif
            char* input = NULL;
            size_t size = 0;
            if(dh_getline(&input, &size, stdin) != -1)
            {
                int str_len = strlen(input);
                input[str_len - 1] = 0;
                dh_write_file(input, data, len);
                free(data);
                free(input);
                return 1;
            }
            else
            {
                free(input);
                return 0;
            }
        }
        else if(bit < 25)
        {
            free(data);
            bit++; // It might be not enough space
        }
        else
        {
            free(data);
            return 0;
        }
    }
    return 0;
}

int dhlrc_nbt_save(NBT* root, const char* pos)
{
    int bit = 1;
    size_t len = 0;
#ifndef LIBNBT_USE_LIBDEFLATE
    size_t old_len = 0;
#endif
    uint8_t* data = NULL;
    while(1)
    {
        len = 1 << bit;
        data = (uint8_t*)malloc(len * sizeof(uint8_t));
        int ret = NBT_Pack(root, data, &len);
        if(ret == 0)
        {
#ifndef LIBNBT_USE_LIBDEFLATE
            if(old_len != len) // compress not finish due to a bug in old libnbt (in submodule)
            {
                old_len = len;
                free(data);
                bit++;
                continue;
            }
#endif
            if(pos)
            {
                dh_write_file(pos, data, len);
                free(data);
                return 1;
            }
            else
            {
                return 0;
            }
        }
        else if(bit < 25)
        {
            free(data);
            bit++; // It might be not enough space
        }
        else
        {
            free(data);
            return 0;
        }
    }
    return 0;
}

NBT *nbtlr_to_next_nbt(NBT *root, int n)
{
    NBT* next_nbt = root;
    /* Protection for "NULL" situation */
    for(int i = 0 ; i < n && next_nbt ; i++)
        next_nbt = next_nbt->next;
    return next_nbt;
}