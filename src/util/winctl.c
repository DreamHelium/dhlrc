/*  winctl - Window/Screen control utilities
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

#include <stdlib.h>
#include <stdio.h>
#include "winctl.h"

static void* isoc_newwin(int a, int b, int c, int d) {
    system_clear();
    return NULL;
}

static int isoc_delwin(void* a)
{
    return 0;
}

static int isoc_mvwin(void* instance, int y, int x)
{
    dh_win_instance* win_instance = (dh_win_instance*)instance;
    if(win_instance->curser_moved == 0)
    {
        for(int i = 0 ; i < y ; i++)
            printf("\n");
        for(int i = 0 ; i < x ; i++)
            printf(" ");
        fflush(stdout);
        return 0;
    }
    else return 0;
}

static int isoc_vprintf(void* a, const char* str, va_list va)
{
    return vprintf(str, va);
}

static dh_win_impl isoc_impl = {isoc_newwin, isoc_delwin, system_clear, isoc_mvwin, isoc_vprintf};
static dh_win_impl* default_impl = NULL;
static dh_win_type default_type = iso;


static dh_win_impl* win_impl_dup(dh_win_impl* impl)
{
    dh_win_impl* new_impl = (dh_win_impl*)malloc(sizeof(dh_win_impl));
    if(new_impl)
    {
        new_impl->newwin_fn = impl->newwin_fn;
        new_impl->clear_fn = impl->clear_fn;
        new_impl->delwin_fn = impl->delwin_fn;
        new_impl->mvwin_fn = impl->mvwin_fn;
        new_impl->vprintf_fn = impl->vprintf_fn;
        return new_impl;
    }
    else return NULL;
}

int init_win_impl(dh_win_impl* impl, dh_win_type type)
{
    dh_win_impl* new_impl = win_impl_dup(impl);
    if(new_impl)
    {
        default_impl = new_impl;
        default_type = type;
        return 1;
    }
    else return 0;
}

dh_win_instance * dhwinctl_init_default_instance()
{
    if(default_impl == NULL)
        default_impl = &isoc_impl;
    return dhwinctl_init_instance(default_impl, default_type);
}


dh_win_instance * dhwinctl_init_instance(dh_win_impl* impl, dh_win_type type)
{
    dh_win_instance* instance = (dh_win_instance*)malloc(sizeof(dh_win_instance));
    if(instance)
    {
        instance->type = type;
        instance->curser_moved = 0;
        dh_win_impl* new_impl = win_impl_dup(impl);
        if(new_impl)
        {
            instance->impl = new_impl;
            instance->win = NULL;
            return instance;
        }
        else
        {
            free(instance);
            return NULL;
        }
    }
    else return NULL;
}

int dhwinctl_newwin(dh_win_instance* instance, int lines, int cols, int starty, int startx)
{
    void* new_win = instance->impl->newwin_fn(lines, cols, starty, startx);
    if( new_win )
    {
        instance->win = new_win;
        return 1;
    }
    else if( instance->type == iso )
    {
        instance->impl->mvwin_fn(NULL, starty, startx);
        return 1;
    }
    else return 0;
}

int dhwinctl_clear(dh_win_instance* instance)
{
    return instance->impl->clear_fn( instance->win );
}

int dhwinctl_mvwin(dh_win_instance* instance, int y, int x)
{
    return instance->impl->mvwin_fn( instance->win, y, x);
}

int dhwinctl_print(dh_win_instance* instance, const char* str, ...)
{
    va_list va;
    va_start(va, str);
    int ret = instance->impl->vprintf_fn( instance->win, str, va);
    va_end(va);
    return ret;
}


void dhwinctl_end_instance(dh_win_instance* instance)
{
    instance->impl->delwin_fn(instance->win);
    free(instance->impl);
    free(instance);
}


int system_clear()
{
#if (defined(__WINDOWS__) || defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
    return system("cls");
#else
    return system("clear");
#endif
}

void end_winctl()
{
    free(default_impl);
}
