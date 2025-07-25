#include "unzip_feature.h"

typedef void *(*OpenFile) (const char *);
typedef void (*CloseFile) (void *);
typedef char *(*GetFileContent) (void *, const char *, gsize *);
typedef DhStrArray *(*GetAllFilesInZip) (void *);

static OpenFile open_file = NULL;
static CloseFile close_file = NULL;
static GetFileContent get_file_content = NULL;
static GetAllFilesInZip get_all_files_in_zip = NULL;

static gboolean enabled = FALSE;

void
dhlrc_unzip_enable (GModule *module)
{
    if (module
        && g_module_symbol (module, "open_zip_file", (gpointer *)&open_file)
        && g_module_symbol (module, "get_file_content_in_zip",
                            (gpointer *)&get_file_content)
        && g_module_symbol (module, "get_all_files_in_zip",
                            (gpointer *)&get_all_files_in_zip)
        && g_module_symbol (module, "close_zip_file", (gpointer *)&close_file))
        enabled = TRUE;
}

gboolean
dhlrc_unzip_enabled ()
{
    return enabled;
}

void *
dhlrc_open_zip_file (const char *filename)
{
    if (open_file)
        return open_file (filename);
    return NULL;
}

void
dhlrc_close_zip_file (void *zip)
{
    if (close_file)
        close_file (zip);
}
char *
dhlrc_get_file_content_in_zip (void *zip, const char *filename, gsize *size)
{
    if (get_file_content)
        return get_file_content (zip, filename, size);
    return NULL;
}

DhStrArray *
dhlrc_get_all_files_in_zip (void *zip)
{
    if (get_all_files_in_zip)
        return get_all_files_in_zip (zip);
    return NULL;
}
