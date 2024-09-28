#include <gtk/gtk.h>
#include "../translation.h"
#include "gtk/gtkshortcut.h"

static GtkWidget* window;

int verbose_level;

static GtkWidget*
make_box();

static GtkWidget*
make_title_bar();

static void
print_hello (GtkWidget *widget,
             gpointer   data)
{
  g_print ("Hello World\n");
}

static void
close_window (GtkWidget *widget,
              gpointer data)
{
  gtk_window_close(GTK_WINDOW(window));
}

static void
activate (GtkApplication *app,
          gpointer        user_data)
{
  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), _("Litematica reader"));

  GtkWidget* header_bar = make_title_bar();

  GtkWidget* box = make_box();

  gtk_window_set_child (GTK_WINDOW (window), box);
  gtk_window_set_titlebar(GTK_WINDOW(window), header_bar);

  gtk_window_present (GTK_WINDOW (window));
}

static GtkWidget*
make_box()
{
  GtkWidget *box;
  GtkWidget *ok_button;
  GtkWidget *close_button;

  /* This is full box */
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  /* Let's put widget here! */
  GtkWidget *first_label = gtk_label_new (_("First, you need to create a Region struct."));
  gtk_box_append (GTK_BOX(box), first_label);
  GtkWidget *region_button = gtk_button_new_with_label(_("Create Region from NBT"));
  gtk_box_append (GTK_BOX(box), region_button);

  GtkWidget *second_label = gtk_label_new(_("Second, you can do lots of things with the Region."));
  gtk_box_append (GTK_BOX(box), second_label);

  /* Lots of options and choices */


  return box;
}

static GtkWidget*
make_title_bar()
{
  GtkWidget *header_bar;
  header_bar = gtk_header_bar_new();

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *region_button = gtk_toggle_button_new_with_label(_("Region"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(region_button), TRUE);
  gtk_box_append(GTK_BOX(box), region_button);

  gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header_bar), box);

  return header_bar;
}

int
main (int    argc,
      char **argv)
{
  translation_init();
  GtkApplication *app;
  int status;

  app = gtk_application_new ("cn.dh.dhlrc", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}