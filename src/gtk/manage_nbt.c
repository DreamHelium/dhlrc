#include "manage_nbt.h"
#include "manage_window.h"

static GtkWidget* window = NULL;

void manage_nbt(GtkButton* self,
       gpointer user_data)
{
    GtkWidget* parent_window = GTK_WIDGET(self);
    gboolean is_window = FALSE;
    while(!is_window)
    {
        parent_window = gtk_widget_get_parent(GTK_WIDGET(parent_window));
        if(GTK_IS_WINDOW(parent_window))
            is_window = TRUE;
    }
    if(!window) window = dh_manage_window_new(parent_window);
    gtk_window_present(GTK_WINDOW(window));
}