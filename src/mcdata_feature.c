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

#include "mcdata_feature.h"

#include <cjson/cJSON.h>

typedef int (*InitTr)(const char*, const char*);
typedef void(*CleanupTr)();
typedef const char* (*GetTr)(const char*, const char*);
typedef int (*Download)(const char*, SigWithSet, SetFunc, void*, void*);
typedef int (*Downloaded)();
typedef const cJSON* (*GetJSON)();
typedef char* (*GetString)(const char*, SetFunc, void*, int, int);
typedef char* (*GetTrFile)(const char*, const char*, const char*);

static gboolean enabled = FALSE;

static InitTr init_tr = NULL;
static InitTr init_tr_from_content = NULL;
static GetTr get_tr = NULL;
static Download download = NULL;
static Downloaded downloaded = NULL;
static Downloaded code = NULL;
static CleanupTr reset_code = NULL;
static GetJSON getJSON = NULL;
static GetString get_string = NULL;
static GetTrFile get_tr_file = NULL;
static CleanupTr cleanup_manifest = NULL;

void dhlrc_mcdata_enable(GModule* module)
{
    if (module && g_module_symbol(module, "init_translation_from_file", (gpointer*)&init_tr)
        && g_module_symbol(module, "init_translation_from_content", (gpointer*)&init_tr_from_content)
        && g_module_symbol(module, "get_translation", (gpointer*)&get_tr)
        && g_module_symbol(module, "download_manifest", (gpointer*)&download)
        && g_module_symbol(module, "manifest_downloaded", (gpointer*)&downloaded)
        && g_module_symbol(module, "get_manifest", (gpointer*)&getJSON)
        && g_module_symbol(module, "manifest_download_code", (gpointer*)&code)
        && g_module_symbol(module, "manifest_reset_code", (gpointer*)&reset_code)
        && g_module_symbol(module, "get_version_json_string", (gpointer*)&get_string)
        && g_module_symbol(module, "get_translation_file", (gpointer*)&get_tr_file)
        && g_module_symbol(module, "cleanup_manifest", (gpointer*)&cleanup_manifest))
        enabled = TRUE;
}

gboolean dhlrc_mcdata_enabled ()
{
    return enabled;
}

int
dhlrc_init_translation_from_file (const char *filename,
                                  const char *large_version)
{
    if (init_tr)
        return init_tr (filename, large_version);
    return FALSE;
}

int
dhlrc_init_translation_from_content (const char *content,
                                     const char *large_version)
{
    if (init_tr_from_content)
        init_tr_from_content (content, large_version);
    return FALSE;
}

const char* dhlrc_get_translation(const char* name, const char* large_version)
{
    if (get_tr)
        return get_tr(name, large_version);
    return name;
}

int dhlrc_download_manifest(const char* dir, SigWithSet sig,
 SetFunc func, void* data, void* klass)
{
    if (download)
        return download(dir, sig, func, data, klass);
    return FALSE;
}

int dhlrc_manifest_downloaded()
{
    if (downloaded)
        return downloaded();
    return FALSE;
}

const cJSON* dhlrc_get_manifest()
{
    if (getJSON)
        return getJSON();
    return NULL;
}

int dhlrc_manifest_download_code()
{
    if (code)
        return code();
    return FALSE;
}

void dhlrc_manifest_reset_code()
{
    if (reset_code)
        reset_code();
}

char* dhlrc_get_version_json_string(const char* version, SetFunc set_func, void* klass, int min, int max)
{
    if (get_string)
        return get_string(version, set_func, klass, min, max);
    return NULL;
}

char* dhlrc_get_translation_file(const char* filename, const char* large_version, const char* lang)
{
    if (get_tr_file)
        return get_tr_file(filename, large_version, lang);
    return NULL;
}

void dhlrc_cleanup_manifest()
{
    if (cleanup_manifest)
        cleanup_manifest();
}