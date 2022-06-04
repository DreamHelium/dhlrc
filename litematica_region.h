#ifndef LITEMATICA_REGION_H
#define LITEMATICA_REGION_H
#ifdef __cplusplus
extern "C"{
#endif

#include "libnbt/nbt.h"
#include "lrc_list.h"

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
ItemList* lite_region_ItemList(NBT* root,int r_num);
ItemList* lite_region_ItemListExtend(NBT* root, int r_num, ItemList *oBlock);
int lite_region_IsBlockWaterlogged(NBT* root,int r_num,int id);
int lite_region_BlockLevel(NBT* root,int r_num,int id);


#ifdef __cplusplus
}
#endif

#endif // LITEMATICA_REGION_H
