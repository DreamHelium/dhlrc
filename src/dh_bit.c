#include "dh_bit.h"
#include <stdlib.h>
#include <string.h>

void dh_bit_push_back_val(DhBit* bit, int move_bit, int64_t val)
{
    for(int i = 0 ; i < move_bit ; i++)
    {
        dh_bit_push_back_bit(bit, (val & ((int64_t)1 << i)));
    }
}

void dh_bit_push_back_bit(DhBit* bit, int b)
{
    if(bit->bits % 64 == 0)
    {
        bit->array = realloc(bit->array, (bit->bits / 64 + 1) * sizeof(int64_t));
        bit->array[bit->bits / 64] = 0;
    }
    bit->array[bit->bits / 64] |= ((int64_t)!!b << (bit->bits % 64));
    bit->bits++;
}

DhBit* dh_bit_new()
{
    DhBit* ret = malloc(sizeof(DhBit));
    if(ret)
    {
        memset(ret, 0, sizeof(DhBit));
        return ret;
    }
    else abort();
}

void dh_bit_free(DhBit* bit)
{
    free(bit->array);
    free(bit);
}

int64_t* dh_bit_dup_array(DhBit* bit, int* len)
{
    int size = bit->bits / 64 + (bit->bits % 64 ? 1 : 0);
    if(len) *len = size;
    int64_t* ret = malloc(size * sizeof(int64_t));
    memcpy(ret, bit->array, size * sizeof(int64_t));
    return ret;
}