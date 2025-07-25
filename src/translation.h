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

#ifndef DH_DISABLE_TRANSLATION

#undef signals
#include "feature/mcdata_feature.h"
#include "recipe_util.h"
#include <libintl.h>
#include <locale.h>

#define _(str) gettext (str)
#define N_(str) str
#define trm(str) name_block_translate (str)
#define mctr(str, large_version) dhlrc_get_translation (str, large_version)
#define ERROR_TITLE _ ("Error!")
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    void translation_init (const char *prog_name);
    char *replace_at_with_slash (char *str);

#ifdef __cplusplus
}
#endif

#endif
