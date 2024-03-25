/*  translaton.h - Easier handler of translation
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

#ifndef TRANSLATION_H
#define TRANSLATION_H

#ifdef __cplusplus
extern "C"{
#endif
void translation_init();
#ifndef DH_DISABLE_TRANSLATION
#include <libintl.h>
#include <locale.h>
#include "recipe_util.h"
#define _(str) gettext (str)
#define N_(str) str
#define trm(str) name_block_translate (str)
#else
#define _(str) str
#define trm(str) str
#endif

#ifdef __cplusplus
}
#endif

#endif
