/*  recipe_shaped - Recipe Shaped Object and class
    Copyright (C) 2023 Dream Helium
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

#ifndef RECIPE_SHAPED_H
#define RECIPE_SHAPED_H

#include "recipe_general.h"

G_BEGIN_DECLS

#define RECIPE_TYPE_SHAPED recipe_shaped_get_type()
G_DECLARE_FINAL_TYPE(RecipeShaped, recipe_shaped, RECIPE, SHAPED, RecipeGeneral)

G_END_DECLS

#endif /* RECIPE_SHAPED_H */
