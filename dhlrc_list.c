#include "dhlrc_list.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cjson/cJSON.h>

void ItemList_Free(ItemList* target)
{
    if(target->next)
        ItemList_Free(target->next);
    target->next = NULL;
    free(target->name);
    target->name = NULL;
    free(target);
    target = NULL;
}

void ItemList_Sort(ItemList** oBlock)
{
    ItemList* head = *oBlock;
    ItemList* prev = NULL;
    ItemList* before_head = NULL;
    while(head)
    {
        ItemList* head_next = head->next;
        ItemList* temp = NULL;             // store bl before max num
        ItemList* temp_next = NULL;        // store bl on max num, then exchange to max num
        int max = head->num;
        ItemList* forward = head;  // staging forward

        while(forward->next)        // scan all
        {
            if(forward->next->num > max)
            {
                max = forward->next->num;
                temp = forward;
            }
            forward = forward->next;
        }
        // temp stores bl before max num, then a before head is created
        // it would be the before of the origin head
        if(temp)
        {
            if(temp == head) // two are besides
            {
                ItemList* temp_next_next = temp->next->next; //actually it's the head's next
                before_head = temp->next;   //actually head next, the max pos
                before_head->next = head;   //head is smaller one
                head->next = temp_next_next;
            }
            else
            {
                ItemList* temp_next_next = temp->next->next;
                temp_next = temp->next;
                before_head = temp_next; //it's on max bl.
                before_head->next = head_next;  // max num next point to the head's next since head would be moved
                temp->next = head; // head exchange to temp's next
                head->next = temp_next_next; // head's next is origin temp's next next
            }
            if(!prev)
            {
                *oBlock = before_head;
            }
            head = before_head;
            if(prev)
                prev->next = head;
        }
        prev = head;
        head = head->next;
    }
}

int ItemList_InitNewItem(ItemList** oBlock,char* block_name)
{
    if(*oBlock == NULL)
    {
        *oBlock = ItemList_Init(block_name);
        if(*oBlock) return 0;
        else return -1;
    }
    ItemList* next_item = *oBlock;
    while(next_item->next)
        next_item = next_item->next;
    next_item->next = (ItemList*)malloc(sizeof(ItemList));
    if(next_item->next)
    {
        next_item = next_item->next;
        next_item->len = strlen(block_name) + 1;
        next_item->name = (char*)malloc(next_item->len * sizeof(char));
        strcpy(next_item->name,block_name);
        next_item->num = 0;
        next_item->collectd = 0;
        next_item->next = NULL;
        return 0;
    }
    else
    {
        ItemList_Free(*oBlock);
        *oBlock = NULL;
        return -1;
    }

}

ItemList *ItemList_Init(char* block_name)
{
    ItemList* bl = (ItemList*)malloc(sizeof(ItemList));
    if(bl)
    {
        bl->len = strlen(block_name) + 1;
        bl->name = (char*)malloc(bl->len * sizeof(char));
        strcpy(bl->name,block_name);
        bl->num = 0;
        bl->collectd = 0;
        bl->next = NULL;
        return bl;
    }
    else
    {
        free(bl);
        return NULL;
    }
}

int ItemList_AddNum(ItemList* bl,int num,char* block_name)
{
    while(bl)
    {
        if(!strcmp(bl->name,block_name))
        {
            bl->num += num;
            return 0;
        }
        bl = bl->next;
    }
    return -1;
}

int ItemList_ScanRepeat(ItemList* bl,char* block_name)
{
    while(bl)
    {
        if(!strcmp(bl->name,block_name))
            return 1;
        bl = bl->next;
    }
    return 0;
}

int ItemList_DeleteItem(ItemList** bl,char* block_name)
{
    ItemList* head = *bl;
    while(head)
    {
        if(head == *bl && !strcmp(head->name,block_name))
        {
            free(head->name);
            head->name = NULL;
            ItemList* head_next = head->next;
            free(head);
            head = NULL;
            *bl = head_next;
            return 0;
        }
        if(head->next)
        {
            if(!strcmp(head->next->name,block_name))
            {
                ItemList* head_next = head->next;
                ItemList* head_next_next = head->next->next;
                free(head_next->name);
                head_next->name = NULL;
                free(head_next);
                head_next = NULL;
                head->next = head_next_next;
                return 0;
            }
        }
        head = head->next;
    }
    return -1;
}

void ItemList_DeleteZeroItem(ItemList** bl)
{
    ItemList* head = *bl;
    ItemList* prev = NULL;
    while(head)
    {
        if(head->num == 0)
        {
            free(head->name);
            head->name = NULL;
            ItemList* head_next = head->next;
            free(head);
            head = NULL;
            if(!prev)
            {
                head = head_next;
                *bl = head;
            }
            else
            {
                head = head_next;
                prev->next = head;
            }
        }
        else
        {
            prev = head;
            head = head->next;
        }
    }
}

int ItemList_Combine(ItemList** dest, ItemList* src)
{
    ItemList* s = src;
    while(s)
    {
        if(!ItemList_ScanRepeat(*dest,s->name))
            if(ItemList_InitNewItem(dest,s->name))
                return -1;
        ItemList_AddNum(*dest,s->num,s->name);
        s = s->next;
    }
    return 0;
}

BlackList* BlackList_Init()
{
    BlackList* bl = (BlackList*) malloc(sizeof(BlackList));
    bl->name = (char*)malloc(5*sizeof(char));
    bl->name = "none";
    bl->next = NULL;
    FILE* f = fopen("config/ignored_blocks.json","rb");
    if(f)
    {
        fseek(f,0,SEEK_END);
        int size = ftell(f);
        fseek(f,0,SEEK_SET);
        char* data = (char*)malloc(size* sizeof(char));
        fread(data,1,size,f);
        fclose(f);
        cJSON* black_list = cJSON_ParseWithLength(data,size);
        if(cJSON_IsArray(black_list))
        {
            int n = cJSON_GetArraySize(black_list);
            for(int i = 0 ; i < n ; i++)
            {
                char* item = cJSON_GetStringValue(cJSON_GetArrayItem(black_list,i));
                BlackList_Extend(bl,item);
            }
        }
        free(data);
        cJSON_free(black_list);
        return bl;
    }
    else
    {
    BlackList_Extend(bl,"minecraft:air");
    BlackList_Extend(bl,"minecraft:piston_head");
    BlackList_Extend(bl,"minecraft:fire");
    BlackList_Extend(bl,"minecraft:soul_fire");
    BlackList_Extend(bl,"minecraft:bubble_column");
    return bl;
    }
}

BlackList* BlackList_Extend(BlackList* bl,const char* name)
{
    if(!strcmp(bl->name,"none"))
    {
        bl->name = (char*)malloc((strlen(name) + 1) * sizeof(char));
        strcpy(bl->name,name);
        return bl;
    }
    else
    {
        BlackList* bld = bl;
        while(bld->next != NULL)
            bld = bld->next;
        bld->next = (BlackList*)malloc(sizeof(BlackList));
        bld->next->name = (char*)malloc((strlen(name) + 1) * sizeof(char));
        strcpy(bld->next->name,name);
        bld->next->next = NULL;
        return bl;
    }
}

void BlackList_Free(BlackList* bl)
{
    if(bl->next)
    {
        BlackList_Free(bl->next);
        bl->next = NULL;
    }
    free(bl->name);
    bl->name = NULL;
    free(bl);
    bl = NULL;
}

int BlackList_Scan(BlackList* bl,const char* name)
{
    if(!bl)
        return 0;
    else
    {
        BlackList* bld = bl;
        while(bld)
        {
            if(!strcmp(name,bld->name)) return 1;
            bld = bld->next;
        }
        return 0;
    }
}

ReplaceList* ReplaceList_Init()
{
    ReplaceList* rl = (ReplaceList*)malloc(sizeof(ReplaceList));
    rl->o_name = (char*)malloc(5 * sizeof(char));
    rl->r_name = (char*)malloc(5 * sizeof(char));
    strcpy(rl->o_name,"none");
    strcpy(rl->r_name,"none");
    rl->next = NULL;
    FILE* f = fopen("config/block_items.json","rb");
    if(f)
    {
        fseek(f,0,SEEK_END);
        int size = ftell(f);
        fseek(f,0,SEEK_SET);
        char* data = (char*)malloc(size* sizeof(char));
        fread(data,1,size,f);
        fclose(f);
        cJSON* rlist = cJSON_ParseWithLength(data,size)->child;
        while(rlist)
        {
            if(cJSON_IsString(rlist))
                ReplaceList_Extend(rl,rlist->string,rlist->valuestring);
            rlist = rlist->next;
        }
        cJSON_free(rlist);
        free(data);
        return rl;
    }
    else
    {
        ReplaceList_Extend(rl,"minecraft:water","minecraft:water_bucket");
        ReplaceList_Extend(rl,"minecraft:lava","minecraft:lava_bucket");
        ReplaceList_Extend(rl,"minecraft:redstone_wall_torch","minecraft:redstone_torch");
        return rl;
    }
}

ReplaceList* ReplaceList_Extend(ReplaceList* rl,const char* o_name,const char* r_name)
{
    if(!strcmp(rl->o_name,"none"))
    {
        rl->o_name = (char*)malloc((strlen(o_name)+1) * sizeof(char));
        rl->r_name = (char*)malloc((strlen(r_name)+1) * sizeof(char));
        strcpy(rl->o_name,o_name);
        strcpy(rl->r_name,r_name);
        return rl;
    }
    else
    {
        ReplaceList* rld = rl;
        while(rld->next) rld = rld->next;
        rld->next = (ReplaceList*)malloc(sizeof(ReplaceList));
        rld = rld->next;
        rld->o_name = (char*)malloc((strlen(o_name)+1) * sizeof(char));
        rld->r_name = (char*)malloc((strlen(r_name)+1) * sizeof(char));
        strcpy(rld->o_name,o_name);
        strcpy(rld->r_name,r_name);
        rld->next = NULL;
        return rl;
    }
}

char* ReplaceList_Replace(ReplaceList* rl,char* o_name)
{
    if(rl)
    {
        ReplaceList* rld = rl;
        while(rld)
        {
            if(!strcmp(o_name,rld->o_name))
            {
                return rld->r_name;
            }
            else rld = rld->next;
        }
    }
    return o_name;
}

void ReplaceList_Free(ReplaceList* rl)
{
    if(rl->next)
        ReplaceList_Free(rl->next);
    rl->next = NULL;
    free(rl->o_name);
    rl->o_name = NULL;
    free(rl->r_name);
    rl->r_name = NULL;
    free(rl);
    rl = NULL;
}


int ItemList_GetItemNum(ItemList *il, char *item_name)
{
    ItemList* ild = il;
    while(ild)
    {
        if(!strcmp(item_name,ild->name))
            return ild->num;
        ild = ild->next;
    }
    return -1;
}
