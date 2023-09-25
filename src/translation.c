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

#include "translation.h"
#include <glib.h>
static gboolean translation_inited = FALSE;
void translation_init()
{
    if(!translation_inited)
    {
#ifndef DH_DISABLE_TRANSLATION
#ifdef G_OS_WIN32
        setlocale(LC_CTYPE, ".UTF-8");
#else
        setlocale(LC_CTYPE, "");
#endif /* G_OS_WIN32 */
        setlocale(LC_MESSAGES, "");
        bindtextdomain("dhlrc", "locale");
        textdomain("dhlrc");
#endif
    }
}
