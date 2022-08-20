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

#ifndef WINCTL_H
#define WINCTL_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdarg.h>

typedef enum dh_win_type{
    iso, cli, gui
} dh_win_type;

typedef struct dh_win_impl{

    /** Start a new window */
    void* (*newwin_fn)(int, int, int, int);
    /** End a window */
    int (*delwin_fn)(void*);
    /** Clear window */
    int (*clear_fn)(void*);
    /** Move curses */
    int (*mvwin_fn)(void*, int, int);
    /** Print (using va_list) */
    int (*vprintf_fn)(void*, const char*, va_list);


} dh_win_impl;

typedef struct dh_win_instance{

    /** type */
    dh_win_type type;
    /** Implement */
    dh_win_impl* impl;
    /** Used in ISO-C : curser moved */
    int curser_moved;
    /** Window variable */
    void* win;

} dh_win_instance;

/** Initialize a window/screen implement */
int init_win_impl(dh_win_impl* impl, dh_win_type type);
/** Initialize an instance (using default) */
dh_win_instance* dhwinctl_init_default_instance();
/** Initialize an instance (using another) */
dh_win_instance* dhwinctl_init_instance(dh_win_impl* impl, dh_win_type type);
/** Initialize a window */
int dhwinctl_newwin(dh_win_instance* instance, int lines, int cols, int starty, int startx);
/** Clear screen */
int dhwinctl_clear(dh_win_instance* instance);
/** Move curser */
int dhwinctl_mvwin(dh_win_instance* instance, int y, int x);
/** Print */
int dhwinctl_print(dh_win_instance* instance, const char* str, ...);
/** End an instance */
void dhwinctl_end_instance(dh_win_instance* instance);

void end_winctl();

/** Clear provided in system */
int system_clear();

#ifdef __cplusplus
}
#endif

#endif // WINCTL_H
