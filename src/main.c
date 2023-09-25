/*  litematica_reader_c - litematic file reader in C
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

#include <stdio.h>
#include <glib.h>
/*#include "dhlrc_config.h"*/
#include "main.h"
#include "translation.h"

int main(int argc,char** argb)
{
    translation_init();
    if(argc == 1)
    {
        printf(_("Usage: %s [options] [file]\n\n"
            "  -r, --reader\tEnter NBT reader mode\n"
            "  -b, --block\tEnter litematica block reader\n"
            "  -l, --list\tEnter litematica material list with recipe combination\n"
        ), argb[0]);
    }
    return main_isoc(argc, argb);
}
