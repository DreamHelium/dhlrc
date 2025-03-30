#include "manage_nbt.h"
#include <cstdio>
#include "../translation.h"
#include "cli_util.h"
#include "../common_info.h"
#include "dh_validator_cpp.hpp"
#include "../nbt_interface/nbt_interface.h"
#include <fmt/color.h>

static int choose_option = -1;

// static void draw_interface()
// {
//     dhlrc_clear_screen();
//     printf("%s\n\n", _("The NBTs are listed below:"));

//     GList* uuid_list = (GList*)common_info_list_get_uuid_list(DH_TYPE_NBT_INTERFACE);
//     if(uuid_list)
//     {
//         for(int i = 0 ; i < g_list_length(uuid_list); i++)
//         {
//             CommonInfo* info = common_info_list_get_common_info(DH_TYPE_NBT_INTERFACE, (const char*)g_list_nth_data(uuid_list, i));
//             if(i != choose_option) printf("[%d] %s\n", i, info->description);
//             else fmt::print(fmt::fg(fmt::color::black) | fmt::bg(fmt::color::white), 
//             "[{}] {}\n", i, info->description);
//         }
//     }
//     printf("\n");
// }

// static int get_choose_option()
// {
//     GList* uuid_list = (GList*)common_info_list_get_uuid_list(DH_TYPE_NBT_INTERFACE);
//     if(uuid_list)
//     {
//         auto len = g_list_length(uuid_list);
//         void* validator = dh_int_validator_new(0, len - 1, 10);
//         GValue val = {0};
//         dh_get_output(validator, nullptr, _("Please enter a number"), &val);
//         if(G_VALUE_HOLDS_INT64(&val))
//             return g_value_get_int64(&val);
//         else return -1;
//     }
//     else fprintf(stderr, _("No NBTs added!"));
//     g_usleep(5 * G_USEC_PER_SEC);
//     return -1;
// }

// static void add_nbt()
// {
//     bool success = false;
//     GValue val = {0};
//     dh_get_output(nullptr, nullptr, "Please enter a filename ", &val);
//     if(G_VALUE_HOLDS_STRING(&val))
//     {
//         const char* str = g_value_get_string(&val);
//         const char* str_d = g_strstrip((char*)str);
//         NbtInstance* inf = dh_nbt_if_parse(str);
//         g_value_reset(&val);
//         if(inf)
//         {
//             success = true;
//             common_info_new(DH_TYPE_NBT_INTERFACE, inf, g_date_time_new_now_local(), "?");
//         }
//         else fprintf(stderr, _("Not valid NBT file!"));
//     }
//     else fprintf(stderr, _("Not valid filename!"));
//     if(!success) g_usleep(5 * G_USEC_PER_SEC);
// }

// extern "C"{
// int manage_nbt_instance()
// {
//     while(true)
//     {
//         draw_interface();
//         if(choose_option == -1)
//         {
//             void* args = dh_arg_new();
//             dhlrc_add_args_common(args, 'a', "add", N_("add_nbt"), _("Add NBT File."));
//             dhlrc_add_args_common(args, 'r', "remove", N_("remove_nbt"), _("Remove NBT."));
//             dhlrc_add_args_common(args, 'c', "choose", N_("choose_nbt"), _("Choose NBT."));
//             GValue val = {0};
//             dh_get_output_arg(args, _("Please choose an option "), true, &val);
//             if(G_VALUE_HOLDS_UCHAR(&val))
//             {
//                 auto val_c = g_value_get_uchar(&val);
//                 g_value_reset(&val);
//                 switch (val_c) 
//                 {
//                     case 'a':
//                         add_nbt();
//                         break;
//                     case 'r':
//                         break;
//                     case 'c':
//                         choose_option = get_choose_option();
//                         break;
//                     default: return -1;
//                 }
//             }
//             else return -1;
//         }
//         else return 0;
//     }
// }
// }