/*  process_state - Processing state
    Copyright (C) 2025 Dream Helium
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

#ifndef PROCESS_STATE_H
#define PROCESS_STATE_H

#include "region.h"
#include "dhlrc_list.h"

G_BEGIN_DECLS

ItemList* item_list_new_from_region(Region* region);

G_END_DECLS

#endif /* PROCESS_STATE_H */