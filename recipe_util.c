#include "recipe_util.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "litematica_region.h"

long* NumArray_GetFromInput(int* array_num)
{
    long* array = NULL;
    int i = 0;
    char* input = NULL;
    int len = 0;
    if(getline(&input,&len,stdin) != -1)
    {
        char* inputl = input;
        while(1)
        {
        char* end;
        long value = strtol(inputl,&end,10);
        if(inputl == end) break;
        i++;
        long* parray = realloc(array,i*sizeof(long));
        if(!parray)
        {
            free(array);
            free(input);
            *array_num = 0;
            return NULL;
        }
        array = parray;
        array[i - 1] = value;
        //printf("%ld ",value);
        inputl = end;
        }
    }
    free(input);
    *array_num = i;
    return array;
}

int ItemList_CombineRecipe(ItemList** o_bl)
{
    ItemList* bl = *o_bl;
    char** recipe_list = NULL;
    int recipe_num = 0;
    while(bl)
    {
        ItemList* recipe = ItemList_Recipe(bl->name,0);
        if(recipe)
        {
            recipe_num++;
            char** temp_rl = realloc(recipe_list,recipe_num * sizeof(char*));
            if(!temp_rl)
            {
                perror("Allocate recipe list error\n");
                lite_region_FreeNameArray(recipe_list,recipe_num);
                return -1;
            }
            recipe_list = temp_rl;
            recipe_list[recipe_num - 1] = (char*)malloc(bl->len * sizeof(char));
            strcpy(recipe_list[recipe_num - 1],bl->name);
            ItemList_Free(recipe);
        }
        bl = bl->next;
    }
    if(recipe_list)
    {
        printf("Please select recipes to combine: \n");
        for(int i = 0 ; i < recipe_num ; i++)
        {
            char* trans_name = Name_BlockTranslate(recipe_list[i]);
            if(trans_name)
                printf("[%d] %s \n",i,trans_name);
            else
                printf("[%d] %s\n",i,recipe_list[i]);
            free(trans_name);
        }
        printf("input here:");
        int craft_num = 0;
        long* craft = NumArray_GetFromInput(&craft_num);
        if(!craft)
        {
            lite_region_FreeNameArray(recipe_list,recipe_num);
            return 0;
        }
        else
        {
            for(int i = 0 ; i < craft_num ; i++)
            {
                int item_num = 0;
                bl = *o_bl;
                while(bl)
                {
                    if(!strcmp(bl->name,recipe_list[craft[i]]))
                    {
                        item_num = bl->num;
                        break;
                    }
                    bl = bl->next;
                }
                ItemList* recipe = ItemList_Recipe(recipe_list[craft[i]],item_num);
                if(ItemList_Combine(o_bl,recipe))
                {
                    free(craft);
                    ItemList_Free(recipe);
                    lite_region_FreeNameArray(recipe_list,recipe_num);
                    return -1;
                }
                else
                {
                    ItemList_DeleteItem(o_bl,recipe_list[craft[i]]);
                }
                ItemList_Free(recipe);
            }
            printf("Success!\n");
            free(craft);
            lite_region_FreeNameArray(recipe_list,recipe_num);
            return 0;
        }
    }
    else
    {
        printf("There's no item to combine.\n");
        return 0;
    }
}

ItemList* ItemList_Recipe(char* block_name,int num)
{
    //d = opendir("recipes");
    char* name = strchr(block_name,':') + 1;
    char* directory = (char*)malloc((14 + strlen(name))*sizeof(char));
    strcpy(directory,"recipes/");
    strcat(directory,name);
    strcat(directory,".json");
    ItemList* test = NULL;
    FILE* f = fopen(directory,"rb");
    free(directory);
    if(f)
        {
            fseek(f,0,SEEK_END);
            int size = ftell(f);
            fseek(f,0,SEEK_SET);
            char* data = (char*)malloc(size* sizeof(char));
            fread(data,1,size,f);
            fclose(f);
            cJSON* block_data = cJSON_ParseWithLength(data,size);
            char* type = cJSON_GetStringValue(cJSON_GetObjectItem(block_data,"type"));
            if(strstr(type,"minecraft:crafting_shaped"))
            {
                cJSON* a = cJSON_GetObjectItem(block_data,"pattern");
                int line = cJSON_GetArraySize(a);
                int count_num = 1;
                if(cJSON_IsNumber(cJSON_GetObjectItem(cJSON_GetObjectItem(block_data,"result"),"count")))
                    count_num = cJSON_GetNumberValue(cJSON_GetObjectItem(cJSON_GetObjectItem(block_data,"result"),"count"));
                int craft_num = 0;
                if(num % count_num == 0) craft_num = num / count_num;
                else craft_num = num / count_num + 1;
                for(int i = 0 ; i < line ; i++)
                {
                    cJSON* table = cJSON_GetArrayItem(a,i);
                    char* table_content = cJSON_GetStringValue(table);
                    for(int j = 0 ; j < strlen(table_content) ; j++)
                    {
                        if(table_content[j] != ' ')
                        {
                            char item_key[2] = {table_content[j],'\0'};
                            cJSON* key = cJSON_GetObjectItem(block_data,"key");
                            cJSON* item = cJSON_GetObjectItem(key,item_key);
                            char* item_name = NULL;
                            if(cJSON_IsArray(item))
                            {
                                item_name = cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetArrayItem(item,0),"item"));
                                if(!item_name)
                                    item_name = cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetArrayItem(item,0),"tag"));
                            }
                            else
                            {
                                item_name = cJSON_GetStringValue(cJSON_GetObjectItem(item,"item"));
                            if(!item_name) item_name = cJSON_GetStringValue(cJSON_GetObjectItem(item,"tag"));
                            }
                                if(!ItemList_ScanRepeat(test,item_name))
                                {
                                    if(ItemList_InitNewItem(&test,item_name))
                                    {
                                        cJSON_free(block_data);
                                        return NULL;
                                    }
                                }
                                    ItemList_AddNum(test,craft_num,item_name);
                           }
                        }
                    }
                /*
                ItemList* testl = test;
                for( ; testl ; testl = testl->next)
                {
                    printf("%-50s,%d\n",testl->name,testl->num);
                }
                */
                cJSON_free(block_data);
                free(data);
                if(test)
                    return test;
                return NULL;
            }
            else
            {
                cJSON_free(block_data);
                free(data);
                return NULL;
            }

            }
    else
    {
        perror("Couldn't find recipes, did you put the recipe in?");
        return NULL;
    }
}

char* Name_BlockTranslate(char* block_name)
{
    FILE* f = fopen("translation.json","rb");
    if(f)
    {
        fseek(f,0,SEEK_END);
        int size = ftell(f);
        fseek(f,0,SEEK_SET);
        char* data = (char*)malloc(size* sizeof(char));
        fread(data,1,size,f);
        fclose(f);
        cJSON* trans_data = cJSON_ParseWithLength(data,size);
        char* pure_name = strchr(block_name,':') + 1;
        int len = strlen(pure_name) + 1 + 5 + 10;
        char* name = (char*) malloc(len * sizeof(char));
        strcpy(name,"item.minecraft.");
        strcat(name,pure_name);
        cJSON* trans_line = cJSON_GetObjectItem(trans_data,name);
        if(cJSON_IsString(trans_line))
        {
            char* trans_name = cJSON_GetStringValue(trans_line);
            char* out = (char*)malloc((strlen(trans_name)+1)*sizeof(char));
            strcpy(out,trans_name);
            free(data);
            cJSON_free(trans_data);
            free(name);
            return out;
        }
        else
        {
            len++;
            free(name);
            name = (char*)malloc(len * sizeof(char));
            strcpy(name,"block.minecraft.");
            strcat(name,pure_name);
            trans_line = cJSON_GetObjectItem(trans_data,name);
            if(cJSON_IsString(trans_line))
            {
                char* trans_name = cJSON_GetStringValue(trans_line);
                char* out = (char*)malloc((strlen(trans_name)+1)*sizeof(char));
                strcpy(out,trans_name);
                free(data);
                cJSON_free(trans_data);
                free(name);
                return out;
            }
            else
            {
                free(data);
                cJSON_free(trans_data);
                free(name);
                return NULL;
            }
        }
    }
    else
        return NULL;
}
