#include "main_conv.h"
#include "../translation.h"
#include "../region.h"

enum format{DH_NBT, DH_LITEMATIC, DH_SCHEMATIC};

static void analyse(const char* input_file, const char* output_format)
{
    GPtrArray* region_array = g_ptr_array_new_with_free_func((GDestroyNotify)region_free);
    enum format if_format_val = -1;
    
    /* Load region */
    if(g_file_test(input_file, G_FILE_TEST_IS_REGULAR))
    {
        char* if_format = strrchr(input_file, '.') + 1;

        if(g_str_equal(if_format, "nbt"))
        {
            if_format_val = DH_NBT;
            Region* region = region_new_from_nbt_file(input_file);
            g_ptr_array_add(region_array, region);
        }
        else if(g_str_equal(if_format, "litematic"))
        {
            if_format_val = DH_LITEMATIC;
            NbtInstance* instance = dh_nbt_instance_parse(input_file);
            int litematic_len = lite_region_num_instance(instance);

            /* Make regions */
            for(int i = 0 ; i < litematic_len ; i++)
            {
                printf("test\n");
                LiteRegion* lr = lite_region_create_instance(instance, i);
                Region* region = region_new_from_lite_region(lr);
                lite_region_free(lr);
                g_ptr_array_add(region_array, region);
            }
            dh_nbt_instance_free(instance);
        }
        else if(g_str_equal(if_format, "schematic")) 
        {
            if_format_val = DH_SCHEMATIC;
            /* TODO */
        }
        else
        {
            fprintf(stderr, _("Not a valid structure file!\n"));
            goto free_array_return;
        }
    }
    else
    {
        fprintf(stderr, _("Not a valid file!\n"));
        goto free_array_return;
    }
    if(region_array->len == 0)
        goto free_array_return;

    if(g_str_equal(output_format, "nbt"))
    {
        if(if_format_val == DH_NBT)
            goto same_format_return;
        for(int i = 0 ; i < region_array->len ; i++)
        {
            Region* region = region_array->pdata[i];

            /* Get filename */
            char* output_filename = g_path_get_basename(input_file);
            *strrchr(output_filename, '.') = 0;
            if(region_array->len != 1)
            {
                char* new_of_name = g_strconcat(output_filename, "_", region->region_name, ".nbt",NULL);
                g_free(output_filename);
                output_filename = new_of_name;
            }

            /* Transform Region to NBT */
            printf(_("Saving file: %s.\n"), output_filename);
            NbtInstance* instance = nbt_instance_new_from_region(region);
            dh_nbt_instance_save_to_file(instance, output_filename);
            dh_nbt_instance_free(instance);

            g_free(output_filename);
        }
        goto free_array_return;
    }
    else if(g_str_equal(output_format, "litematic"))
    {

    }
    else if(g_str_equal(output_format, "schematic"))
    {

    }
    else fprintf(stderr, _("Unrecognized format!\n"));

free_array_return:
    g_ptr_array_free(region_array, TRUE);
    return;

same_format_return:
    fprintf(stderr, _("The format of input file is the same as the output file, which is invalid!\n"));
    goto free_array_return;
}

extern int
start_point (int argc, char **argv, const char* prpath)
{
    if(argc == 2 && (g_str_equal(argv[1], "--help") || g_str_equal(argv[1], "-h")))
    {
        printf(_("This is a simple but stupid converter, it can only \n"
        "receive following format:\n"));
        printf("\n");
        printf(_("[input file] -of [format]\n"));

        printf("\n");
        printf(_("For example: Station.litematic -of nbt\n"));

        printf("\n");
        printf(_("format support: nbt, litematic, schematic\n"));
    }
    else if(argc == 4 && g_str_equal(argv[2], "-of"))
    {
        analyse(argv[1], argv[3]);
    }
    else printf(_("Unrecognized arguments, see \"--help\" for help.\n"));
    return 0;
}

extern const char* module_name()
{
    return NULL;
}

extern DhStrArray* module_name_array()
{
    DhStrArray* arr = NULL;
    dh_str_array_add_str(&arr, "conv");
    dh_str_array_add_str(&arr, "converter");
    return arr;
}

extern const char* module_description()
{
    return _("Convert a type of schematic file to other format.");
}

extern const char* help_description()
{
    return NULL;
}