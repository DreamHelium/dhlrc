/*  dhlrc_list - useful linked lists
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

#include "dhlrc_list.h"
#include "recipe_class/recipe_general.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cjson/cJSON.h>
#include "recipe_util.h"
#include "translation.h"
#include "json_util.h"

static gboolean enable_shaped = TRUE;
static gboolean enable_smelting = TRUE;
static gboolean enable_shapeless = TRUE;
static gboolean type_is_supported(const char* type);

void recipe_list_enable_feature(gboolean shaped, gboolean smelting,gboolean shapeless)
{
    enable_shaped = shaped;
    enable_smelting = smelting;
    enable_shapeless = shapeless;
}

typedef struct RListData{
    char* o_name;
    DhStrArray* r_name;
} RListData;

typedef struct RecipeListBaseData{
    char* filename;
    char* inamespace;
    char* item_name;
    char* recipe_type;
    gboolean supported;
} RecipeListBaseData;

static GList* recipe_filenames(const char* dir, ItemList* il);
static RecipeListBaseData* rlbasedata_init(const char* dir, const char* filename);
static void get_name(cJSON* json, char** namespace, char** item_name);
static void rlbasedata_free(gpointer data);

static ItemList* analyse_shaped(guint num, const char* filename, DhGeneral* self);
static ItemList* analyse_smelting(guint num, const char* filename);
static ItemList* analyse_shapeless(guint num, const char* filename, DhGeneral* self);
static void analyse_object(guint num, cJSON* object, ItemList** list);
static guint mod_decide(guint num1, guint num2, DhGeneral* self);
static char* get_array_item_name(cJSON* json, int num);
static guint get_key_nums(const char* str, char key);
static void get_name_from_name(const char* name, char** namespace, char** item_name);

static int internal_strcmp(gconstpointer a, gconstpointer b)
{
    return strcmp(a,b);
}

static int rlistdata_strcmp(gconstpointer a, gconstpointer b)
{
    /* The first argument is GList Element data */
    return strcmp( ((RListData*)a)->o_name , b );
}

static int ilistdata_strcmp(gconstpointer a, gconstpointer b)
{
    return strcmp( ((IListData*)a)->name, b );
}

static int ilistdata_compare_by_total(gconstpointer a, gconstpointer b)
{
    return -(((IListData*)a)->total - ((IListData*)b)->total);
}

static int ilistdata_iszero(gconstpointer a, gconstpointer b)
{
    b = NULL;
    return (((IListData*)a)->total - 0);
}

static int rlbasedata_isfalse(gconstpointer a, gconstpointer b)
{
    b = NULL;
    return ((RecipeListBaseData*)a)->supported;
}

static int rcldata_issame(gconstpointer a, gconstpointer b)
{
    RecipeListBaseData* data = (RecipeListBaseData*)a;
    const char* str = (char*)b;

    char* namespace = NULL;
    char* item_name = NULL;
    get_name_from_name(str, &namespace, &item_name);
    int ret = -1;
    if(g_str_equal(namespace, data->inamespace) && g_str_equal(item_name, data->item_name))
        ret = 0;
    free(namespace);
    free(item_name);
    return ret;
}

static IListData* ilistdata_init_with_tag(const char* name, gboolean is_tag)
{
    IListData* ildata = (IListData*)malloc(sizeof(IListData));
    ildata->name = dh_strdup(name);
    ildata->total = 0;
    ildata->placed = 0;
    ildata->available = 0;
    ildata->is_tag = is_tag;
    return ildata;
}

static IListData* ilistdata_init(const char* name)
{
    return ilistdata_init_with_tag(name, FALSE);
}

void item_list_read(ItemList* il, DhGeneral* general)
{
    dh_new_win(general, FALSE);
    dh_printf(general, _("Name\t\tTotal\tPlaced\tAvailable\tIs tag\n"));
    while(il)
    {
        IListData* data = il->data;
        dh_printf(general, "%s\t\t%d\t%d\t%d\t%d\n", trm(data->name), data->total, data->placed, data->available, data->is_tag);
        // g_message("%ld", strlen(trm(data->name)));
        il = il->next;
    }
}

void item_list_free(ItemList* target)
{
    g_return_if_fail(target != NULL);
    GList* gld = target;
    for(; gld ; gld = gld->next)
    {
        IListData* ildata = gld->data;
        free(ildata->name);
        free(ildata);
    }
    g_list_free(target);
}

void item_list_sort(ItemList** oBlock)
{
    /* Originally this sort function compares total.
     * After update, this will still sort by total */
    ItemList* iln = item_list_sort_by_total(*oBlock);
    *oBlock = iln;
}

ItemList * item_list_sort_by_total(ItemList* il)
{
    return g_list_sort(il, ilistdata_compare_by_total);
}


int item_list_init_new_item(ItemList** oBlock,const char* block_name)
{
    return item_list_init_new_item_with_tag(oBlock, block_name, FALSE);
}

int item_list_init_new_item_with_tag(ItemList **oBlock, const char* block_name, gboolean is_tag)
{
    GList* gl = *oBlock;
    IListData* ildata = ilistdata_init_with_tag(block_name, is_tag);
    gl = g_list_prepend(gl, ildata);
    *oBlock = gl;
    return 0;
}

ItemList *item_list_init(const char* block_name)
{
    ItemList* il = NULL;
    item_list_init_new_item(&il, block_name);
    return il;
}

ItemList* item_list_init_with_tag(const char* block_name, gboolean is_tag)
{
    ItemList* il = NULL;
    item_list_init_new_item_with_tag(&il, block_name, is_tag);
    return il;
}

int item_list_add_num(ItemList** bl,int num,char* block_name)
{
    ItemList* item = g_list_find_custom(*bl, block_name, ilistdata_strcmp);
    if(item)
    {
        IListData* ildata = item->data;
        ildata->total = ildata->total + num;
        return 0;
    }
    else
    {
        /* In this new implement we could create item for it first */
        ItemList* il = *bl;
        item_list_init_new_item( &il, block_name);
        *bl = il;
        item = g_list_find_custom(il, block_name, ilistdata_strcmp);
        if(item)
        {
            IListData* ildata = item->data;
            ildata->total = ildata->total + num;
            return 0;
        }
        else return -1;
    }
}

void item_list_add_num_by_index(ItemList* il, gint num, gint index)
{
    ItemList* item = g_list_nth(il, index);
    if(item)
    {
        IListData* ildata = item->data;
        ildata->total = ildata->total + num;
    }
    else return;
}

gint item_list_item_index(ItemList* il, const char* item_name)
{
    ItemList* item = g_list_find_custom(il, item_name, ilistdata_strcmp);
    if(item)
        return g_list_position(il, item);
    else return -1;
}


int item_list_scan_repeated(ItemList* bl,char* block_name)
{
    ItemList* item = g_list_find_custom(bl, block_name, ilistdata_strcmp);
    if(item) return 1;
    else return 0;
}

int item_list_delete_item(ItemList** bl,char* block_name)
{
    ItemList* il = *bl;
    GList* item = g_list_find_custom(il, block_name, ilistdata_strcmp);
    if(item)
    {
        IListData* data = item->data;
        free(data->name);
        free(data);
        il = g_list_remove(il, item->data);
        *bl = il;
        return 0;
    }
    else return -1;
}

void item_list_delete_zero_item(ItemList** bl)
{
    ItemList* head = *bl;
    gint have_zero = 1;
    while(have_zero)
    {
        GList* item = g_list_find_custom(head, GINT_TO_POINTER(0), ilistdata_iszero);
        if(item)
        {
            IListData* data = item->data;
            free(data->name);
            free(data);
            head = g_list_remove(head, item->data);
            *bl = head;
        }
        else have_zero = 0;
    }
}

int item_list_combine(ItemList** dest, ItemList* src)
{
    ItemList* s = src;
    while(s)
    {
        IListData* ildata = s->data;
        item_list_add_num(dest, ildata->total, ildata->name);
        s = s->next;
    }
    return 0;
}

BlackList* black_list_init()
{
    GList* bl = NULL;
    cJSON *black_list = dhlrc_file_to_json("config/ignored_blocks.json");
    if(black_list)
    {
        if(cJSON_IsArray(black_list))
        {
            int n = cJSON_GetArraySize(black_list);
            for(int i = 0 ; i < n ; i++)
            {
                char* item = cJSON_GetStringValue(cJSON_GetArrayItem(black_list,i));
                bl = black_list_extend(bl,item);
            }
        }
        cJSON_Delete(black_list);
        return bl;
    }
    else
    {
        bl = black_list_extend(bl,"minecraft:air");
        bl = black_list_extend(bl,"minecraft:piston_head");
        bl = black_list_extend(bl,"minecraft:fire");
        bl = black_list_extend(bl,"minecraft:soul_fire");
        bl = black_list_extend(bl,"minecraft:bubble_column");
    return bl;
    }
}

BlackList* black_list_extend(BlackList* bl,const char* name)
{
    bl = g_list_prepend(bl, dh_strdup(name));
    return bl;
}

void black_list_free(BlackList* bl)
{
    g_list_free_full(bl, free);
}

int black_list_scan(BlackList* bl,const char* name)
{
    if(!bl)
        return 0;
    else
    {
        GList* gl = g_list_find_custom(bl, name, internal_strcmp);
        if(gl) return 1;
        else return 0;
    }
}

ReplaceList* replace_list_init()
{
    ReplaceList* rl = NULL;
    cJSON* rlist_o = dhlrc_file_to_json("config/block_items.json");
    if(rlist_o)
    {
        cJSON* rlist = rlist_o->child;
        while(rlist)
        {
            if(cJSON_IsString(rlist))
                rl = replace_list_extend(rl,rlist->string,rlist->valuestring);
            else if(cJSON_IsArray(rlist))
            {
                int size = cJSON_GetArraySize(rlist);
                DhStrArray* str = NULL;
                for(int i = 0; i < size ; i++)
                {
                    char* r_name = cJSON_GetStringValue( cJSON_GetArrayItem(rlist, i) );
                    dh_str_array_add_str( &str, r_name);
                }
                rl = replace_list_extend_str_array(rl, rlist->string, str);
            }
            rlist = rlist->next;
        }
        cJSON_Delete(rlist_o);
        return rl;
    }
    else
    {
        rl = replace_list_extend(rl,"minecraft:water","minecraft:water_bucket");
        rl = replace_list_extend(rl,"minecraft:lava","minecraft:lava_bucket");
        rl = replace_list_extend(rl,"minecraft:redstone_wall_torch","minecraft:redstone_torch");
        return rl;
    }
}

ReplaceList* replace_list_extend(ReplaceList* rl,const char* o_name,const char* r_name)
{
    RListData* rld = (RListData*) malloc(sizeof(RListData));
    DhStrArray* str = dh_str_array_init( r_name );
    rld->o_name = dh_strdup(o_name);
    rld->r_name = str;
    rl = g_list_prepend(rl, rld);
    return rl;
}

ReplaceList * replace_list_extend_str_array(ReplaceList* rl, const char* o_name, DhStrArray* str)
{
    RListData* rld = (RListData*) malloc(sizeof(RListData));
    rld->o_name = dh_strdup(o_name);
    rld->r_name = str;
    rl = g_list_prepend(rl, rld);
    return rl;
}


const char* replace_list_replace(ReplaceList* rl, const char* o_name)
{
    DhStrArray* str = replace_list_replace_str_array(rl, o_name);
    if(str)
        return (str->val)[0];
    else return o_name;
}

DhStrArray * replace_list_replace_str_array(ReplaceList* rl, const char* o_name)
{
    if(rl)
    {
        GList* gl =  g_list_find_custom(rl, o_name, rlistdata_strcmp);
        if(gl)
        {
            RListData* rld = gl->data;
            return rld->r_name;
        }
        else return NULL;
    }
    else return NULL;
}


void replace_list_free(ReplaceList* rl)
{
    ReplaceList* rld = rl;
    for( ; rld ; rld = rld->next)
    {
        RListData* rldata = rld->data;
        dh_str_array_free(rldata->r_name);
        free(rldata->o_name);
        free(rldata);
    }
    g_list_free(rl);
}


int item_list_get_item_num(ItemList *il, char *item_name)
{
    GList* item = g_list_find_custom(il, item_name, ilistdata_strcmp);
    if(item)
    {
        IListData* ildata = item->data;
        return ildata->total;
    }
    else return -1;
}

int item_list_to_csv(char* pos,ItemList* il)
{
    FILE* f = fopen(pos,"wb");
    fprintf(f,"\"Item\",\"Total\",\"Missing\",\"Available\"\n");
    ItemList* ild = il;
    while(ild)
    {
        IListData* ildata = ild->data;
        const char* trans = name_block_translate(ildata->name);
        if(trans)
            fprintf(f,"\"%s\",%d,%d,%d\n",trans,ildata->total,ildata->total-ildata->placed,ildata->available);
        else
            fprintf(f,"%s,%d,%d,%d\n",ildata->name,ildata->total,ildata->total-ildata->placed,ildata->available);
        ild = ild->next;
    }
    fclose(f);
    return 0;
}

const char * item_list_item_name(ItemList* il)
{
    if(il)
    {
        IListData* ildata = il->data;
        return ildata->name;
    }
    else return NULL;
}

RecipeList* recipe_list_init(const char* dir, ItemList* il)
{
    GList* filenames = recipe_filenames(dir,il );
    RecipeList* list = NULL;

    GList* filenames_d = filenames;
    while (filenames_d) {
        RecipeListBaseData* rlbasedata = rlbasedata_init(dir, (char*)filenames_d->data);
        list = g_list_prepend(list, rlbasedata);
        filenames_d = filenames_d->next;
    }

    g_list_free_full(filenames, free);

    /********     Remove unsupported files    ****************/
    gboolean has_unsupported = TRUE;
    while(has_unsupported)
    {
        GList* item = g_list_find_custom(list, NULL, rlbasedata_isfalse);
        if(item)
        {
            rlbasedata_free(item->data);
            list = g_list_remove(list, item->data);
        }
        else has_unsupported = FALSE;
    }
    return list;
}

static RecipeListBaseData* rlbasedata_init(const char* dir, const char* filename)
{
    RecipeListBaseData* data = malloc(sizeof(RecipeListBaseData));
    g_return_val_if_fail(data != NULL, NULL);

    /* For example, "recipe", "/", "torch.json" */
    char* full_filename = g_strconcat(dir, "/",filename , NULL);
    cJSON* json_object = dhlrc_file_to_json(full_filename);
    if(json_object)
    {
        gboolean supported = type_is_supported( cJSON_GetStringValue(
            cJSON_GetObjectItem(json_object, "type")
        )  );
        data->supported = supported;
        data->filename = dh_strdup(full_filename);
        data->recipe_type = dh_strdup(cJSON_GetStringValue(
            cJSON_GetObjectItem(json_object, "type")
        ));

        get_name(json_object, &(data->inamespace), &(data->item_name));
        cJSON_Delete(json_object);
        g_free(full_filename);
        return data;
    }
    else
    {
        g_free(full_filename);
        return NULL;
    }
}

static gboolean type_is_supported(const char* type)
{
    if(enable_shaped)
    {
        if(g_str_has_suffix(type, "crafting_shaped"))
            return TRUE;
    }
    if(enable_smelting)
    {
        if(g_str_has_suffix(type, "blasting") || g_str_has_suffix(type, "smelting"))
            return TRUE;
    }
    if(enable_shapeless)
    {
        if(g_str_has_suffix(type, "crafting_shapeless"))
            return TRUE;
    }
    return FALSE;
}

static void get_name(cJSON* json, char** namespace, char** item_name)
{
    cJSON* result_json = cJSON_GetObjectItem(json, "result");
    char* full_name = NULL;
    if(cJSON_IsString(result_json))
        full_name = cJSON_GetStringValue(result_json);
    else
    {
        result_json = cJSON_GetObjectItem(result_json, "item");
        full_name = cJSON_GetStringValue(result_json);
    }

    char* new_full_name = g_strdelimit(dh_strdup(full_name), ":" ,0 );
    *namespace = dh_strdup(new_full_name);

    char* full_name_d = new_full_name;
    while(*full_name_d != 0 ) full_name_d++;
    full_name_d++;
    *item_name = dh_strdup(full_name_d);
    free(new_full_name);
}

static void get_name_from_name(const char* name, char** namespace, char** item_name)
{
    char* new_full_name = g_strdelimit(dh_strdup(name), ":" ,0 );
    *namespace = dh_strdup(new_full_name);

    char* full_name_d = new_full_name;
    while(*full_name_d != 0 ) full_name_d++;
    full_name_d++;
    *item_name = dh_strdup(full_name_d);
    free(new_full_name);
}

static GList* recipe_filenames(const char* dir, ItemList* il)
{
    /* Get file names */
    GList* filenames = dh_file_list_create(dir);
    g_return_val_if_fail(filenames != NULL , NULL);
    /* Get corresponding recipe file */
    GList* recipe_file_names = NULL;
    ItemList* ild = il;
    while (ild) {
        const char* item_name = item_list_item_name(ild);
        while(*item_name != ':') item_name++;
        item_name++;
        GList* single_recipe_filename = dh_search_in_list(filenames, item_name );
        GList* single_recipe_filename_d = single_recipe_filename;
        /* g_message("%s",item_name); */
        /* Find first corresponding file name */

        gchar* json_file = g_strconcat(item_name,".json" , NULL);

        while (single_recipe_filename_d) {
            if(g_str_equal(json_file, single_recipe_filename_d->data))
            {
                /* g_message("c"); */
                /* Write this file to list */
                recipe_file_names = g_list_prepend(recipe_file_names, dh_strdup(json_file));
                g_free(json_file);
                /* Force update */
                single_recipe_filename_d = single_recipe_filename_d->next;
                json_file = g_strconcat(item_name, "_from", NULL);
                GList* recipe_from_filenames = dh_search_in_list(single_recipe_filename_d, json_file);
                /* The "recipe_for_filenames" are the newly assigned memory,
                 * so just concat here  */
                /* Maybe not because _from might have a wrong prefix */
                if(recipe_from_filenames)
                {
                    GList* recipe_from_filenames_d = recipe_from_filenames;
                    while(recipe_from_filenames_d)
                    {
                        if(g_str_has_prefix(recipe_from_filenames_d->data, json_file))
                            recipe_file_names = g_list_prepend(recipe_file_names, dh_strdup(recipe_from_filenames_d->data));
                        recipe_from_filenames_d = recipe_from_filenames_d->next;
                    }
                    g_list_free_full(recipe_from_filenames, free);
                }
                break;
            }
            single_recipe_filename_d = single_recipe_filename_d -> next;
        }

        g_free(json_file);

        g_list_free_full(single_recipe_filename, free);

        ild = ild->next;
    }
    recipe_file_names = g_list_sort(recipe_file_names, internal_strcmp);
    g_list_free_full(filenames, free);
    return recipe_file_names;
}

char* recipe_list_filename(RecipeList* rcl)
{
    return ((RecipeListBaseData*)(rcl->data))->filename;
}

char* recipe_list_item_name(RecipeList* rcl)
{
    return ((RecipeListBaseData*)(rcl->data))->item_name;
}

static char* rcl_namespace(RecipeList* rcl)
{
    return ((RecipeListBaseData*)(rcl->data))->inamespace;
}

DhStrArray* recipe_list_item_names(RecipeList* rcl)
{
    DhStrArray* arr = NULL;
    while(rcl)
    {
        char* item_name = recipe_list_item_name(rcl);
        if(!dh_str_array_find_repeated(arr, item_name))
            dh_str_array_add_str(&arr, item_name);
        rcl = rcl->next;
    }
    return arr;
}

DhStrArray* recipe_list_item_names_with_namespace(RecipeList* rcl)
{
    DhStrArray* arr = NULL;
    while(rcl)
    {
        char* item_name = recipe_list_item_name(rcl);
        char* namespace = rcl_namespace(rcl);

        gchar* full_name_g = g_strconcat(namespace, ":", item_name , NULL);

        if(!dh_str_array_find_repeated(arr, full_name_g))
            dh_str_array_add_str(&arr, full_name_g);
        g_free(full_name_g);
        rcl = rcl->next;
    }
    return arr;
}

void recipe_list_free(RecipeList* rcl)
{
    g_list_free_full(rcl, rlbasedata_free);
}

static void rlbasedata_free(gpointer data)
{
    RecipeListBaseData* data_d = data;
    free(data_d->filename);
    free(data_d->item_name);
    free(data_d->inamespace);
    free(data_d->recipe_type);
    free(data_d);
}

ItemList* item_list_recipe(RecipeList* rcl, int num, const char* item_name, DhGeneral* general)
{
    /*dh_*/printf(/*general, */_("Processing %s.\n"), trm(item_name));
    RecipeList* item_recipes = dh_search_in_list_custom(rcl, item_name, rcldata_issame);
    RecipeList* option_recipe = item_recipes;

    if(g_list_length(item_recipes) > 1)
    {
        dh_new_win(general, FALSE);
        dh_printf(general, _("There are %d corresponding files to the item %s:\n"), g_list_length(item_recipes), trm(item_name));
        RecipeList* item_recipes_d = item_recipes;
        while(item_recipes_d)
        {
            dh_option_printer(general, g_list_index(item_recipes, item_recipes_d->data), recipe_list_filename(item_recipes_d));
            item_recipes_d = item_recipes_d->next;
        }
        int option = dh_selector(general, _("Please select an option, or 'd' to discard (d): "), g_list_length(item_recipes), "d", _("&Discard"));
        if(option == -1)
        {
            g_list_free(item_recipes);
            return NULL;
        }
        else
            option_recipe = g_list_nth(item_recipes, option);
    }

    /* Analyse */
    ItemList* ret = NULL;
    RecipeGeneral* recipe_general = recipe_general_new(recipe_list_filename(option_recipe));
    RecipeGeneralClass* klass = RECIPE_GENERAL_GET_CLASS(recipe_general);

    ret = klass->get_recipe(recipe_general, num, general);


    // char* type = ((RecipeListBaseData*)(option_recipe->data))->recipe_type;
    // if(enable_shaped)
    // {
    //     if(g_str_has_suffix(type, "crafting_shaped"))
    //         ret = analyse_shaped(num,RecipeList_Filename(option_recipe) ,general );
    // }
    // if(enable_smelting)
    // {
    //     if(g_str_has_suffix(type, "blasting") || g_str_has_suffix(type, "smelting"))
    //         ret = analyse_smelting(num, RecipeList_Filename(option_recipe));
    // }
    // if(enable_shapeless)
    // {
    //     if(g_str_has_suffix(type, "crafting_shapeless"))
    //         ret = analyse_shapeless(num, RecipeList_Filename(option_recipe), general);
    // }
    g_list_free(item_recipes);
    return ret;
}
//
// /* Reference: https://minecraft.fandom.com/wiki/Recipe
//  * And recipe files */
//
// static ItemList* analyse_smelting(guint num, const char* filename)
// {
//     cJSON* json = dhlrc_FileToJSON(filename);
//     g_return_val_if_fail(json != NULL, NULL);
//     ItemList* list = NULL;
//     cJSON* ingredient = cJSON_GetObjectItem(json, "ingredient");
//
//     if(cJSON_IsObject(ingredient))
//         analyse_object(num, ingredient, &list);
//     else if(cJSON_IsArray(ingredient))
//     {
//         /* Hopefully it works */
//         guint arr_num = cJSON_GetArraySize(ingredient);
//         for(int i = 0 ; i < arr_num ; i++)
//         {
//             cJSON* object = cJSON_GetArrayItem(ingredient, i);
//             analyse_object(num, object, &list);
//         }
//     }
//     cJSON_Delete(json);
//     return list;
// }
//
// static ItemList* analyse_shapeless(guint num, const char* filename, DhGeneral* self)
// {
//     cJSON* json = dhlrc_FileToJSON(filename);
//     g_return_val_if_fail(json != NULL, NULL);
//     int num2 = 1;
//     cJSON* count = cJSON_GetObjectItem(cJSON_GetObjectItem(json, "result"), "count");
//     if(cJSON_IsNumber(count))
//         num2 = cJSON_GetNumberValue( count );
//
//     ItemList* list = NULL;
//     guint division = mod_decide(num, num2, self);
//     if(division)
//     {
//         cJSON* ingredient = cJSON_GetObjectItem(json, "ingredient");
//         if(cJSON_IsObject(ingredient))
//             analyse_object(division, ingredient, &list);
//         else if(cJSON_IsArray(ingredient))
//         {
//             dh_printf(self, _("There are some ingredients to choose:\n"));
//             guint size = cJSON_GetArraySize(ingredient);
//             for(int i = 0 ; i < size ; i++)
//                 dh_option_printer(self, i, get_array_item_name(ingredient, i));
//             int option = dh_selector(self, _("Please select an item/tag, or enter 'a' to give up selecting (a): "), size, "a", _("&Abort"));
//             if(option == -1)
//             {
//                 cJSON_Delete(json);
//                 return NULL;
//             }
//             else {
//                 analyse_object(division, cJSON_GetArrayItem(ingredient, option), &list);
//             }
//         }
//         else{
//             cJSON_Delete(json);
//             return NULL;
//         }
//     }
//     else {
//         cJSON_Delete(json);
//         return NULL;
//     }
//     cJSON_Delete(json);
//     return list;
// }
//
// static ItemList* analyse_shaped(guint num, const char* filename, DhGeneral* self)
// {
//     cJSON* json = dhlrc_FileToJSON(filename);
//     g_return_val_if_fail(json != NULL, NULL);
//
//     int num2 =  1;
//     cJSON* count = cJSON_GetObjectItem(cJSON_GetObjectItem(json, "result"), "count");
//     if(cJSON_IsNumber(count))
//         num2 = cJSON_GetNumberValue( count );
//
//     ItemList* list = NULL;
//     guint division = mod_decide(num, num2, self);
//     if(self)
//     {
//         cJSON* pattern = cJSON_GetObjectItem(json, "pattern");
//         guint pattern_size = cJSON_GetArraySize(pattern);
//         DhStrArray* pattern_stra = NULL;
//         for(int i = 0 ; i < pattern_size ; i++)
//         {
//             char* str = cJSON_GetStringValue(cJSON_GetArrayItem(pattern, i));
//             DhStrArray_AddStr(&pattern_stra, str);
//         }
//         char* pattern_string = DhStrArray_cat(pattern_stra);
//         DhStrArray_Free(pattern_stra);
//
//         /* Get keys */
//         cJSON* keys = cJSON_GetObjectItem(json, "key");
//         guint key_num = cJSON_GetArraySize(keys);
//         for(int i = 0 ; i < key_num ; i++)
//         {
//             cJSON* key = cJSON_GetArrayItem(keys, i);
//             guint item_num = get_key_nums(pattern_string, *(key->string));
//             /* g_message("%s key corresponding %d nums.", key->string, item_num); */
//             if(cJSON_IsObject(key))
//                 analyse_object(item_num * division, key, &list);
//             else if(cJSON_IsArray(key))
//             {
//                 dh_printf(self, _("There are some ingredients to choose:\n"));
//                 guint size = cJSON_GetArraySize(key);
//                 for(int i = 0 ; i < size ; i++)
//                     dh_option_printer(self, i, get_array_item_name(key, i));
//                 int option = dh_selector(self, _("Please select an item/tag, or enter 'a' to give up selecting (a): "), size, "a", _("&Abort"));
//                 if(option == -1)
//                 {
//                     free(pattern_string);
//                     cJSON_Delete(json);
//                     ItemList_Free(list);
//                     return NULL;
//                 }
//                 else {
//                     analyse_object(item_num * division, cJSON_GetArrayItem(key, option), &list);
//                 }
//             }
//         }
//         free(pattern_string);
//     }
//     else {
//         cJSON_Delete(json);
//         return NULL;
//     }
//     cJSON_Delete(json);
//     return list;
// }
//
// static void analyse_object(guint num, cJSON* object, ItemList** list)
// {
//     cJSON* item = cJSON_GetObjectItem(object, "item");
//     if(item)
//     {
//         ItemList_AddNum(list, num, cJSON_GetStringValue(item));
//     }
//     else {
//         cJSON* tag = cJSON_GetObjectItem(object, "tag");
//         if(tag)
//         {
//             ItemList_AddNum(list, num, cJSON_GetStringValue(tag));
//         }
//     }
// }



// static guint mod_decide(guint num1, guint num2, DhGeneral* self)
// {
//     if(num2 == 0) return 0;
//     int mod = num1 % num2;
//     if(mod)
//     {
//         dh_printf(self, _("There is a remainder with %d and %d.\n"), num1, num2);
//         int choice = dh_selector(self, _("Enter 'd' to discard the result, or enter 'c' to continue (c): ") ,0, "cd", _("&Continue"), _("&Discard"));
//         if(choice == 0 || choice == -1) /* Return division number. */
//         {
//             return (num1 / num2) + 1;
//         }
//         else return 0;
//     }
//     else return (num1 / num2);
// }


static char* get_array_item_name(cJSON* json, int num)
{
    cJSON* item = cJSON_GetArrayItem(json, num);
    if(item)
    {
        cJSON* item_name = cJSON_GetObjectItem(item, "item");
        if(item_name)
            return cJSON_GetStringValue(item_name);
        else
        {
            cJSON* tag_name = cJSON_GetObjectItem(item, "tag");
            if(tag_name)
                return cJSON_GetStringValue(tag_name);
            else return NULL;
        }
    }
    else return NULL;
}

static guint get_key_nums(const char* str, char key)
{
    /* g_message("String is %s", str); */
    const char* str_d = str;
    guint i = 0;
    while(*str_d)
    {
        if(*str_d == key) i++;
        /* g_message("Processing %c", *str_d); */
        str_d++;
    }
    return i;
}
