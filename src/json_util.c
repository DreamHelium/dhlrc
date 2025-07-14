/*  json_util - cJSON utils
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

#include "json_util.h"
#include <dhutil.h>

cJSON *
dhlrc_file_to_json (const char *pos)
{
    gsize size;
    char *data = dh_read_file (pos, &size);
    if (data)
        {
            cJSON *json_data = cJSON_ParseWithLength (data, size);
            free (data);
            if (json_data)
                return json_data;
            else
                return NULL;
        }
    else
        return NULL;
}