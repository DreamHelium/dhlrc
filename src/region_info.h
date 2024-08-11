/*  region_info - region info struct
    Copyright (C) 2024 Dream Helium
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

#ifndef REGION_INFO_H
#define REGION_INFO_H

#include <glib.h>
#include "region.h"

G_BEGIN_DECLS

void  region_info_new(Region* region, GDateTime* time, const gchar* description);
void  region_info_list_clear();
guint region_info_list_length();
Region*  region_info_get_region(guint id);
GDateTime* region_info_get_time(guint id);
/* Don't free the string! */
gchar* region_info_get_description(guint id);

G_END_DECLS

#endif /* REGION_INFO_H */