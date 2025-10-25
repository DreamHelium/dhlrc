/* This header just shows what functions we have
 * For usage, see conv_feature.h */

#ifndef MAIN_CONV_H
#define MAIN_CONV_H

#include "../feature/dh_module.h"
#include "../region.h"
#include "dh_string_util.h"

typedef void (*DhProgressFullSet) (void *, int, const char *);

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
    extern void init (DhModule *module);
    extern void *lite_instance_ptr_new_from_region_full (Region *region,
                                                         gboolean temp_root,
                                                         int lite_version);
    extern void *nbt_instance_ptr_new_from_region_full (Region *region,
                                                        gboolean temp_root,
                                                        gboolean ignore_air);
    extern void *nbt_instance_ptr_new_from_region_real_full (
        Region *region, gboolean temp_root, gboolean ignore_air,
        DhProgressFullSet set_func, void *main_klass,
        GCancellable *cancellable);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_CONV_H */