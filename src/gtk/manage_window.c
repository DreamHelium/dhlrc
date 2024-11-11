#include "manage_window.h"
#include "../translation.h"
#include "glib-object.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "gtk/gtkshortcut.h"

enum{
    ADD,
    REMOVE,
    REFRESH,
    SAVE,
    SHOW,
    CLOSE,
    OK,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

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

    GtkWidget* list_view;

};

G_DEFINE_TYPE(DhManageWindow, dh_manage_window, GTK_TYPE_WINDOW)

static void
dh_manage_window_class_init(DhManageWindowClass* klass)
{
    GObjectClass* object_klass = (GObjectClass*)klass;
    signals[ADD] = g_signal_new("add", G_TYPE_FROM_CLASS(object_klass), G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 
                                0, NULL, NULL, NULL, G_TYPE_NONE, 1, GTK_TYPE_LIST_VIEW);
    signals[REMOVE] = g_signal_new("remove", G_TYPE_FROM_CLASS(object_klass), G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 
                                0, NULL, NULL, NULL, G_TYPE_NONE, 1, GTK_TYPE_BITSET);
}

static void
emit_add (GtkButton* self,
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
    g_signal_emit(parent_window, signals[ADD], 0, user_data);
}

static GtkBitset*
get_selection(GtkListView* view)
{
    GtkSelectionModel* model = gtk_list_view_get_model(view);
    GtkBitset* bitset = gtk_selection_model_get_selection(model);
    return bitset;
}

static void
emit_remove (GtkButton* self, gpointer user_data)
{
    GtkBitset* bitset = get_selection(GTK_LIST_VIEW(user_data));

    GtkWidget* parent_window = GTK_WIDGET(self);
    gboolean is_window = FALSE;
    while(!is_window)
    {
        parent_window = gtk_widget_get_parent(GTK_WIDGET(parent_window));
        if(GTK_IS_WINDOW(parent_window))
            is_window = TRUE;
    }
    g_signal_emit(parent_window, signals[REMOVE], 0, bitset);
}

static void
dh_manage_window_init(DhManageWindow* self)
{
    GtkWindow* window = GTK_WINDOW(self);

    gtk_window_set_default_size(GTK_WINDOW(window), 400, 400);
    gtk_window_set_hide_on_close(GTK_WINDOW(self), TRUE);

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
    self->list_view = gtk_list_view_new(NULL, NULL);

    gtk_box_append(GTK_BOX(box), self->list_view);
    gtk_widget_set_hexpand(self->list_view, TRUE);
    gtk_widget_set_vexpand(self->list_view, TRUE);

    GtkWidget* two_button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    char* ok_str = R(_("&OK"));
    GtkWidget* ok_button = gtk_button_new_with_mnemonic(ok_str);
    g_free(ok_str);
    char* close_str = R(_("&Close"));
    GtkWidget* close_button = gtk_button_new_with_mnemonic(close_str);
    g_free(close_str);
    gtk_box_append(GTK_BOX(two_button_box), ok_button);
    gtk_box_append(GTK_BOX(two_button_box), close_button);
    gtk_widget_set_halign(two_button_box, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(box), two_button_box);
    gtk_widget_set_valign(box, GTK_ALIGN_FILL);

    gtk_widget_set_margin_bottom(box, 10);
    gtk_widget_set_margin_top(box, 10);
    gtk_widget_set_margin_start(box, 10);
    gtk_widget_set_margin_end(box, 10);

    gtk_window_set_child(window, box);

    g_signal_connect(add_button, "clicked", G_CALLBACK(emit_add), self->list_view);
    g_signal_connect(remove_button, "clicked", G_CALLBACK(emit_remove), self->list_view);
}

GtkWidget* dh_manage_window_new(GtkWidget* parent_window)
{
    return g_object_new(DH_TYPE_MANAGE_WINDOW, "modal", TRUE, 
                        "transient-for", GTK_WINDOW(parent_window), NULL);
}