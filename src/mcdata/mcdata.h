/* This header just shows what functions we have
 * For usage, see mcdata_feature.h */

#ifndef MCDATA_H
#define MCDATA_H

#include <cjson/cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*Sig)(void*);
typedef void (*SetFunc)(void*, int);
typedef void (*SigWithSet)(void*, SetFunc, void*);


int init_translation_from_file(const char *filename, const char* large_version);
const char* get_translation(const char* name, const char* large_version);
int download_manifest(const char* dir, SigWithSet sig,
 SetFunc func, void* data, void* klass);
int download_manifest_sync(const char* dir);
int manifest_downloaded();
int manifest_download_code();
void manifest_reset_code();
const cJSON* get_manifest();
char* get_version_json_string(const char* version, SetFunc set_func, void* klass, int min, int max);

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
