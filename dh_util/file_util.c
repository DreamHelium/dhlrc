/*  file_util - file utilities
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

#include "dh_string_util.h"
#include "file_util.h"
#include <stdlib.h>
#include <string.h>


int dhlrc_WriteFile(char* pos,char* content, size_t count)
{
    FILE* f = fopen(pos,"wb");
    fwrite(content,1,count,f);
    fclose(f);
    return 0;
}

char *dhlrc_ReadFile(const char *filepos, int *size)
{
    FILE* f = fopen(filepos,"rb");
    if(f)
    {
        fseek(f,0,SEEK_END);
        *size = ftell(f);
        fseek(f,0,SEEK_SET);

        char* data = (char*)malloc(*size * sizeof(char));
        fread(data,1,*size,f);
        fclose(f);
        return data;
    }
    else
    {
        *size = 0;
        return NULL;
    }
}

int dhlrc_mkconfig()
{
    cJSON* config = cJSON_CreateObject();
    if(config)
    {
        cJSON_AddStringToObject(config, "OverrideLang", "");
        cJSON_AddStringToObject(config, "SecondLang", "");
        cJSON_AddStringToObject(config, "langDir", "lang/");
        cJSON_AddStringToObject(config, "AdditionConfig", "config/");
        cJSON_AddStringToObject(config, "RecipeConfig", "recipes/");
        cJSON_AddStringToObject(config, "ItemTranslate", "translation.json");
        char* config_char = cJSON_Print(config);
        dhlrc_WriteFile("config.json", config_char, strlen(config_char));
        free(config_char);
        cJSON_Delete(config);
        return 0;
    }
    else return -1;
}

int dhlrc_ConfigExist()
{
    return dhlrc_FileExist("config.json");
}

char *dhlrc_ConfigContent(const char *str)
{
    if(dhlrc_ConfigExist())
    {
        int size;
        char* json = dhlrc_ReadFile("config.json", &size);
        if(json)
        {
            cJSON* json_content = cJSON_ParseWithLength(json, size);
            cJSON* json_str = cJSON_GetObjectItem(json_content, str);
            free(json);
            if(cJSON_IsString(json_str))
            {
                char* ret = String_Copy(cJSON_GetStringValue(json_str));
                cJSON_Delete(json_content);
                return ret;
            }
            else return NULL; /** \todo add item*/
        }
        else return NULL;
    }
    else return NULL;
}

int dhlrc_FileExist(const char *filepos)
{
    FILE* f = fopen(filepos, "rb");
    if(f)
    {
        fclose(f);
        return 1;
    }
    else return 0;
}

cJSON *dhlrc_FileToJSON(const char *pos)
{
    int size;
    char* data = dhlrc_ReadFile(pos, &size);
    if(data)
    {
        cJSON* json_data = cJSON_ParseWithLength(data, size);
        free(data);
        if(json_data) return json_data;
        else return NULL;
    }
    else return NULL;
}
