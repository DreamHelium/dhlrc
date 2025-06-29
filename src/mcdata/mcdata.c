#include "mcdata.h"
#include "../json_util.h"
#include "../download_file.h"
#include "../config.h"
#include <dh_file_util.h>
#include <dh_string_util.h>

static DhStrArray* item_name = NULL;
static DhStrArray* item_value = NULL;

static DhStrArray* block_name = NULL;
static DhStrArray* block_value = NULL;

static gboolean is_initialized = FALSE;

static gboolean manifest_download = FALSE;
static cJSON* manifest_json = NULL;
static gchar* manifest_dir = NULL;
static int ret_code = -1;

int init_translation_from_file(const char *filename)
{
    cJSON* root = NULL;
    cJSON* real_root = NULL;
    if ((root = dhlrc_file_to_json (filename)) != NULL)
        {
            real_root = root;
            for (root = root->child; !g_str_has_prefix(root->string, "block"); root = root->next);
            for (; g_str_has_prefix(root->string, "block"); root = root->next)
                {
                    char* block = g_strdup(root->string);
                    char* block_name_p = block;
                    for (;*block_name_p != '.'; block_name_p++);
                    block_name_p = block_name_p + 1;
                    if (strstr(block_name_p, "."))
                        {
                            *strstr(block_name_p, ".") = ':';
                            dh_str_array_add_str(&block_name, block_name_p);
                            dh_str_array_add_str(&block_value, cJSON_GetStringValue(root));
                            g_free(block);
                        }
                }
            for (; !g_str_has_prefix(root->string, "item"); root = root->next);
            for (; g_str_has_prefix(root->string, "item"); root = root->next)
                {
                    char* item = g_strdup(root->string);
                    char* item_name_p = item;
                    for (;*item_name_p != '.'; item_name_p++);
                    item_name_p = item_name_p + 1;
                    if (strstr(item_name_p, "."))
                        {
                            *strstr(item_name_p, ".") = ':';
                            dh_str_array_add_str(&item_name, item_name_p);
                            dh_str_array_add_str(&item_value, cJSON_GetStringValue(root));
                            g_free(item);
                        }
                }
            cJSON_Delete(real_root);
            is_initialized = TRUE;
            return TRUE;
        }
    return 0;
}

void cleanup_translation()
{
    dh_str_array_free (item_name);
    dh_str_array_free (block_name);
    dh_str_array_free (item_value);
    dh_str_array_free (block_value);
    is_initialized = FALSE;
}

const char* get_translation(const char* name)
{
    int val = 0;
    if ((val = dh_str_array_find(item_name, name)) != -1)
        return item_value->val[val];
    if ((val = dh_str_array_find(block_name, name)) != -1)
        return block_value->val[val];
    return NULL;
}

int has_translation()
{
    return is_initialized;
}

typedef struct SigWithData
{
    SigWithSet sig;
    SetFunc func;
    void* data;
    void* klass;
} SigWithData;

static void
finish_callback (GObject *source_object, GAsyncResult *res, gpointer data)
{
    int ret = g_task_propagate_int (G_TASK (res), NULL);
    ret_code = ret;
    SigWithData* sig_data = (SigWithData*)data;
    if (ret != 0)
        {
            g_message("Download failed with code %d", ret);
            manifest_download = FALSE;
            g_free(manifest_dir);
            if (manifest_json) cJSON_Delete(manifest_json);
            g_free(sig_data);
        }
    else
        {
            gchar* dir = g_build_path(G_DIR_SEPARATOR_S, manifest_dir, "version_manifest.json", NULL);
            manifest_download = TRUE;
            g_free(manifest_dir);
            if (manifest_json) cJSON_Delete(manifest_json);
            manifest_json = dhlrc_file_to_json (dir);
            g_free(dir);
            sig_data->sig(sig_data->data, sig_data->func, sig_data->klass);
            g_free(sig_data);
        }
}

int manifest_download_code()
{
    return ret_code;
}

void manifest_reset_code()
{
    ret_code = -1;
}

int download_manifest(const char* dir, SigWithSet sig, SetFunc func, void* data, void* klass)
{
    manifest_reset_code();
    if (dh_file_is_directory(dir))
        {
            manifest_dir = g_strdup(dir);
            SigWithData* sig_with_data = g_new0(SigWithData, 1);
            sig_with_data->sig = sig;
            sig_with_data->func = func;
            sig_with_data->data = data;
            sig_with_data->klass = klass;
            dh_file_download_async (
                "https://launchermeta.mojang.com/mc/game/version_manifest.json",
                dir, dh_file_progress_callback, NULL, TRUE, finish_callback, sig_with_data);
            return TRUE;
        }
    return FALSE;
}

int download_manifest_sync(const char* dir)
{
    return dh_file_download_full_arg(
        "https://launchermeta.mojang.com/mc/game/version_manifest.json",
        dir, dh_file_progress_callback, NULL, TRUE);
}

int manifest_downloaded()
{
    return manifest_download;
}

const cJSON* get_manifest()
{
    return manifest_json;
}

static void wait(GTask* task, gpointer source_object, gpointer task_data, GCancellable* cancellable)
{
    g_usleep(1 * G_USEC_PER_SEC);
}

static GMainLoop* loop = NULL;
static char* output = NULL;

static void get_output(void* data, SetFunc func, void* klass)
{
    if (func && klass)
        func(klass, 50);
    const char* version = data;
    /* Second get version json */
    cJSON* versions = cJSON_GetObjectItem(manifest_json, "versions");
    versions = versions->child;
    do
        {
            char* tmp_version = cJSON_GetStringValue(cJSON_GetObjectItem(versions, "id"));
            if (g_str_equal(tmp_version, version))
                {
                    gchar* cache_dir = dh_get_cache_dir();
                    char* url = cJSON_GetStringValue(cJSON_GetObjectItem(versions, "url"));
                    dh_file_download_full_arg(url, cache_dir, dh_file_progress_callback, NULL, TRUE);
                    gchar* url_suffix = g_strconcat(version, ".json", NULL);
                    gchar* url_dir = g_build_path(G_DIR_SEPARATOR_S, cache_dir, url_suffix, NULL);
                    g_free(cache_dir);
                    g_free(url_suffix);
                    cJSON* json = dhlrc_file_to_json(url_dir);
                    g_free(url_dir);

                    output = cJSON_GetStringValue(cJSON_GetObjectItem(
                        cJSON_GetObjectItem (json, "assetIndex"), "id"));
                    output = g_strdup(output);
                    cJSON_Delete(json);
                    break;
                }
            versions = versions->next;
        }
    while (versions);

    g_main_loop_quit(loop);
}

char* get_version_json_string(const char* version, SetFunc set_func, void* klass)
{
    gboolean first = TRUE;
    /* First get manifest_json */
    if (!manifest_json)
        {
            first = FALSE;
            gchar* cache_dir = dh_get_cache_dir();
            download_manifest(cache_dir, get_output, set_func,(void*)version, klass);

            loop = g_main_loop_new (NULL, FALSE);
            g_main_loop_run (loop);
            g_main_loop_unref (loop);
            if (set_func && klass)
                set_func(klass, 100);
        }
    if (manifest_json && first)
        {
            get_output((void*)version, set_func, klass);
            if (set_func && klass)
                set_func(klass, 100);
        }


    return output;
}