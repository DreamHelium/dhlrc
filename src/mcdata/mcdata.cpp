#include "mcdata.h"
#include "../config.h"
#include "../download_file.h"
#include "../json_util.h"
#include "../csv_parser.h"
#include <dh_file_util.h>
#include <dh_string_util.h>
#include <future>
#include <iostream>
#include <map>
#include <vector>

extern "C"
{
    typedef struct TranslationGroup
    {
        std::string large_version;

        std::vector<std::string> filenames;
        std::map<std::string, std::string> translations;
    } TranslationGroup;

#define RESOURCES_URL "https://resources.download.minecraft.net/"
#define MANIFEST_URL                                                          \
    "https://launchermeta.mojang.com/mc/game/version_manifest.json"

    static std::vector<TranslationGroup> translation_groups;

    static gboolean manifest_download = FALSE;
    static cJSON *manifest_json = nullptr;
    static gchar *manifest_dir = nullptr;
    static int ret_code = -1;
    static std::map<int, std::vector<std::string>> jar_files;
    static std::map<int, std::string> version_map;
    static bool map_inited = false;

    static int
    download_object (const char *hash, const char *gamedir)
    {
        char hash_before[3] = { hash[0], hash[1], 0 };
        char *url = g_strconcat (RESOURCES_URL, hash_before, "/", hash, NULL);
        char *dir = g_build_filename (gamedir, "assets", "objects",
                                      hash_before, NULL);
        int ret = dh_file_download_full_arg (
            url, dir, dh_file_progress_callback, (void *)("Object"), TRUE);
        g_free (url);
        g_free (dir);
        return ret;
    }

    static bool
    is_repeated (const std::string &str,
                 const std::map<std::string, std::string> &translations)
    {
        auto it = translations.find (str);
        if (it == translations.end ())
            return false;
        return true;
    }

    static void
    cache_translation_from_content (
        const char *content, std::map<std::string, std::string> &translations)
    {
        cJSON *json = cJSON_Parse (content);
        if (json)
            {
                cJSON *real_json = json->child;
                for (; !g_str_has_prefix (real_json->string, "block");
                     real_json = real_json->next)
                    ;
                for (; g_str_has_prefix (real_json->string, "block");
                     real_json = real_json->next)
                    {
                        char *block = g_strdup (real_json->string);
                        char *block_name_p = block;
                        for (; *block_name_p != '.'; block_name_p++)
                            ;
                        block_name_p = block_name_p + 1;
                        if (strstr (block_name_p, "."))
                            {
                                *strstr (block_name_p, ".") = ':';
                                std::string name = block_name_p;
                                std::string value
                                    = cJSON_GetStringValue (real_json);
                                if (!is_repeated (name, translations))
                                    translations.insert (
                                        std::pair (name, value));
                                g_free (block);
                            }
                    }
                for (; !g_str_has_prefix (real_json->string, "item");
                     real_json = real_json->next)
                    ;
                for (; g_str_has_prefix (real_json->string, "item");
                     real_json = real_json->next)
                    {
                        char *item = g_strdup (real_json->string);
                        char *item_name_p = item;
                        for (; *item_name_p != '.'; item_name_p++)
                            ;
                        item_name_p = item_name_p + 1;
                        if (strstr (item_name_p, "."))
                            {
                                *strstr (item_name_p, ".") = ':';
                                std::string name = item_name_p;
                                std::string value
                                    = cJSON_GetStringValue (real_json);
                                if (!is_repeated (name, translations))
                                    translations.insert (
                                        std::pair (name, value));
                                g_free (item);
                            }
                    }

                cJSON_Delete (json);
            }
        else
            /* It's an old file format */
            g_message ("The file is too old! couldn't get translation!");
    }

    static void
    cache_translation (const char *filename,
                       std::map<std::string, std::string> &translations)
    {
        gchar *content = nullptr;
        gsize len = 0;
        if (g_file_get_contents (filename, &content, &len, nullptr))
            {
                cache_translation_from_content (content, translations);
                g_free (content);
            }
    }

    int
    init_translation_from_file (const char *filename,
                                const char *large_version)
    {
        if (g_file_test (filename, G_FILE_TEST_IS_REGULAR))
            {
                /* Searching for group */
                for (auto group : translation_groups)
                    {
                        if (group.large_version == large_version)
                            {
                                for (const auto &group_filename :
                                     group.filenames)
                                    {
                                        if (group_filename == filename)
                                            return FALSE;
                                    }
                                group.filenames.emplace_back (filename);
                                cache_translation (filename,
                                                   group.translations);
                                return TRUE;
                            }
                    }
                TranslationGroup group;
                group.large_version = large_version;
                group.filenames.emplace_back (filename);
                cache_translation (filename, group.translations);

                translation_groups.emplace_back (group);

                return TRUE;
            }
        return 0;
    }

    int
    init_translation_from_content (const char *content,
                                   const char *large_version)
    {
        /* Searching for group */
        for (auto group : translation_groups)
            {
                if (group.large_version == large_version)
                    {
                        cache_translation_from_content (content,
                                                        group.translations);
                        return TRUE;
                    }
            }
        TranslationGroup group;
        group.large_version = large_version;
        cache_translation_from_content (content, group.translations);

        translation_groups.emplace_back (group);

        return TRUE;
    }

    const char *
    get_translation (const char *name, const char *large_version)
    {
        for (const auto &group : translation_groups)
            {
                if (group.large_version == large_version)
                    {
                        const char *ret = name;
                        auto it = group.translations.find (name);
                        if (it != group.translations.end ())
                            ret = it->second.c_str ();
                        return ret;
                    }
            }
        return name;
    }

    int
    manifest_download_code ()
    {
        return ret_code;
    }

    void
    manifest_reset_code ()
    {
        ret_code = -1;
    }

    int
    download_manifest_sync (const char *dir)
    {
        int ret = dh_file_download_full_arg (
            MANIFEST_URL, dir, dh_file_progress_callback,
            (void *)("Version Manifest"), TRUE);
        if (ret == 0)
            {
                gchar *json_dir
                    = g_build_filename (dir, "version_manifest.json", NULL);
                manifest_json = dhlrc_file_to_json (json_dir);
                g_free (json_dir);
            }
        return ret;
    }

    int
    manifest_downloaded ()
    {
        return manifest_download;
    }

    const cJSON *
    get_manifest ()
    {
        return manifest_json;
    }

    static char *
    get_index_dir (const char *gamedir)
    {
        return g_build_filename (gamedir, "assets", "indexes", NULL);
    }

    static const char *
    get_version_url (const char *version)
    {
        cJSON *versions = cJSON_GetObjectItem (manifest_json, "versions");
        versions = versions->child;
        do
            {
                char *tmp_version = cJSON_GetStringValue (
                    cJSON_GetObjectItem (versions, "id"));
                if (g_str_equal (tmp_version, version))
                    return cJSON_GetStringValue (
                        cJSON_GetObjectItem (versions, "url"));
                versions = versions->next;
            }
        while (versions);
        return nullptr;
    }

    /* Considered that 2 files are downloaded in this function,
     * We might download the version json. */
    char *
    get_version_json_string (const char *version, SetFunc set_func,
                             void *klass, int min, int max)
    {
        gboolean first = TRUE;
        bool too_old = false;
        /* First get manifest_json */
        int staging = (max - min) / 3;
        gchar *cache_dir = dh_get_cache_dir ();
        gchar *output = nullptr;
        if (!manifest_json)
            {
                first = FALSE;
                if (download_manifest_sync (cache_dir))
                    {
                        std::cerr << "download manifest failed!" << '\n';
                        goto fail_return; /* Failed to get manifest json */
                    }
            }
        if (set_func && klass)
            set_func (klass, min + staging);
        if (auto version_url = get_version_url (version))
            {
                if (dh_file_download_full_arg (version_url, cache_dir,
                                               dh_file_progress_callback,
                                               (void *)"Version JSON", true))
                    {
                        std::cerr << "download version json failed!" << '\n';
                        goto fail_return;
                    }
                /* Download version json success */
                std::string url_suffix = version;
                url_suffix += ".json";
                gchar *url_dir
                    = g_build_filename (cache_dir, url_suffix.c_str (), NULL);
                cJSON *json = dhlrc_file_to_json (url_dir);
                g_free (url_dir);

                output = cJSON_GetStringValue (cJSON_GetObjectItem (
                    cJSON_GetObjectItem (json, "assetIndex"), "id"));
                if (!g_str_equal (output, "legacy")
                    || !g_str_equal (output, "1.12"))
                    output = g_strdup (output);
                else
                    {
                        cJSON_Delete (json);
                        too_old = true;
                        g_message ("The file is too old! Fallback to 1.14!");
                        goto too_old;
                    }

                auto json_url = cJSON_GetStringValue (cJSON_GetObjectItem (
                    cJSON_GetObjectItem (json, "assetIndex"), "url"));

                if (set_func && klass)
                    set_func (klass, max - staging);

                std::string filename = output;
                filename += ".json";
                auto gamedir = dh_get_game_dir ();
                auto dir = get_index_dir (gamedir);
                g_free (gamedir);
                auto filedir = g_build_filename (dir, filename.c_str (), NULL);

                /* The json file is not exist. */
                if (!g_file_test (filedir, G_FILE_TEST_IS_REGULAR))
                    {
                        if (dh_file_download_full_arg (
                                json_url, dir, dh_file_progress_callback,
                                (void *)("Large Version JSON"), TRUE))
                            {
                                std::cerr
                                    << "download large version json failed!"
                                    << '\n';
                                cJSON_Delete (json);
                                g_free (filedir);
                                g_free (dir);
                                goto fail_return;
                            }
                    }
                g_free (filedir);
                g_free (dir);
                cJSON_Delete (json);
            }
        else
            goto fail_return;
        if (set_func && klass)
            set_func (klass, max);
        return output;

    fail_return:
        g_free (cache_dir);
        return nullptr;
    too_old:
        g_free (cache_dir);
        return get_version_json_string ("1.14", set_func, klass, min, staging);
    }

    char *
    get_object_hash (const char *gamedir, const char *large_version,
                     const char *object)
    {
        char *ret = nullptr;
        char *index_file = nullptr;
        if (gamedir && large_version && object
            && g_file_test (gamedir, G_FILE_TEST_IS_DIR))
            {
                std::string filename = large_version;
                filename += ".json";
                char *index_dir = get_index_dir (gamedir);
                index_file
                    = g_build_filename (index_dir, filename.c_str (), NULL);
                g_free (index_dir);
            }
        if (g_file_test (index_file, G_FILE_TEST_IS_REGULAR))
            {
                cJSON *index = dhlrc_file_to_json (index_file);
                g_free (index_file);
                cJSON *objects = cJSON_GetObjectItem (index, "objects");
                cJSON *object_json = cJSON_GetObjectItem (objects, object);
                char *hash = cJSON_GetStringValue (
                    cJSON_GetObjectItem (object_json, "hash"));
                ret = g_strdup (hash);
                cJSON_Delete (index);
            }
        return ret;
    }

    char *
    get_object_dir (const char *gamedir, const char *hash)
    {
        if (gamedir && hash)
            {
                char hash_before[3] = { hash[0], hash[1], 0 };
                char *dir = g_build_filename (gamedir, "assets", "objects",
                                              hash_before, hash, NULL);
                return dir;
            }
        return nullptr;
    }

    static char *
    get_pure_lang (const char *lang)
    {
        char *lang_dup = lang ? g_strdup (lang) : g_strdup ("zh_cn");
        for (int i = 0; i < strlen (lang_dup); i++)
            {
                lang_dup[i] = g_ascii_tolower (lang_dup[i]);
            }
        char *point_pos = NULL;
        if ((point_pos = strchr (lang_dup, '.')) != NULL)
            *point_pos = 0;
        return lang_dup;
    }

    char *
    get_translation_file (const char *gamedir, const char *large_version,
                          const char *lang)
    {
        char *lang_dup = get_pure_lang (lang);
        char *object
            = g_strconcat ("minecraft/lang/", lang_dup, ".json", NULL);
        char *hash = get_object_hash (gamedir, large_version, object);
        char *dir = nullptr;
        /* hash maybe not exist. */
        if (hash)
            dir = get_object_dir (gamedir, hash);
        else
            {
                g_free (lang_dup);
                g_free (object);
                return nullptr;
            }
        if (dir && g_file_test (dir, G_FILE_TEST_IS_REGULAR))
            {
                g_free (lang_dup);
                g_free (object);
                g_free (hash);
                return dir;
            }
        /* Download translation file */
        if (download_object (hash, gamedir))
            {
                g_free (dir);
                dir = nullptr;
            }
        g_free (lang_dup);
        g_free (object);
        g_free (hash);
        return dir;
    }

    void
    cleanup_manifest ()
    {
        cJSON_Delete (manifest_json);
    }

    void
    load_jar (const char *filename, int data_version)
    {
        auto it = jar_files.find (data_version);
        if (it == jar_files.end ())
            {
                std::string jar_filename = filename;
                std::vector files = { jar_filename };
                jar_files.insert (std::make_pair (data_version, files));
            }
        else
            {
                auto files = it->second;
                files.emplace_back (filename);
            }
    }

    void
    load_version_map ()
    {
        auto datas
            = g_resources_lookup_data ("/cn/dh/dhlrc/data_version.csv",
                                       G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr);
        auto data
            = static_cast<const char *> (g_bytes_get_data (datas, nullptr));
        auto array = dh_csv_parse (data);
        for (int i = 0; i < array->len; i++)
            {
                auto val = (char **)array->pdata[i];
                std::string version = val[0];
                int dataversion = std::stoi (val[1]);
                version_map.insert (std::make_pair (dataversion, version));
            }
        g_bytes_unref (datas);
        g_ptr_array_free (array, true);
        map_inited = true;
    }

    gboolean
    version_map_inited ()
    {
        return map_inited;
    }

    void* get_version_map()
    {
        return &version_map;
    }
}
