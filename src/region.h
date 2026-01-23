#ifndef DHLRC_REGION_H
#define DHLRC_REGION_H
#include <cstdint>
#include <cstdlib>

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
  int64_t region_get_modify_timestamp (void *region);
  void region_get_size (void *region);
  /* Note: The getter should not return nullptr unless error occurred */
  const char *region_get_name (void *region);
  const char *region_set_name (void *region, const char *name);
  const char *region_get_description (void *region);
  const char *region_set_description (void *region, const char *description);
  const char *region_get_author (void *region);
  const char *region_set_author (void *region, const char *author);

  int32_t region_get_index (void *region, int32_t x, int32_t y, int32_t z);

  uint32_t region_get_data_version (void *region);
  void region_set_data_version (void *region, uint32_t version);
  void region_set_size (void *region, int32_t x, int32_t y, int32_t z);
  int32_t region_get_x (void *region);
  int32_t region_get_y (void *region);
  int32_t region_get_z (void *region);
  uint32_t region_get_block_id_by_index (void *region, size_t index);
  const char *region_get_palette_id_name (void *region, size_t id);
  size_t region_get_palette_property_len (void *region, size_t id);
  size_t region_get_palette_len (void *region);
  const char *region_get_palette_property_name (void *region, size_t id,
                                                size_t index);
  const char *region_get_palette_property_data (void *region, size_t id,
                                                size_t index);
  const char *region_set_palette_property_name_and_data (void *region,
                                                         size_t id,
                                                         size_t index,
                                                         const char *name,
                                                         const char *data);

  typedef void (*ProgressFunc) (void *, int, const char *, const char *);
  void *file_try_uncompress (const char *filename, ProgressFunc progress_func,
                             void *main_klass, int *failed,
                             const void *cancel_flag);
  void vec_free (void *vec);
  char *vec_to_cstr (void *vec);
  const void *cancel_flag_new ();
  int cancel_flag_is_cancelled (const void *cancel_flag);
  void cancel_flag_cancel (const void *cancel_flag);
  void cancel_flag_destroy (const void *cancel_flag);
  const void *cancel_flag_clone (const void *cancel_flag);
  void *get_system_info_object ();
  uint64_t get_free_memory (void *system);
  void system_info_object_free (void *system);
  const void *region_get_block_entity (void *region, uint32_t index);
  size_t nbt_vec_get_len (const void *nbt);
  const char *nbt_vec_get_key (const void *nbt, size_t index);
  const char *nbt_vec_get_value_type (const void *nbt, size_t index);
  const char *nbt_vec_get_value_string (const void *nbt, size_t index);
  int32_t nbt_vec_get_value_type_int (const void *nbt, size_t index);
  const void *nbt_vec_get_value_to_child (const void *nbt, size_t index);
  const void *nbt_vec_get_value_list_to_child (const void *nbt, size_t index);

#ifdef __cplusplus
}
#endif

#endif // DHLRC_REGION_H
