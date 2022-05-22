#ifndef NBTPROCESS_H
#define NBTPROCESS_H
#ifdef __cplusplus
extern "C"{
#endif

#include "libnbt/nbt.h"


typedef struct Block{
    int len;
    char* name;
    int num;
} Block;

typedef struct BlackList{
    char* name;
    struct BlackList* next;
} BlackList;

typedef struct ReplaceList{
    char* o_name;
    char* r_name;
    struct ReplaceList* next;
} ReplaceList;

// litematica processing stuff

/*************************************************************
 * Region Processing Stuff:
 * Get Region nums and names, processing blocks in the region.
 *************************************************************/
int lite_region_Num(NBT* root);
char** lite_region_Name(NBT* root, int rNum, int* err);
void lite_region_FreeNameArray(char** region, int rNum);
NBT* lite_region_RegionNBT(NBT* root,int r_num);



NBT* lite_region_BlockStatePalette(NBT* root, int r_num);
int lite_region_BlockNum(NBT* root, int r_num);
char** lite_region_BlockName(NBT* root, int r_num ,int bNum);
uint64_t* lite_region_BlockStatesArray(NBT* root, int r_num, int* len);
int lite_region_BlockArrayPos(NBT* root, int r_num, uint64_t index);
int* lite_region_SizeArray(NBT* root,int r_num);
uint64_t lite_region_BlockIndex(NBT* root, int r_num,int x, int y, int z);
NBT* lite_region_SpecificBlockStatePalette(NBT* root,int r_num,int id);
char* lite_region_BlockType(NBT* root,int r_num, int id);
Block* lite_region_BlockList(NBT* root,int r_num, int* rBlockNum);
Block* lite_region_BlockListExtend(NBT* root,int r_num,Block* oBlock, int oBlockNum, int* rBlockNum);
int lite_region_IsBlockWaterlogged(NBT* root,int r_num,int id);
int lite_region_BlockLevel(NBT* root,int r_num,int id);

/*************************************************************
 * BlockList Processing Stuff:
 * Processing BlockList information
 *************************************************************/


void Block_Free(Block* target,int num);
/*
 *  Free the memory used in Block* in case of memory leak.
 */


Block* BlockList_Sort(Block* oBlock, int oBlockNum);
Block* BlockList_InitNewItem(Block* oBlock,char* block_name,int* block_num);
BlackList* BlackList_Init();
/*
 * Init a new blacklist and add some common blacklisted blocks.
 */
void BlackList_Free(BlackList* bl);
BlackList* BlackList_Extend(BlackList* bl, const char* name);
int BlackList_Scan(BlackList* bl,const char* name);

ReplaceList* ReplaceList_Init();
ReplaceList* ReplaceList_Extend(ReplaceList* rl,const char* o_name,const char* r_name);
char* ReplaceList_Replace(ReplaceList* rl,char* o_name);
void ReplaceList_Free(ReplaceList* rl);

#ifdef __cplusplus
}
#endif

#endif // NBTPROCESS_H
