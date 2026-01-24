#ifndef DHLRC_NBTVEC_H
#define DHLRC_NBTVEC_H

#ifdef __cplusplus
extern "C"
{
#endif

  /* This file is the output symbol of libnbt_to_vec.so */
  void* file_to_nbt_vec(const char* filename);
  void nbt_vec_free (void* vec);

#ifdef __cplusplus
}
#endif

#endif // DHLRC_NBTVEC_H
