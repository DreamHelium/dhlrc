/*  common - Common Functions
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

#ifndef MAIN_H
#define MAIN_H

#include <gio/gio.h>
#include <glib.h>

G_BEGIN_DECLS

gboolean dhlrc_common_contents_init (const char *prname);
gboolean dhlrc_common_contents_is_inited ();
gboolean dhlrc_config_is_listened ();
void dhlrc_common_contents_free ();
GResource *dhlrc_get_resource ();
void dhlrc_init (const char *prname);
gboolean dhlrc_is_inited ();
void dhlrc_cleanup ();

G_END_DECLS

#endif /* MAIN_H */