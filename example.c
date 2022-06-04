#include <stdio.h>
#include "litematica_region.h"
#include <stdint.h>
#include <stdlib.h>
#include "libnbt/nbt.h"
#include <string.h>
#include "recipe_util.h"


int main(int argc,char** argb)
{
    //Test();
    if(argc == 1)
        printf("No input, quiting......\n");
    else
        printf("%s\n",argb[1]);

    FILE* f = fopen(argb[1],"rb");
    //FILE* f = fopen("/path/to/litematic","rb");
    if(!f)
    {
        printf("Read Error, exiting!\n");
        return -10;
    }
    fseek(f,0,SEEK_END);
    int size = ftell(f);
    fseek(f,0,SEEK_SET);

    uint8_t* data = (uint8_t*)malloc(size * sizeof(uint8_t));
    fread(data,1,size,f);
    fclose(f);

    NBT* root = NBT_Parse(data,size);
    if(root) printf("SUCCESS!\n");
    free(data);

    int rNum = lite_region_Num(root);
    //printf("%d\n",rNum);
    int region_err = 0;
    char** region = lite_region_Name(root,rNum,&region_err);
    if(region);
    else
    {
        NBT_Free(root);
        return -1;
    }

    ItemList* blockList0 = NULL;
    for(int i = 0 ; i < rNum ; i++)
    {
        printf("Processing Region %d / %d : %s \n",i + 1,rNum,region[i]);
        blockList0 = lite_region_ItemListExtend(root,i,blockList0);
    }
    ItemList_DeleteZeroItem(&blockList0);
    ItemList_CombineRecipe(&blockList0);
    ItemList_Sort(&blockList0);
    printf("\n\nAfter Sorting Block list: \n");
    ItemList* block_list_read = blockList0;
    for( ; block_list_read ; block_list_read = block_list_read->next)
    {
        char* trans_name = Name_BlockTranslate(block_list_read->name);
        if(trans_name)
            printf("%s,%d\n",trans_name,block_list_read->num);
        else
            printf("%s,%d\n",block_list_read->name,block_list_read->num);
        free(trans_name);
    }
    ItemList_Free(blockList0);
    lite_region_FreeNameArray(region,rNum);
    NBT_Free(root);
    return 0;
}
