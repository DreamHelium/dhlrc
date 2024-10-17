#ifndef MANAGE_WINDOW_H
#define MANAGE_WINDOW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define DH_TYPE_MANAGE_WINDOW dh_manage_window_get_type()
G_DECLARE_FINAL_TYPE(DhManageWindow, dh_manage_window, DH, MANAGE_WINDOW, GtkWindow)

GtkWidget* dh_manage_window_new(GtkWidget* parent_window);

G_END_DECLS

#endif /* MANAGE_WINDOW_H */