#ifndef DHLRC_GLOBAL_VARIANT_H
#define DHLRC_GLOBAL_VARIANT_H

#include <glib.h>

G_BEGIN_DECLS

void dhlrc_global_variant_init ();
void dhlrc_global_variant_clear ();

/* boolean */
void dhlrc_set_ignore_extension_name (gboolean ignore_extension);
/** default to FALSE  \n
 * Ignores the file extension. */
gboolean dhlrc_get_ignore_extension_name ();
void dhlrc_set_ignore_leftover (gboolean ignore_leftover);
gboolean dhlrc_get_ignore_leftover ();

/* PtrArray */
/** The member of the list should be `char*` */
void dhlrc_set_ignore_air_list (GPtrArray *air_list);
const GPtrArray *dhlrc_get_ignore_air_list ();

G_END_DECLS

#endif // DHLRC_GLOBAL_VARIANT_H
