#ifndef LRC_LIST_H
#define LRC_LIST_H

#ifdef __cplusplus
extern "C" {
#endif



typedef struct ItemList{
    int len;
    char* name;
    int num;
    int collectd;
    struct ItemList* next;
} ItemList;

typedef struct BlackList{
    char* name;
    struct BlackList* next;
} BlackList;

typedef struct ReplaceList{
    char* o_name;
    char* r_name;
    struct ReplaceList* next;
} ReplaceList;




void ItemList_Free(ItemList* target);
void ItemList_Sort(ItemList **oBlock);
int ItemList_InitNewItem(ItemList **oBlock, char* block_name);
int ItemList_AddNum(ItemList* bl, int num , char* block_name);
int ItemList_ScanRepeat(ItemList* bl,char* block_name);
int ItemList_DeleteItem(ItemList** bl,char* block_name);
void ItemList_DeleteZeroItem(ItemList** bl);
int ItemList_Combine(ItemList** dest,ItemList* src);

ItemList* ItemList_Init(char* block_name);
BlackList* BlackList_Init();
void BlackList_Free(BlackList* bl);
BlackList* BlackList_Extend(BlackList* bl, const char* name);
int BlackList_Scan(BlackList* bl,const char* name);



ReplaceList* ReplaceList_Init();
ReplaceList* ReplaceList_Extend(ReplaceList* rl,const char* o_name,const char* r_name);
char* ReplaceList_Replace(ReplaceList* rl,char* o_name);
void ReplaceList_Free(ReplaceList* rl);

//only for debug
//int Test();


#ifdef __cplusplus
}
#endif
#endif // LRC_LIST_H
