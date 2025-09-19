/*  download_file - Download file from Mojang
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

#include "download_file.h"

int
dh_file_progress_callback (void *data, curl_off_t total, curl_off_t current,
                           curl_off_t unused0, curl_off_t unused1)
{
    char *description = data;
    double percentage = (double)current / total * 100;

    fprintf (stderr,
             "[%.2f%%] Copying %s."
             " (%" CURL_FORMAT_CURL_OFF_T "/%" CURL_FORMAT_CURL_OFF_T ").\r",
             percentage, description, current, total);
    if (total == current && total != 0)
        fprintf (stderr, "\n");
    return 0;
}
