/*  litematica_reader_c - litematic file reader in C (ISO C mode)
    Copyright (C) 2022 Dream Helium
    This file is part of litematica_reader_c.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>. */

#include "create_nbt.h"
#include "dhlrc_list.h"
#include "download_file.h"
#include "libnbt/nbt.h"
#include "main.h"
#include "nbt_litereader.h"
#include "translation.h"
#include <dhutil.h>
#include "litematica_region.h"
#include "recipe_util.h"
#include "lrc_extend.h"
#include <dh_validator.h>
#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>

enum option{Reader, Litematic_material_lister, Litematic_block_show,
        #ifdef DH_DEBUG_IN_IDE
            Debug,
        #endif
            Exit};

static enum option start_without_option();
static int start_func(NBT* root, enum option opt);
static int start_lrc_main(NBT* root);
#ifdef DH_DEBUG_IN_IDE
static int debug(NBT* root);
#endif

extern gchar* log_filename;
static FILE* log_file = NULL;

static void write_log(const gchar* log_domain, GLogLevelFlags log_level, const gchar* message, gpointer user_data)
{
    if(log_file && (log_level != G_LOG_LEVEL_CRITICAL))
    {
        GDateTime* time = g_date_time_new_now_local();
        gchar* time_literal = g_date_time_format(time, "%T");
        fprintf(log_file, "%s.%d ", time_literal, g_date_time_get_microsecond(time));
        fprintf(log_file, "%s" ,message);
        fprintf(log_file, "\n");

        g_free(time_literal);
        g_date_time_unref(time);
    }
    else
        g_log_default_handler(log_domain, log_level, message, user_data);
}

int main_isoc(int argc, char** argv)
{
#ifndef DH_DEBUG_IN_IDE
    if(argc == 1)
    {
        g_strfreev(argv);
        return -1;
    }
#endif
    gchar* cache_dir = dh_get_cache_dir();
    // dh_download_version_manifest(cache_dir, dh_file_progress_callback);
    g_free(cache_dir);
    int size = 0;
    uint8_t* data = NULL;
    data = (uint8_t*)dh_read_file(argv[1],&size);
#ifdef DH_DEBUG_IN_IDE
    printf("You are in debug mode! Don't define \"DH_DEBUG_IN_IDE\" to use the normal program!\n");
    printf("Anyway, enter 3 in the following program (if success reading file) to enter debug function. \n\n");
    if(!data)
        data = (uint8_t*)dh_read_file("/path/to/litematic",&size);
#endif
    if(!data)
    {
        puts(_("Error when reading file."));
        g_strfreev(argv);
        return -10;
    }
    /* Set log system */
    if(log_filename)
    {
        log_file = fopen(log_filename, "wb");
        g_log_set_default_handler(write_log, NULL);
    }

    NBT* root = NBT_Parse(data,size);
    free(data);

    if(!root)
    {
        puts(_("Not a valid NBT file!"));
        g_strfreev(argv);
        return -1;
    }
    else
    {
        puts(_("Valid NBT file!"));
        int ret = start_func(root, start_without_option());
        NBT_Free(root);
        if(log_file)
            fclose(log_file);
        g_strfreev(argv);
        dh_exit1();
        return ret;
    }
}


static enum option start_without_option()
{
    printf(_("There are three functions:\n"
    "[0] NBT lite reader with modifier\n"
    "[1] Litematica material list with recipe combination\n"
    "[2] Litematica block reader\n\n"));
    DhOut* out = dh_out_new();
    DhIntValidator* validator = dh_int_validator_new(0, 2);
    DhArgInfo* arg = dh_arg_info_new();
    dh_arg_info_add_arg(arg, 'q', "quit", N_("Quit application"));
    #ifdef DH_DEBUG_IN_IDE
    dh_arg_info_add_arg(arg, 'd', "debug", "Enter debug mode");
    #endif
    GValue val = {0};
    dh_out_read_and_output(out, N_("Please select an option, or enter 'q' to exit the program (q): "), "dhlrc"
    , arg, DH_VALIDATOR(validator), FALSE, &val);
    g_object_unref(out);
    g_object_unref(validator);
    g_object_unref(arg);
    
    if(G_VALUE_HOLDS_INT64(&val))
        return g_value_get_int64(&val);
    else
#ifdef DH_DEBUG_IN_IDE
    if(G_VALUE_HOLDS_CHAR(&val))
        if(g_value_get_schar(&val) == 'd')
            return Debug;
    else
#endif
        return Exit;
}

static int start_func(NBT *root, enum option opt)
{
    switch (opt) {
    case Reader:
        nbtlr_start(root);
        return 0;
    case Litematic_material_lister:
        return start_lrc_main(root);
    case Litematic_block_show:
        start_lrc_extend(root);
        return 0;
#ifdef DH_DEBUG_IN_IDE
    case Debug:
        return debug(root);
#endif
    case Exit:
        return 0;
    default: return 0;
    }
}

static int start_lrc_main(NBT *root)
{
    int region_num = lite_region_num(root);
    int err = 0;
    char** region_name = lite_region_names(root,region_num,&err);
    if(region_name)
    {
        printf(_("There are %d regions:\n"),region_num);
        for(int i = 0 ; i < region_num ; i++)
        {
            printf("[%2d] %s\n",i,region_name[i]);
        }
        printf("\n");
        int process_num = 0;
        long* process_region_i = num_array_get_from_input(&process_num, region_num);
        ItemList* il = NULL;
        if(process_region_i)
        {
            for(int i = 0 ; i < process_num ; i++)
            {
                g_message(_("Processing: region %d / %d : [%ld] %s"),
                       i,process_num,process_region_i[i],region_name[process_region_i[i]]);
                il = lite_region_item_list_extend(root, process_region_i[i], il, 1);
            }
            free(process_region_i);
            lite_region_free_names(region_name,region_num);
            item_list_delete_zero_item(&il);

            item_list_sort(&il);
            item_list_to_csv("test.csv",il);
            item_list_free(il);
        }
    }
    return dh_exit();
}



#ifdef DH_DEBUG_IN_IDE

#include "region.h"

int debug(NBT* root)
{
    LiteRegion* lr = lite_region_create(root, 0);
    Region* region = region_new_from_lite_region(lr);
    BlockInfoArray* arr = region->block_info_array;
    for(int i = 0 ; i < arr->len ; i++)
    {
        if(((BlockInfo*)(arr->pdata[i]))->nbt)
            nbtlr_start(((BlockInfo*)(arr->pdata[i]))->nbt);
    }
    
    return 0;
}


#endif
