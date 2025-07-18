#ifndef UNZIP_H
#define UNZIP_H

#include "dh_string_util.h"
#include <glib.h>
#include <zip.h>

#ifdef __cplusplus
extern "C"
{
#endif

    zip_t *open_zip_file (const char *filename);
    char *get_file_content_in_zip (zip_t *zip, const char *filename, gsize *size);
    DhStrArray *get_all_files_in_zip (zip_t *zip);
    void close_zip_file (zip_t *zip);

#ifdef __cplusplus
}
#endif

#endif // UNZIP_H
