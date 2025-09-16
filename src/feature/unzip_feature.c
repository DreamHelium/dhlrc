#include "unzip_feature.h"

#include "dh_module.h"

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
dhlrc_unzip_enable ()
{
    DhModule* module = dh_search_inited_module ("unzip");
    if (module)
        {
            open_file = module->module_functions->pdata[0];
            get_file_content = module->module_functions->pdata[1];
            get_all_files_in_zip = module->module_functions->pdata[2];
            close_file = module->module_functions->pdata[3];
            enabled = TRUE;
        }
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
