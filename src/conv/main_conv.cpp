#include "main_conv.h"
#include "../litematica_region.h"
#include "../region.h"
#include "../translation.h"
#define DH_EDITLINE_USED
#include "../common_info.h"
#include "../feature/dh_module.h"
#include "../region_litematic.h"
#include "dh_validator_cpp.hpp"
#include "glib.h"

#include <string>

extern "C"
{

  enum format
  {
    DH_NBT,
    DH_LITEMATIC,
    DH_SCHEMATIC,
    DH_INVALID
  };

  typedef struct Converter
  {
    std::map<std::string, Region *> regions;
    std::string filename;
    format output_format;
  } Converter;

  static void
  refresh_screen ()
  {
#ifdef G_OS_WIN32
    system ("cls");
#else
    system ("clear");
#endif
  }

  static format
  test_file_format (DhNbtInstance &instance)
  {
    if (lite_region_num_instance (&instance))
      return DH_LITEMATIC;
    if (file_is_new_schem (&instance))
      return DH_SCHEMATIC;
    return DH_NBT;
  }

  static format
  get_format ()
  {
    dh::Arg arg;
    arg.add_arg ('n', _ ("Choose NBT"), "NBT");
    arg.add_arg ('l', _ ("Choose litematic"), "litematic", "lite", "lr");
    arg.add_arg ('s', _ ("Choose schematic"), "schematic", "schem");
    printf (_ ("Please choose an option:\n"));
    printf ("[0] NBT\n");
    printf ("[1] litematic\n");
    printf ("[2] schematic\n");
    auto val
        = dh::GetOutput::get_output (&arg, _ ("Choose an option: "), true);
    switch (val)
      {
      case 'n':
        return DH_NBT;
      case 'l':
        return DH_LITEMATIC;
      case 's':
        return DH_SCHEMATIC;
      default:
        return DH_INVALID;
      }
  }

  static std::string
  format_to_string (format format)
  {
    switch (format)
      {
      case DH_NBT:
        return "NBT";
      case DH_LITEMATIC:
        return "litematic";
      case DH_SCHEMATIC:
        return "schematic";
      default:
        return "";
      }
  }

  static void
  get_regions_from_litematic (std::map<std::string, Region *> &regions,
                              DhNbtInstance &instance)
  {
    auto len = lite_region_num_instance (&instance);
    printf ("%d\n", len);
    printf (_ ("The regions are listed below:\n"));
    auto names = lite_region_name_array_instance (&instance);
    for (int i = 0; i < len; i++)
      {
        printf ("[%d] %s\n", i, names->val[i]);
      }
    dh::VectorValidator<int64_t> validator (true, ',');
    validator.add_range (0, len - 1);
    auto ret = dh::GetOutput::get_output (&validator, nullptr,
                                          _ ("Please input region nums: "));
    try
      {
        auto ret_val = std::any_cast<std::vector<int64_t>> (ret);
        for (auto i : ret_val)
          {
            auto region = region_new_from_litematic (
                instance.get_original_nbt (), i, nullptr, nullptr, nullptr);
            regions.insert (
                std::pair<std::string, Region *> (names->val[i], region));
          }
      }
    catch (const std::bad_any_cast &e)
      {
      }
    dh_str_array_free (names);
  }

  static void
  add_file (std::vector<Converter> &regions)
  {
    auto output = dh::GetOutput::get_output (_ ("Please input filename: "));
    try
      {
        auto str = std::any_cast<std::string> (output);
        auto str_dup = g_strdup (str.c_str ());
        auto str_dup_o = str_dup;
        str_dup = g_strstrip (str_dup);
        DhNbtInstance instance (str_dup);
        auto file_basename = g_path_get_basename (str_dup);
        g_free (str_dup_o);
        if (instance.is_non_null ())
          {
            format instance_format = test_file_format (instance);
            Region *region = nullptr;
            std::map<std::string, Region *> lite_regions;
            switch (instance_format)
              {
              case DH_NBT:
                region = region_new_from_nbt_instance_ptr (&instance);
                break;
              case DH_SCHEMATIC:
                region = region_new_from_new_schem (&instance);
                break;
              case DH_LITEMATIC:
                get_regions_from_litematic (lite_regions, instance);
                break;
              default:
                break;
              }
            if (region)
              lite_regions.insert (std::pair (str, region));
            if (lite_regions.empty ())
              {
                printf (_ ("No region successfully added!"));
              }
            else
              {
                format output_format = get_format ();
                if (output_format != DH_INVALID)
                  {
                    Converter converter{ lite_regions, file_basename,
                                         output_format };
                    regions.push_back (converter);
                  }
                else
                  printf (_ ("Not a valid format chosen!"));
              }
            g_free (file_basename);
          }
        else
          printf (_ ("Not a valid file!"));
      }
    catch (const std::bad_any_cast &e)
      {
        printf (_ ("No file specified!"));
      }
  }

  static void
  set_dir (std::string &dir)
  {
    auto val = dh::GetOutput::get_output (_ ("Please enter a directory: "));
    try
      {
        auto tmp_dir = std::any_cast<std::string> (val);
        auto dir_dup = g_strdup (tmp_dir.c_str ());
        auto dir_dup_p = dir_dup;
        dir_dup = g_strstrip (dir_dup);
        if (g_file_test (dir_dup, G_FILE_TEST_IS_DIR))
          dir = dir_dup;
        else
          printf (_ ("Not a valid directory!"));
        g_free (dir_dup_p);
      }
    catch (const std::bad_any_cast &e)
      {
        printf (_ ("Error!"));
      }
  }

  static void
  convert_progress (Region *region, const std::string &filename,
                    const std::string &suffix, format output_format,
                    const std::string &dir)
  {
    auto filesuffix = [] (format format) -> std::string
      {
        switch (format)
          {
          case DH_NBT:
            return ".nbt";
          case DH_LITEMATIC:
            return ".litematic";
          case DH_SCHEMATIC:
            return ".schem";
          default:
            return {};
          }
      };

    std::string real_filename = dir;
    real_filename += G_DIR_SEPARATOR_S;
    real_filename += filename;
    if (!suffix.empty ())
      real_filename += "_" + suffix;
    real_filename += filesuffix (output_format);

    DhNbtInstance *instance = nullptr;
    switch (output_format)
      {
      case DH_NBT:
        instance = static_cast<DhNbtInstance *> (
            nbt_instance_ptr_new_from_region (region, false));
        break;
      case DH_LITEMATIC:
        instance = static_cast<DhNbtInstance *> (
            lite_instance_ptr_new_from_region (region, false));
        break;
      case DH_SCHEMATIC:
        instance = static_cast<DhNbtInstance *> (
            new_schema_instance_ptr_new_from_region (region, false));
        break;
      default:
        instance = nullptr;
        break;
      }
    if (instance)
      {
        instance->save_to_file (real_filename.c_str ());
        delete instance;
      }
  }

  static void
  real_convert (const std::vector<Converter> &converters,
                const std::string &dir)
  {
    for (auto i : converters)
      {
        auto &regions = i.regions;
        auto &filename = i.filename;
        auto &output_format = i.output_format;
        if (regions.size () == 1)
          {
            auto region = regions.begin ().operator* ().second;
            convert_progress (region, filename, {}, output_format, dir);
          }
        else
          {
            for (const auto &j : regions)
              {
                convert_progress (j.second, filename, j.first, output_format,
                                  dir);
              }
          }
      }
  }

  static void
  interactive ()
  {
    std::vector<Converter> regions;
    std::string dir;
    dh::Arg arg;
    arg.add_arg ('a', _ ("Add NBT File."), "add", _ ("add_"));
    arg.add_arg ('s', _ ("Set the output directory."), "set", _ ("set_"));
    arg.add_arg ('c', _ ("Convert the files."), "conv", _ ("convert"));
    arg.add_arg ('e', _ ("Exit the program."), "exit", _ ("exit_"));
    bool jump_out = false;
    while (!jump_out)
      {
        refresh_screen ();
        if (regions.empty ())
          printf (_ ("There are no regions.\n"));
        else
          {
            printf (_ ("The region(s) and converter are listed "
                       "below:\n"));
            for (const auto &i : regions)
              {
                printf ("%s, %lu region(s), convert to %s\n",
                        i.filename.c_str (), i.regions.size (),
                        format_to_string (i.output_format).c_str ());
              }
          }
        char val = dh::GetOutput::get_output (
            &arg, _ ("Please select an option: "), true);
        switch (val)
          {
          case 'a':
            add_file (regions);
            break;
          case 's':
            set_dir (dir);
            break;
          case 'c':
            if (dir.empty ())
              set_dir (dir);
            real_convert (regions, dir);
          default:
            jump_out = true;
            break;
          }
      }
    for (const auto &i : regions)
      {
        for (const auto &j : i.regions)
          {
            region_free (j.second);
          }
      }
  }

  static void
  analyse (const char *input_file, const char *output_format)
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
            int litematic_len = lite_region_num_instance (&instance);

            /* Make regions */
            for (int i = 0; i < litematic_len; i++)
              {
                Region *region
                    = region_new_from_litematic (instance.get_original_nbt (),
                                                 i, nullptr, nullptr, nullptr);
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
                    output_filename, "_", region->data->name, ".nbt", NULL);
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
            auto instance = (DhNbtInstance *)nbt_instance_ptr_new_from_region (
                region, false);
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
                fprintf (stderr, _ ("Multiple file convertion to "
                                    "litematic is not supported yet!\n"));
                g_free (output_filename);
                goto free_array_return;
              }
            else
              {
                char *new_of_name
                    = g_strconcat (output_filename, ".litematic", NULL);
                g_free (output_filename);
                output_filename = new_of_name;
              }

            /* Transform Region to NBT */
            printf (_ ("Saving file: %s.\n"), output_filename);
            auto instance
                = (DhNbtInstance *)lite_instance_ptr_new_from_region (region,
                                                                      false);
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
            auto region = static_cast<Region *> (region_array->pdata[i]);

            /* Get filename */
            char *output_filename = g_path_get_basename (input_file);
            *strrchr (output_filename, '.') = 0;
            if (region_array->len != 1)
              {
                char *new_of_name = g_strconcat (
                    output_filename, "_", region->data->name, ".schem", NULL);
                g_free (output_filename);
                output_filename = new_of_name;
              }
            else
              {
                char *new_of_name
                    = g_strconcat (output_filename, ".schem", NULL);
                g_free (output_filename);
                output_filename = new_of_name;
              }

            /* Transform Region to NBT */
            printf (_ ("Saving file: %s.\n"), output_filename);
            auto instance = static_cast<DhNbtInstance *> (
                new_schema_instance_ptr_new_from_region (region, false));
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
    translation_init (prpath);
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
        analyse (argv[1], argv[3]);
      }
    else if (argc == 1)
      interactive ();
    else
      printf (_ ("Unrecognized arguments, see \"--help\" for help.\n"));
    return 0;
  }

  extern void
  init (DhModule *module)
  {
    module->module_name = g_strdup ("conv");
    module->module_type = g_strdup ("custom");
    module->module_description = g_strdup ("Convertion Module");
    module->module_functions = g_ptr_array_new ();
    g_ptr_array_add (module->module_functions, (gpointer)start_point);
    g_ptr_array_add (module->module_functions,
                     (gpointer)nbt_instance_ptr_new_from_region);
    g_ptr_array_add (module->module_functions,
                     (gpointer)lite_instance_ptr_new_from_region);
    g_ptr_array_add (module->module_functions,
                     (gpointer)new_schema_instance_ptr_new_from_region);
    g_ptr_array_add (module->module_functions,
                     (gpointer)lite_instance_ptr_new_from_region_full);
    g_ptr_array_add (module->module_functions,
                     (gpointer)nbt_instance_ptr_new_from_region_full);
    g_ptr_array_add (module->module_functions,
                     (gpointer)nbt_instance_ptr_new_from_region_real_full);
    g_ptr_array_add (module->module_functions,
                     (gpointer)nbt_node_new_from_region_full);
  }
}