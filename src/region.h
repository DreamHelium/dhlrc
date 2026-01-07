#ifndef DHLRC_REGION_H
#define DHLRC_REGION_H
#include <cstdint>

#ifdef __cplusplus
extern "C"
{
#endif

  /* This file is the output symbol of libregion_rs.so */
  void *region_new ();
  void region_free (void *region);
  const char *region_set_time (void *region, int64_t create_time,
                               int64_t modify_time);
  void string_free (const char *string);
  int64_t region_get_create_timestamp (void *region);

#ifdef __cplusplus
}
#endif

#endif // DHLRC_REGION_H
