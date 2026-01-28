#ifndef DHLRC_NBTVEC_H
#define DHLRC_NBTVEC_H

#ifdef __cplusplus
extern "C"
{
#endif

  /* This file is the output symbol of libnbt_to_vec.so */
  typedef void (*ProgressFunc) (void *, int, const char *, const char *);

  void *file_to_nbt_vec (const char *filename, ProgressFunc progress_func,
                         void *main_klass);
  void nbt_vec_free (void *vec);
  void nbt_vec_to_file (const void *vec, const char *filename);

#ifdef __cplusplus
}
#endif

#endif // DHLRC_NBTVEC_H
