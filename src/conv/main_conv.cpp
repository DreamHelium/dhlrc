#include "main_conv.h"
#include "../litematica_region.h"
#include "../region.h"
#include "../translation.h"
#include "glib.h"

enum format
{
    DH_NBT,
    DH_LITEMATIC,
    DH_SCHEMATIC
};

static void
analyse (const char *input_file, const char *output_format, bool fast_mode)
{
    GPtrArray *region_array
        = g_ptr_array_new_with_free_func ((GDestroyNotify)region_free);
    auto if_format_val = -1;

    /* Load region */
    if (g_file_test (input_file, G_FILE_TEST_IS_REGULAR))
        {
            const char *if_format = strrchr (input_file, '.') + 1;

            if (g_str_equal (if_format, "nbt"))
                {
                    if_format_val = DH_NBT;
                    Region *region = region_new_from_nbt_file (input_file);
                    g_ptr_array_add (region_array, region);
                }
            else if (g_str_equal (if_format, "litematic"))
                {
                    if_format_val = DH_LITEMATIC;
                    DhNbtInstance instance (input_file);
                    int litematic_len
                        = lite_region_num_instance (&instance);

                    /* Make regions */
                    for (int i = 0; i < litematic_len; i++)
                        {
                            LiteRegion *lr
                                = lite_region_create_from_root_instance_cpp (
                                    instance, i);
                            Region *region = region_new_from_lite_region (lr);
                            lite_region_free (lr);
                            g_ptr_array_add (region_array, region);
                        }
                }
            else if (g_str_equal (if_format, "schem"))
                {
                    if_format_val = DH_SCHEMATIC;
                    DhNbtInstance instance (input_file);
                    Region *region = region_new_from_new_schem (&instance);
                    g_ptr_array_add (region_array, region);
                }
            else
                {
                    fprintf (stderr, _ ("Not a valid structure file!\n"));
                    goto free_array_return;
                }
        }
    else
        {
            fprintf (stderr, _ ("Not a valid file!\n"));
            goto free_array_return;
        }
    if (region_array->len == 0)
        goto free_array_return;

    if (g_str_equal (output_format, "nbt"))
        {
            if (if_format_val == DH_NBT)
                goto same_format_return;
            for (int i = 0; i < region_array->len; i++)
                {
                    Region *region = (Region *)(region_array->pdata[i]);

                    /* Get filename */
                    char *output_filename = g_path_get_basename (input_file);
                    *strrchr (output_filename, '.') = 0;
                    if (region_array->len != 1)
                        {
                            char *new_of_name = g_strconcat (
                                output_filename, "_", region->data->name,
                                ".nbt", NULL);
                            g_free (output_filename);
                            output_filename = new_of_name;
                        }
                    else
                        {
                            char *new_of_name
                                = g_strconcat (output_filename, ".nbt", NULL);
                            g_free (output_filename);
                            output_filename = new_of_name;
                        }

                    /* Transform Region to NBT */
                    printf (_ ("Saving file: %s.\n"), output_filename);
                    if (fast_mode)
                        fprintf (stderr, _ ("Fast mode is invalid!"));
                    auto instance
                        = (DhNbtInstance *)nbt_instance_ptr_new_from_region (
                            region, true);
                    instance->save_to_file (output_filename);
                    instance->self_free ();
                    delete instance;

                    g_free (output_filename);
                }
            goto free_array_return;
        }
    else if (g_str_equal (output_format, "litematic"))
        {
            if (if_format_val == DH_LITEMATIC)
                goto same_format_return;
            for (int i = 0; i < region_array->len; i++)
                {
                    Region *region = (Region *)(region_array->pdata[i]);

                    /* Get filename */
                    char *output_filename = g_path_get_basename (input_file);
                    *strrchr (output_filename, '.') = 0;
                    if (region_array->len != 1)
                        {
                            fprintf (stderr,
                                     _ ("Multiple file convertion to "
                                        "litematic is not supported yet!\n"));
                            g_free (output_filename);
                            goto free_array_return;
                        }
                    else
                        {
                            char *new_of_name = g_strconcat (
                                output_filename, ".litematic", NULL);
                            g_free (output_filename);
                            output_filename = new_of_name;
                        }

                    /* Transform Region to NBT */
                    printf (_ ("Saving file: %s.\n"), output_filename);
                    if (fast_mode)
                        fprintf (stderr, _ ("Fast mode is invalid in "
                                            "of-litematic mode.\n"));
                    auto instance
                        = (DhNbtInstance *)lite_instance_ptr_new_from_region (
                            region, true);
                    instance->save_to_file (output_filename);
                    instance->self_free ();
                    delete instance;

                    g_free (output_filename);
                }
            goto free_array_return;
        }
    else if (g_str_equal (output_format, "schematic"))
        {
            if (if_format_val == DH_SCHEMATIC)
                goto same_format_return;
            for (int i = 0; i < region_array->len; i++)
                {
                    auto region
                        = static_cast<Region *> (region_array->pdata[i]);

                    /* Get filename */
                    char *output_filename = g_path_get_basename (input_file);
                    *strrchr (output_filename, '.') = 0;
                    if (region_array->len != 1)
                        {
                            char *new_of_name = g_strconcat (
                                output_filename, "_", region->data->name,
                                ".schem", NULL);
                            g_free (output_filename);
                            output_filename = new_of_name;
                        }
                    else
                        {
                            char *new_of_name = g_strconcat (output_filename,
                                                             ".schem", NULL);
                            g_free (output_filename);
                            output_filename = new_of_name;
                        }

                    /* Transform Region to NBT */
                    printf (_ ("Saving file: %s.\n"), output_filename);
                    if (fast_mode)
                        fprintf (stderr, _ ("Fast mode is invalid!"));
                    auto instance = static_cast<DhNbtInstance *> (
                        new_schema_instance_ptr_new_from_region (region,
                                                                 true));
                    instance->save_to_file (output_filename);
                    instance->self_free ();
                    delete instance;

                    g_free (output_filename);
                }
            goto free_array_return;
        }
    else
        fprintf (stderr, _ ("Unrecognized format!\n"));

free_array_return:
    g_ptr_array_free (region_array, TRUE);
    return;

same_format_return:
    fprintf (stderr, _ ("The format of input file is the same as the output "
                        "file, which is invalid!\n"));
    goto free_array_return;
}

extern int
start_point (int argc, char **argv, const char *prpath)
{
    if (argc == 2
        && (g_str_equal (argv[1], "--help") || g_str_equal (argv[1], "-h")))
        {
            printf (_ ("This is a simple but stupid converter, it can only \n"
                       "receive following format:\n"));
            printf ("\n");
            printf (_ ("[input file] -of [format]\n"));

            printf ("\n");
            printf (_ ("For example: Station.litematic -of nbt\n"));

            printf ("\n");
            printf (_ ("format support: nbt, litematic, schematic\n"));
        }
    else if (argc == 4 && g_str_equal (argv[2], "-of"))
        {
            analyse (argv[1], argv[3], false);
        }
    else if (argc == 5 && g_str_equal (argv[2], "-of")
             && g_str_equal (argv[4], "--fast"))
        {
            analyse (argv[1], argv[3], true);
        }
    else
        printf (_ ("Unrecognized arguments, see \"--help\" for help.\n"));
    return 0;
}
