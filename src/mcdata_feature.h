/*  mcdata_feature - Mcdata features
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

#ifndef MCDATA_FEATURE_H
#define MCDATA_FEATURE_H

#include <cjson/cJSON.h>
#include <glib.h>
#include <gmodule.h>

G_BEGIN_DECLS

typedef void (*Sig)(void* data);
typedef void (*SetFunc)(void*, int);
typedef void (*SigWithSet)(void*, SetFunc, void*);

void dhlrc_mcdata_enable(GModule* module);
gboolean dhlrc_mcdata_enabled();
int dhlrc_init_translation_from_file(const char* filename, const char* large_version);
int dhlrc_init_translation_from_content(const char* content, const char* large_version);
const char* dhlrc_get_translation(const char* name, const char* large_version);
int dhlrc_download_manifest(const char* dir, SigWithSet sig,
 SetFunc func, void* data, void* klass);
int dhlrc_manifest_download_code();
void dhlrc_manifest_reset_code();
int dhlrc_manifest_downloaded();
const cJSON* dhlrc_get_manifest();
char* dhlrc_get_version_json_string(const char* version, SetFunc set_func, void* klass, int min, int max);
char* dhlrc_get_translation_file(const char* filename, const char* large_version, const char* lang);
void dhlrc_cleanup_manifest();

G_END_DECLS

#endif //MCDATA_FEATURE_H
