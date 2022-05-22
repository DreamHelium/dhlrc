#include <stdio.h>
#include "nbtProcess.h"
#include <stdint.h>
#include <stdlib.h>
#include "libnbt/nbt.h"
#include <string.h>

int main(int argc,char** argb)
{

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

    int rbN = 0;
    printf("Processing Region 1 / %d : %s \n",rNum,region[0]);
    Block* blockList0 = lite_region_BlockList(root,0,&rbN);
    for(int i = 1 ; i < rNum ; i++)
    {
        printf("Processing Region %d / %d : %s \n",i + 1,rNum,region[i]);
        blockList0 = lite_region_BlockListExtend(root,i,blockList0,rbN,&rbN);
    }
    blockList0 = BlockList_Sort(blockList0,rbN);
    printf("\n\nAfter Sorting Block list: \n");
    for(int i = 0 ; i < rbN ; i++)
    {
        if(blockList0[i].num != 0)
            printf("%-50s,%d\n",blockList0[i].name,blockList0[i].num);
    }
    Block_Free(blockList0,rbN);
    lite_region_FreeNameArray(region,rNum);
    NBT_Free(root);


    return 0;
}
