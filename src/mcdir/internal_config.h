/*  internal_config - Internal config settings
    Copyright (C) 2025 Dream Helium
    This file is part of dhmcdir.

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

#ifndef INTERNAL_CONFIG_H
#define INTERNAL_CONFIG_H

#include "../dhutil/dh_string_util.h"
#include <cjson/cJSON.h>

void dhmcdir_exit ();
void dhmcdir_update_content ();
void dhmcdir_set_single_translation_dir (const char *dir);
void dhmcdir_set_multi_translation_dir (DhStrArray *arr);
const DhStrArray *dhmcdir_get_translation_dir ();

#endif // INTERNAL_CONFIG_H
