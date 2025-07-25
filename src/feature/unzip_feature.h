#ifndef UNZIP_FEATURE_H
#define UNZIP_FEATURE_H

#include <dh_string_util.h>
#include <glib.h>
#include <gmodule.h>

G_BEGIN_DECLS

void dhlrc_unzip_enable(GModule* module);
gboolean dhlrc_unzip_enabled();
void* dhlrc_open_zip_file (const char *filename);
void dhlrc_close_zip_file (void *zip);
char* dhlrc_get_file_content_in_zip(void* zip, const char* filename, gsize *size);
DhStrArray* dhlrc_get_all_files_in_zip(void* zip);

G_END_DECLS

#endif //UNZIP_FEATURE_H
