/*  config - make some config
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

#ifndef CONFIG_H
#define CONFIG_H

#include <cjson/cJSON.h>
#include <glib.h>

#ifdef __cplusplus
extern "C"{
#endif

void dhlrc_make_config();
void dhlrc_reread_config();
void dh_exit1();
/** Get game directory in config, if fail return NULL */
char* dh_get_game_dir();
/** Get cache directory in config, if fail return what should be default */
char* dh_get_cache_dir();
/** Get Override Game Version in config, if fail return 1.18.2 */
char* dh_get_version();
/** Get recipe directory, if fail return decompressed position */
char* dh_get_recipe_dir();
/** This is used by mcdir file */
char* dh_get_translation_dir();


#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H */
