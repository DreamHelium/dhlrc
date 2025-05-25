/* This header just shows what functions we have
 * For usage, see conv_feature.h */

#ifndef MAIN_CONV_H
#define MAIN_CONV_H

#include "../region.h"
#include "dh_string_util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    extern int start_point (int argc, char **argv, const char *prpath);
    extern void *nbt_instance_ptr_new_from_region (Region *region,
                                                   gboolean temp_root);
    extern void *lite_instance_ptr_new_from_region (Region *region,
                                                    gboolean temp_root);
    extern void *new_schema_instance_ptr_new_from_region (Region *region,
                                                          gboolean temp_root);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_CONV_H */