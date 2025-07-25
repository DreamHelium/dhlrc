/* This header just shows what functions we have
 * For usage, see mcdata_feature.h */

#ifndef MCDATA_H
#define MCDATA_H

#include <cjson/cJSON.h>
#include <glib.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef void (*SetFunc) (void *, int);

    int init_translation_from_file (const char *filename,
                                    const char *large_version);
    int init_translation_from_content (const char *content,
                                       const char *large_version);
    const char *get_translation (const char *name, const char *large_version);
    int download_manifest_sync (const char *dir);
    int manifest_downloaded ();
    int manifest_download_code ();
    void manifest_reset_code ();
    const cJSON *get_manifest ();
    char *get_version_json_string (const char *version, SetFunc set_func,
                                   void *klass, int min, int max);
    char *get_object_hash (const char *gamedir, const char *large_version,
                           const char *object);
    char *get_object_dir (const char *gamedir, const char *hash);
    char *get_translation_file (const char *gamedir, const char *large_version,
                                const char *lang);
    void cleanup_manifest ();
    void load_jar(const char* filename, int data_version);
    void load_version_map();
    gboolean version_map_inited();
    void* get_version_map();

#ifdef __cplusplus
}
#endif

#endif // MCDATA_H
