#include "mcdata.h"
#include "../config.h"
#include "../download_file.h"
#include "../json_util.h"
#include <dh_file_util.h>
#include <dh_string_util.h>
#include <future>
#include <iostream>
#include <vector>

extern "C"
{
typedef struct TranslationGroup
{
    std::string large_version;

    std::vector<std::string> item_name;
    std::vector<std::string> item_value;

    std::vector<std::string> block_name;
    std::vector<std::string> block_value;
} TranslationGroup;

static std::vector<TranslationGroup> translation_groups;

static gboolean manifest_download = FALSE;
static cJSON* manifest_json = NULL;
static gchar* manifest_dir = NULL;
static int ret_code = -1;

int init_translation_from_file(const char *filename, const char* large_version)
{
    cJSON* root = nullptr;
    cJSON* real_root = nullptr;
    if ((root = dhlrc_file_to_json (filename)) != nullptr)
        {
            real_root = root;
            TranslationGroup translation_group;
            translation_group.large_version = large_version;
            std::vector<std::string> block_name;
            std::vector<std::string> block_value;
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
                            block_name.emplace_back(block_name_p);
                            block_value.emplace_back(cJSON_GetStringValue(root));
                            g_free(block);
                        }
                }
            translation_group.block_name = block_name;
            translation_group.block_value = block_value;
            for (; !g_str_has_prefix(root->string, "item"); root = root->next);
            std::vector<std::string> item_name;
            std::vector<std::string> item_value;
            for (; g_str_has_prefix(root->string, "item"); root = root->next)
                {
                    char* item = g_strdup(root->string);
                    char* item_name_p = item;
                    for (;*item_name_p != '.'; item_name_p++);
                    item_name_p = item_name_p + 1;
                    if (strstr(item_name_p, "."))
                        {
                            *strstr(item_name_p, ".") = ':';
                            item_name.emplace_back(item_name_p);
                            item_value.emplace_back(cJSON_GetStringValue(root));
                            g_free(item);
                        }
                }
            translation_group.item_name = item_name;
            translation_group.item_value = item_value;
            translation_groups.emplace_back(translation_group);
            cJSON_Delete(real_root);
            return TRUE;
        }
    return 0;
}

const char* get_translation(const char* name, const char* large_version)
{
    for (const auto& group : translation_groups)
        {
            if (group.large_version == large_version)
                {
                    for (int i = 0 ; i < group.item_name.size(); i++)
                        {
                            if (group.item_name[i] == name)
                                return group.item_value[i].c_str();
                        }
                    for (int i = 0 ; i < group.block_name.size(); i++)
                        {
                            if (group.block_name[i] == name)
                                return group.block_value[i].c_str();
                        }
                }
        }
    return nullptr;
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
            if (sig_data->sig)
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
    int ret = dh_file_download_full_arg(
        "https://launchermeta.mojang.com/mc/game/version_manifest.json",
        dir, dh_file_progress_callback, (void*)("Version Manifest"), TRUE);
    if (ret == 0)
        {
            gchar* json_dir = g_build_filename(dir, "version_manifest.json", NULL);
            manifest_json = dhlrc_file_to_json(json_dir);
            g_free(json_dir);
        }
    return ret;
}

int manifest_downloaded()
{
    return manifest_download;
}

const cJSON* get_manifest()
{
    return manifest_json;
}

static const char* get_version_url(const char* version)
{
    cJSON* versions = cJSON_GetObjectItem(manifest_json, "versions");
    versions = versions->child;
    do
        {
            char* tmp_version = cJSON_GetStringValue(cJSON_GetObjectItem(versions, "id"));
            if (g_str_equal(tmp_version, version))
                    return cJSON_GetStringValue(cJSON_GetObjectItem(versions, "url"));
            versions = versions->next;
        }
    while (versions);
    return nullptr;
}

char* get_version_json_string(const char* version, SetFunc set_func, void* klass, int min, int max)
{
    gboolean first = TRUE;
    /* First get manifest_json */
    int staging = (max - min) / 2;
    gchar* cache_dir = dh_get_cache_dir();
    gchar* output = nullptr;
    if (!manifest_json)
        {
            first = FALSE;
            std::future<int> success = std::async(std::launch::deferred, download_manifest_sync, cache_dir);
            success.wait ();
            if (success.get())
                {
                    std::cerr << "download manifest failed!" << '\n';
                    goto fail_return; /* Failed to get manifest json */
                }
        }
    if (set_func && klass)
        set_func(klass, min + staging);
    if (auto version_url = get_version_url (version))
        {
            auto success = std::async(std::launch::deferred, dh_file_download_full_arg, version_url, cache_dir, dh_file_progress_callback, (void*)"Version JSON" ,true);
            success.wait ();
            if (success.get())
                {
                    std::cerr << "download version json failed!" << '\n';
                    goto fail_return;
                }
            /* Download version json success */
            std::string url_suffix = version;
            url_suffix += ".json";
            gchar* url_dir = g_build_filename(cache_dir, url_suffix.c_str(), NULL);
            cJSON* json = dhlrc_file_to_json (url_dir);
            g_free(url_dir);

            output = cJSON_GetStringValue(cJSON_GetObjectItem(
                cJSON_GetObjectItem (json, "assetIndex"), "id"));
            output = g_strdup(output);
            cJSON_Delete(json);
        }
    else goto fail_return;
    if (set_func && klass)
        set_func(klass, max);
    return output;

    fail_return:
    g_free(cache_dir);
    return nullptr;
}

}