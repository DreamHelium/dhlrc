#include "unzip.h"

zip_t *
open_zip_file (const char *filename)
{
    zip_t *zip;
    int err;
    if ((zip = zip_open (filename, 0, &err)) == NULL)
        {
            zip_error_t error;
            zip_error_init_with_code (&error, err);
            g_debug ("Zip file open failed with message: %s",
                     zip_error_strerror (&error));
            zip_error_fini (&error);
        }
    return zip;
}

char *
get_file_content_in_zip (zip_t *zip, const char *filename, gsize *size)
{
    zip_stat_t st;
    zip_stat(zip, filename, 0, &st);
    *size = st.size;
    zip_file_t *file = zip_fopen(zip, filename, 0);
    if (file)
        {
            char *ret = g_new0 (char, *size);
            zip_fread (file, ret, *size);
            g_message("%s", ret);
            zip_fclose (file);
            return ret;
        }
    return NULL;
}

DhStrArray *
get_all_files_in_zip (zip_t *zip)
{
    gint64 len = zip_get_num_entries (zip, 0);
    DhStrArray *files = NULL;
    for (gint64 i = 0; i < len; i++)
        {
            const char *filename = zip_get_name (zip, i, 0);
            dh_str_array_add_str (&files, filename);
        }
    return files;
}

void
close_zip_file (zip_t *zip)
{
    zip_close (zip);
}