#include "input_dialog.h"
#include "gtk/gtk.h"
#include <string.h>

static char* text = NULL;
static GtkWidget* dialog;
static GtkWidget* input_line;
static GtkWidget* ok_button;
static GtkWidget* close_button;

static void set_text(GtkButton* self, gpointer user_data)
{
    if(self == GTK_BUTTON(ok_button))
    {
        GtkEntryBuffer* buffer = gtk_entry_get_buffer(GTK_ENTRY(input_line));
        text = g_strdup(gtk_entry_buffer_get_text(buffer));
    }
    else text = NULL;
    gtk_window_destroy(GTK_WINDOW(dialog));
}

GtkWidget* dh_input_dialog_new(const char* title, const char* content, const char* tip, const char* default_input, GtkWindow* parent)
{
    dialog = gtk_window_new();
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_title(GTK_WINDOW(dialog), title);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget* label = gtk_label_new(content);

    input_line = gtk_entry_new();
    GtkEntryBuffer* buffer = gtk_entry_buffer_new(default_input, strlen(default_input));
    gtk_entry_set_placeholder_text(GTK_ENTRY(input_line), tip);
    gtk_entry_set_buffer(GTK_ENTRY(input_line), buffer);

    GtkWidget* button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    ok_button = gtk_button_new_with_label("OK");
    close_button = gtk_button_new_with_label("Close");
    gtk_box_append(GTK_BOX(button_box), ok_button);
    gtk_box_append(GTK_BOX(button_box), close_button);

    gtk_box_append(GTK_BOX(box), label);
    gtk_box_append(GTK_BOX(box), input_line);
    gtk_box_append(GTK_BOX(box), button_box);

    gtk_window_set_child(GTK_WINDOW(dialog), box);

    g_signal_connect(ok_button, "clicked", G_CALLBACK(set_text), NULL);
    g_signal_connect(close_button, "clicked", G_CALLBACK(set_text), NULL);

    return dialog;
}

char* dh_input_dialog_get_text()
{
    return text;
}