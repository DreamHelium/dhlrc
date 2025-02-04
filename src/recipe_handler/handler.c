/*  handler - Recipes Handler
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

#include "handler.h"
#include <gmodule.h>

typedef GPtrArray RecipesArray;

typedef struct Handler
{
    GModule* module;
    const char* recipe_type;
    
} Handler;

void recipe_handler_init(const char* recipelib_path)
{

}

void recipe_handler_free();