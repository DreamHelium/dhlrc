#include <gtk/gtk.h>
#include "../translation.h"
#include "../nbt_info.h"
#include "glib.h"
#include "input_dialog.h"
#include "manage_nbt.h"
#include "../region_info.h"
#include "../il_info.h"
#include "../config.h"

static GtkWidget* window;
static GtkWidget* region_box;
static GtkWidget* nbt_box;
static GtkWidget* item_list_box;
static GtkWidget* region_button;
static GtkWidget* nbt_button;
static GtkWidget* item_list_button;

int verbose_level;

static GtkWidget*
make_region_box();

static GtkWidget*
make_nbt_box();

static GtkWidget*
make_item_list_box();

static GtkWidget*
make_title_bar();

static void
debug (GtkButton* self,
       gpointer user_data)
{
  char* input = dh_input_dialog_new_no_func("test", "test", "test", "test", GTK_WINDOW(window));
#ifdef GDK_AVAILABLE_IN_4_10
  GtkAlertDialog* dialog = gtk_alert_dialog_new("%s", input? input : "NULL");
  gtk_alert_dialog_show(dialog, GTK_WINDOW(window));
#else
  GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(window), 
  GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "%s", input ? input : "NULL");
  g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
#endif
}

static void
title_bar_button_toggled (GtkToggleButton* self,
                          gpointer user_data)
{
  GtkWidget *child = gtk_window_get_child(GTK_WINDOW(window));
  if(self == GTK_TOGGLE_BUTTON(region_button)) {
    region_box = make_region_box();
    gtk_window_set_child(GTK_WINDOW(window), region_box);
  }
  else if(self == GTK_TOGGLE_BUTTON(nbt_button)) {
    nbt_box = make_nbt_box();
    gtk_window_set_child(GTK_WINDOW(window), nbt_box);
  }
  else if(self == GTK_TOGGLE_BUTTON(item_list_button)) {
    item_list_box = make_item_list_box();
    gtk_window_set_child(GTK_WINDOW(window), item_list_box);
  }
}

static void
activate (GtkApplication *app,
          gpointer        user_data)
{
  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), _("Litematica reader"));
  gtk_window_set_default_size(GTK_WINDOW(window), 400, 400);

  GtkWidget* header_bar = make_title_bar();

  nbt_box = make_nbt_box();

  gtk_window_set_child (GTK_WINDOW (window), nbt_box);
  gtk_window_set_titlebar(GTK_WINDOW(window), header_bar);

  gtk_window_present (GTK_WINDOW (window));
}

static GtkWidget*
make_region_box()
{
  GtkWidget *box;

  /* This is full box */
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  /* Let's put widget here! */
  GtkWidget *first_label = gtk_label_new (_("First, you need to create a Region struct."));
  gtk_box_append (GTK_BOX(box), first_label);
  char* region_str = R(_("&Create Region from NBT"));
  GtkWidget *region_button = gtk_button_new_with_mnemonic(region_str);
  g_free(region_str);
  gtk_box_append (GTK_BOX(box), region_button);

  GtkWidget *second_label = gtk_label_new(_("Second, you can do lots of things with the Region."));
  gtk_box_append (GTK_BOX(box), second_label);

  /* Lots of options and choices */
  char* gen_item_list_str = R(_("&Generate item list"));
  GtkWidget *gen_item_list_btn = gtk_button_new_with_mnemonic (gen_item_list_str);
  g_free(gen_item_list_str);

  char* block_reader_str = R(_("&Block reader"));
  GtkWidget *block_reader_btn = gtk_button_new_with_mnemonic (block_reader_str);
  g_free(block_reader_str);
  gtk_box_append (GTK_BOX(box), gen_item_list_btn);
  gtk_box_append (GTK_BOX(box), block_reader_btn);

  gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

  gtk_widget_set_margin_bottom(box, 20);
  gtk_widget_set_margin_top(box, 20);
  gtk_widget_set_margin_start(box, 20);
  gtk_widget_set_margin_end(box, 20);
  gtk_box_set_spacing(GTK_BOX(box), 20);
  return box;
}

static GtkWidget*
make_nbt_box()
{
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  GtkWidget *first_label = gtk_label_new (_("Let's load a NBT file here!"));
  gtk_box_append (GTK_BOX(box), first_label);

  char* manage_str = R(_("&Manage NBT"));
  GtkWidget *manage_btn = gtk_button_new_with_mnemonic (manage_str);
  g_free(manage_str);
  gtk_box_append (GTK_BOX(box), manage_btn);
  g_signal_connect(manage_btn, "clicked", G_CALLBACK(manage_nbt), NULL);

  GtkWidget *second_label = gtk_label_new(_("You can do things below for the NBT file."));
  gtk_box_append (GTK_BOX(box), second_label);
  char* nbt_reader_str = R(_("N&BT Reader"));
  GtkWidget *nbt_reader_btn = gtk_button_new_with_mnemonic(nbt_reader_str);
  g_free(nbt_reader_str);
  gtk_box_append (GTK_BOX(box), nbt_reader_btn);
  gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

  gtk_widget_set_margin_bottom(box, 20);
  gtk_widget_set_margin_top(box, 20);
  gtk_widget_set_margin_start(box, 20);
  gtk_widget_set_margin_end(box, 20);
  gtk_box_set_spacing(GTK_BOX(box), 20);

  return box;
}

static GtkWidget*
make_item_list_box()
{
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  char* reader_str = R(_("I&tem list reader and modifier"));
  GtkWidget *reader_btn = gtk_button_new_with_mnemonic(reader_str);
  g_free(reader_str);
  char* recipe_str = R(_("R&ecipe combiner"));
  GtkWidget *recipe_btn = gtk_button_new_with_mnemonic(recipe_str);
  g_free(recipe_str);
  gtk_box_append (GTK_BOX(box), reader_btn);
  gtk_box_append (GTK_BOX(box), recipe_btn);

  gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

  gtk_widget_set_margin_bottom(box, 20);
  gtk_widget_set_margin_top(box, 20);
  gtk_widget_set_margin_start(box, 20);
  gtk_widget_set_margin_end(box, 20);
  gtk_box_set_spacing(GTK_BOX(box), 20);

  return box;
}

static GtkWidget*
make_title_bar()
{
  GtkWidget *header_bar;
  header_bar = gtk_header_bar_new();

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  region_button = gtk_toggle_button_new_with_label(_("Region"));
  nbt_button = gtk_toggle_button_new_with_label(_("NBT"));
  item_list_button = gtk_toggle_button_new_with_label(_("Item List"));
#ifdef DH_DEBUG_IN_IDE
  GtkWidget* debug_button = gtk_toggle_button_new_with_label("debug");
#endif
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(nbt_button), TRUE);
  
  gtk_box_append(GTK_BOX(box), nbt_button);
  gtk_box_append(GTK_BOX(box), region_button);
  gtk_box_append(GTK_BOX(box), item_list_button);
#ifdef DH_DEBUG_IN_IDE
  gtk_box_append(GTK_BOX(box), debug_button);
#endif

  gtk_toggle_button_set_group (GTK_TOGGLE_BUTTON(nbt_button), GTK_TOGGLE_BUTTON(region_button));
  gtk_toggle_button_set_group (GTK_TOGGLE_BUTTON(item_list_button), GTK_TOGGLE_BUTTON(region_button));

  gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header_bar), box);
  g_signal_connect (region_button, "toggled", G_CALLBACK(title_bar_button_toggled), NULL);
  g_signal_connect (nbt_button, "toggled", G_CALLBACK(title_bar_button_toggled), NULL);
  g_signal_connect (item_list_button, "toggled", G_CALLBACK(title_bar_button_toggled), NULL);
#ifdef DH_DEBUG_IN_IDE
  g_signal_connect (debug_button, "toggled", G_CALLBACK(debug), NULL);
#endif

  return header_bar;
}

static void
startup (GApplication* self,
         gpointer user_data
)
{
  translation_init();
  nbt_info_list_init();
  region_info_list_init();
  il_info_list_init();
}

static void app_shutdown (GApplication* self,
                          gpointer user_data
)
{
  nbt_info_list_free();
  region_info_list_free();
  il_info_list_free();
  dh_exit();
  dh_exit1();
}


int
main (int    argc,
      char **argv)
{
  GtkApplication *app;
  int status;

#ifdef GIO_AVAILABLE_ENUMERATOR_IN_2_74
  app = gtk_application_new ("cn.dh.dhlrc", G_APPLICATION_DEFAULT_FLAGS);
#else
  app = gtk_application_new ("cn.dh.dhlrc", G_APPLICATION_FLAGS_NONE);
#endif
  g_signal_connect (app, "startup", G_CALLBACK(startup), NULL);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  g_signal_connect (app, "shutdown", G_CALLBACK(app_shutdown), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);

  g_object_unref (app);

  return status;
}