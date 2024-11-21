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
#include "dh_file_util.h"
#include <glib.h>
#include <locale.h>

static gboolean set_locale_to_utf8();

static gboolean translation_inited = FALSE;
void translation_init(const char* prog_name)
{
    if(!translation_inited)
    {
        char* dir = dh_file_get_current_program_dir(prog_name);
        set_locale_to_utf8();
        char* locale_dir = g_strconcat(dir, G_DIR_SEPARATOR_S, "locale", NULL);
        bindtextdomain("dhlrc", locale_dir);
        /* Force the output for UTF-8 */
        bind_textdomain_codeset("dhlrc", "UTF-8");
        textdomain("dhlrc");
        g_free(locale_dir);
        g_free(dir);
    }
}

static gboolean set_locale_to_utf8()
{
    /* First get the locale of the current environment */
    gchar* current_locale = setlocale(LC_CTYPE, "");
    if(g_str_has_suffix(current_locale, "UTF-8") ||
        g_str_has_suffix(current_locale, "utf-8") ||
        g_str_has_suffix(current_locale, ".65001")
    )
    {
        /* We don't need to change anything */
        setlocale(LC_MESSAGES, "");
        return TRUE;
    }
    else
    {
        current_locale = g_strdup(current_locale);
        gchar* current_locale_d = current_locale;
        while(current_locale_d)
        {
            if(*current_locale_d == '.' || *current_locale_d == 0)
                break;
            current_locale_d++;
        }
        if(*current_locale_d == '.')
            *current_locale_d = 0;
        /* Set new locale */
        gchar* new_locale = NULL;
#ifdef G_OS_WIN32
        new_locale = g_strconcat(current_locale, ".65001", NULL);
#else
        new_locale = g_strconcat(current_locale, ".UTF-8", NULL);
#endif
        g_free(current_locale);
        current_locale = setlocale(LC_CTYPE, new_locale);

        /* confirm success */
        if(current_locale)
        {
            if(g_str_equal(current_locale, new_locale))
            {
                setlocale(LC_MESSAGES, "");
                g_free(new_locale);
                return TRUE;
            }
            else {
                g_free(new_locale);
                return FALSE;
            }
        }
        else
        {
            g_free(new_locale);
            return FALSE;
        }
    }
}

char* replace_at_with_slash(char* str)
{
    char* ret = g_strdup(str);
    char* at_pos = strchr(ret, '&');
    at_pos[0] = '_';
    return ret;
}