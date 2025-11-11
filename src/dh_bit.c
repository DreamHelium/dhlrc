#include "dh_bit.h"
#include <stdlib.h>
#include <string.h>

void
dh_bit_push_back_val (DhBit *bit, int move_bit, int64_t val)
{
  for (int i = 0; i < move_bit; i++)
    {
      dh_bit_push_back_bit (bit, (val & ((int64_t)1 << i)));
    }
}

void
dh_bit_push_back_bit (DhBit *bit, int b)
{
  if (bit->left_bits == 64)
    {
      bit->pos++;
      bit->array = g_realloc (bit->array, (bit->pos + 1) * sizeof (int64_t));
      bit->array[bit->pos] = 0;
      bit->left_bits = 0;
    }
  bit->array[bit->pos] |= ((int64_t)!!b << bit->left_bits);
  bit->left_bits++;
}

DhBit *
dh_bit_new ()
{
  DhBit *ret = g_malloc (sizeof (DhBit));
  memset (ret, 0, sizeof (DhBit));
  ret->array = g_new0 (int64_t, 1);
  ret->pos = 0;
  ret->left_bits = 0;
  return ret;
}

DhBit *
dh_bit_new_with_len (int len)
{
  return dh_bit_new ();
}

void
dh_bit_free (DhBit *bit)
{
  g_free (bit->array);
  g_free (bit);
}

int64_t *
dh_bit_dup_array (DhBit *bit, int *len)
{
  int size = bit->pos + 1;
  if (len)
    *len = size;
  int64_t *ret = malloc (size * sizeof (int64_t));
  memcpy (ret, bit->array, size * sizeof (int64_t));
  return ret;
}