/*  common_info - common info struct
    Copyright (C) 2025 Dream Helium
    This file is part of litematica_reader_c (aka dhlrc).

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

#include "common_info.h"

DH_TYPE_REGISTER_GET_FUNC(DhRegion, dh, region, region_free)
DH_TYPE_REGISTER_GET_FUNC(DhItemList, dh, item_list, item_list_free)
DH_TYPE_REGISTER_GET_FUNC(DhNbtInterfaceCpp, dh, nbt_interface_cpp, dh_nbt_instance_cpp_free)
