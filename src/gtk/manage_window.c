#include "manage_window.h"
#include "../translation.h"

static GtkWidget* make_button(const char* icon, const char* mn)
{
    GtkWidget* button = gtk_button_new_with_mnemonic(mn);
    gtk_button_set_icon_name(GTK_BUTTON(button), icon);
    g_free((void*)mn);
    return button;
}

struct _DhManageWindow
{
    GtkWindow parent_instance;

    GtkWidget* column_view;

};

G_DEFINE_TYPE(DhManageWindow, dh_manage_window, GTK_TYPE_WINDOW)

static void
dh_manage_window_class_init(DhManageWindowClass* klass)
{

}

static void
dh_manage_window_init(DhManageWindow* self)
{
    GtkWindow* window = GTK_WINDOW(self);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    GtkWidget* button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget* add_button = make_button("list-add", R(_("&Add")));
    GtkWidget* remove_button = make_button("list-remove", R(_("&Remove")));
    GtkWidget* refresh_button = make_button("system-reboot", R(_("R&efresh")));
    GtkWidget* save_button = make_button("document-save", R(_("&Save")));
    gtk_box_append(GTK_BOX(button_box), add_button);
    gtk_box_append(GTK_BOX(button_box), remove_button);
    gtk_box_append(GTK_BOX(button_box), refresh_button);
    gtk_box_append(GTK_BOX(button_box), save_button);
    gtk_widget_set_halign(button_box, GTK_ALIGN_END);

    gtk_box_append(GTK_BOX(box), button_box);
    self->column_view = gtk_column_view_new(NULL);
    gtk_box_append(GTK_BOX(box), self->column_view);

    char* close_str = R(_("&Close"));
    GtkWidget* close_button = gtk_button_new_with_mnemonic(close_str);
    g_free(close_str);
    gtk_widget_set_halign(close_button, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(box), close_button);
    gtk_widget_set_valign(box, GTK_ALIGN_FILL);

    gtk_window_set_child(window, box);
}

GtkWidget* dh_manage_window_new(GtkWidget* parent_window)
{
    return g_object_new(DH_TYPE_MANAGE_WINDOW, "modal", TRUE, 
                        "transient-for", GTK_WINDOW(parent_window), NULL);
}