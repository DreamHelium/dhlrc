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

#include "common.h"
#include "common_info.h"
#include "dh_file_util.h"
#include "feature/recipe_feature.h"
#include "global_variant.h"
#include "iniparser.h"
#include "translation.h"

static GResource *res = NULL;
static gboolean res_load = FALSE;
static gboolean config_listened = FALSE;
static gboolean inited = FALSE;
static GFileMonitor *file_monitor = NULL;
static gchar *config_filepos = NULL;

gboolean
dhlrc_common_contents_init (const char *prname)
{
  char *res_path = NULL;
  char *prpath = dh_file_get_current_program_dir (prname);
#ifdef RESOURCEDIR
  res_path = g_strdup (RESOURCEDIR);
#else
  res_path = g_strconcat (prpath, G_DIR_SEPARATOR_S, "dhlrc_resources", NULL);
#endif
  GError *err = NULL;
  res = g_resource_load (res_path, &err);
  g_free (prpath);
  g_free (res_path);
  if (err)
    {
      g_message ("Load resource error with message: %s.", err->message);
      g_error_free (err);
      return FALSE;
    }
  else
    {
      g_resources_register (res);
      res_load = TRUE;
      return TRUE;
    }
}

static void
load_config ()
{
  dictionary *dictionary = iniparser_load (config_filepos);
  gboolean ignore_leftover
      = iniparser_getboolean (dictionary, "General:IgnoreLeftover", FALSE);
  dhlrc_set_ignore_leftover (ignore_leftover);

  iniparser_freedict (dictionary);
}

static void
listen_cb ()
{
  typedef void (*func) ();
  DhModule *module = dh_search_inited_module ("qt");
  if (module)
    {
      func fn = module->module_functions->pdata[1];
      fn ();
    }
  load_config ();
}

static void
watch_config_file ()
{
  config_filepos = g_build_path (G_DIR_SEPARATOR_S, g_get_user_config_dir (),
                                 "dhlrcrc", NULL);
  GFile *file = g_file_new_for_path (config_filepos);
  if (!g_file_query_exists (file, NULL))
    {
      GFileOutputStream *os
          = g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);
      g_object_unref (os);
    }
  file_monitor = g_file_monitor (file, G_FILE_MONITOR_NONE, NULL, NULL);
  g_object_unref (file);
  g_signal_connect (file_monitor, "changed", G_CALLBACK (listen_cb), NULL);
  config_listened = TRUE;
  load_config ();
}

gboolean
dhlrc_common_contents_is_inited ()
{
  return res_load;
}

gboolean
dhlrc_config_is_listened ()
{
  return config_listened;
}

GResource *
dhlrc_get_resource ()
{
  return res;
}

void
dhlrc_init (const char *prname)
{
  translation_init (prname);
  dhlrc_common_contents_init (prname);
  char *recipes_module_path = NULL;
#ifdef RECIPESDIR
  recipes_module_path = g_strdup (RECIPESDIR);
#else
  char *prpath = dh_file_get_current_program_dir (prname);
  recipes_module_path
      = g_build_path (G_DIR_SEPARATOR_S, prpath, "recipes_module", NULL);
  g_free (prpath);
#endif
  dhlrc_recipe_init (recipes_module_path);
  g_free (recipes_module_path);
  dhlrc_global_variant_init ();
  inited = TRUE;
  watch_config_file ();
}

gboolean
dhlrc_is_inited ()
{
  return inited;
}

void
dhlrc_cleanup ()
{
  dhlrc_common_contents_free ();
  dhlrc_recipe_module_clean ();
  dhlrc_cleanup_manifest ();
  dh_type_free ();
  dhlrc_global_variant_clear ();
  g_object_unref (file_monitor);
  g_free (config_filepos);
}

void
dhlrc_common_contents_free ()
{
  g_resource_unref (res);
}