#ifndef DH_BIT_H
#define DH_BIT_H

#include <glib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct DhBit
  {
    int64_t *array;
    int pos;
    int left_bits;
  } DhBit;

  DhBit *dh_bit_new ();
  DhBit *dh_bit_new_with_len (int len);
  void dh_bit_push_back_val (DhBit *bit, int move_bit, int64_t val);
  void dh_bit_push_back_bit (DhBit *bit, int b);
  int64_t *dh_bit_dup_array (DhBit *bit, int *len);
  void dh_bit_free (DhBit *bit);

#ifdef __cplusplus
}
#endif

#endif /* DH_BIT_H */