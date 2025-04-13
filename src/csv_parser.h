/*  csv_parser - csv parser
    Copyright (C) 2025 Dream Helium
    This file is part of dhlrc.

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

#ifndef CSV_PARSER_H
#define CSV_PARSER_H

#include <glib.h>

G_BEGIN_DECLS

typedef GPtrArray DhCsvArray;

DhCsvArray* dh_csv_parse(const char* data);

G_END_DECLS

#endif /* CSV_PARSER_H */