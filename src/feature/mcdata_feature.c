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

typedef int (*InitTr) (const char *, const char *);
typedef void (*CleanupTr) ();
typedef const char *(*GetTr) (const char *, const char *);
typedef int (*Downloaded) ();
typedef const cJSON *(*GetJSON) ();
typedef char *(*GetString) (const char *, SetFunc, void *, int, int);
typedef char *(*GetTrFile) (const char *, const char *, const char *);
typedef void (*LoadJar) (const char *, int);
typedef gboolean (*Inited) ();
typedef void *(*GetMap) ();

static gboolean enabled = FALSE;

static InitTr init_tr = NULL;
static InitTr init_tr_from_content = NULL;
static GetTr get_tr = NULL;
static Downloaded downloaded = NULL;
static GetJSON getJSON = NULL;
static GetString get_string = NULL;
static GetTrFile get_tr_file = NULL;
static CleanupTr cleanup_manifest = NULL;
static LoadJar load_jar = NULL;
static CleanupTr load_version_map = NULL;
static Inited inited = NULL;
static GetMap get_map = NULL;

void
dhlrc_mcdata_enable ()
{
    DhModule *module = dh_search_inited_module ("mcdata");
    if (module)
        {
            init_tr = module->module_functions->pdata[0];
            init_tr_from_content = module->module_functions->pdata[1];
            get_tr = module->module_functions->pdata[2];
            downloaded = module->module_functions->pdata[4];
            getJSON = module->module_functions->pdata[5];
            get_string = module->module_functions->pdata[6];
            get_tr_file = module->module_functions->pdata[9];
            cleanup_manifest = module->module_functions->pdata[10];
            load_jar = module->module_functions->pdata[11];
            load_version_map = module->module_functions->pdata[12];
            inited = module->module_functions->pdata[13];
            get_map = module->module_functions->pdata[14];
            enabled = TRUE;
        }
}

gboolean
dhlrc_mcdata_enabled ()
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

const char *
dhlrc_get_translation (const char *name, const char *large_version)
{
    if (get_tr)
        return get_tr (name, large_version);
    return name;
}

int
dhlrc_manifest_downloaded ()
{
    if (downloaded)
        return downloaded ();
    return FALSE;
}

const cJSON *
dhlrc_get_manifest ()
{
    if (getJSON)
        return getJSON ();
    return NULL;
}

char *
dhlrc_get_version_json_string (const char *version, SetFunc set_func,
                               void *klass, int min, int max)
{
    if (get_string)
        return get_string (version, set_func, klass, min, max);
    return NULL;
}

char *
dhlrc_get_translation_file (const char *filename, const char *large_version,
                            const char *lang)
{
    if (get_tr_file)
        return get_tr_file (filename, large_version, lang);
    return NULL;
}

void
dhlrc_cleanup_manifest ()
{
    if (cleanup_manifest)
        cleanup_manifest ();
}

void
dhlrc_load_jar (const char *filename, int data_version)
{
    if (load_jar)
        load_jar (filename, data_version);
}

void
dhlrc_load_version_map ()
{
    if (load_version_map)
        load_version_map ();
}

gboolean
dhlrc_version_map_inited ()
{
    if (inited)
        return inited ();
    return FALSE;
}

void *
dhlrc_get_version_map ()
{
    if (get_map)
        return get_map ();
    return NULL;
}