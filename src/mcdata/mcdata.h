/* This header just shows what functions we have
 * For usage, see mcdata_feature.h */

#ifndef MCDATA_H
#define MCDATA_H

#include <cjson/cJSON.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*Sig)(void*);

int init_translation_from_file(const char *filename);
int has_translation();
void cleanup_translation();
const char* get_translation(const char* name);
int download_manifest(const char* dir, Sig sig, void* data);
int download_manifest_sync(const char* dir);
int manifest_downloaded();
int manifest_download_code();
void manifest_reset_code();
const cJSON* get_manifest();
char* get_version_json_string(const char* version);

/* Some random code here:
 * bool success = false;
 * while(!success)
 * {
 *    download_manifest(dir);
 *    while(manifest_download_code() == -1); // Not finished
 *    if(manifest_download_code() == 0) // Or manifest_downloaded()
 *    { success = true; }
 * }
 */

#ifdef __cplusplus
}
#endif

#endif //MCDATA_H
