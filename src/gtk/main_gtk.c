#include <gtk/gtk.h>
#include "../translation.h"
#include "../libnbt/nbt.h"
#include "../nbt_info.h"
#include "glib.h"
#include "input_dialog.h"

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
nbt_open_response (GtkDialog *dialog,
                  int        response)
{
  if (response == GTK_RESPONSE_ACCEPT)
    {
      GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
      GFile* file = gtk_file_chooser_get_file (chooser);
      /* Unfinished */
      char* name = g_file_get_basename(file);
      char* content;
      gsize len;
      g_file_load_contents(file, NULL, &content, &len, NULL, NULL);
      NBT* new_nbt = NBT_Parse(content, len);

      GtkWidget* input_dialog = dh_input_dialog_new(_("Enter Desciption"), _("Please enter description for the NBT."), _("Desciption"), name, GTK_WINDOW(window));
      gtk_window_present(GTK_WINDOW(input_dialog));
      char* description = dh_input_dialog_get_text();

      if(description)
      {
        nbt_info_new(new_nbt, g_date_time_new_now_local(), description);
        g_free(description);
      }
      else
      {
        NBT_Free(new_nbt);
        GtkAlertDialog* dialog = gtk_alert_dialog_new(_("No desciption entered! The NBT will not be added!"));
        gtk_alert_dialog_show(dialog, GTK_WINDOW(window));
      }

      g_object_unref(file);
    }

  gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
load_nbt_file (GtkButton* self,
               gpointer user_data)
{
  GtkFileFilter* filter = gtk_file_filter_new();
  gtk_file_filter_add_pattern(filter, "*.litematic");
  gtk_file_filter_add_pattern(filter, "*.nbt");

  /* This is the code deprecated in 4.10, however Debian uses 4.8 */
  GtkWidget* dialog = gtk_file_chooser_dialog_new(_("Open NBT file"), GTK_WINDOW(window),
  GTK_FILE_CHOOSER_ACTION_OPEN, _("_Cancel"), GTK_RESPONSE_CANCEL,
  _("_Open"), GTK_RESPONSE_ACCEPT, NULL);
  gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filter);
  gtk_window_present(GTK_WINDOW(dialog));
  g_signal_connect(dialog, "response", G_CALLBACK(nbt_open_response), NULL);
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
  GtkWidget *region_button = gtk_button_new_with_label(_("Create Region from NBT"));
  gtk_box_append (GTK_BOX(box), region_button);

  GtkWidget *second_label = gtk_label_new(_("Second, you can do lots of things with the Region."));
  gtk_box_append (GTK_BOX(box), second_label);

  /* Lots of options and choices */
  GtkWidget *gen_item_list_btn = gtk_button_new_with_label (_("Generate item list"));
  GtkWidget *block_reader_btn = gtk_button_new_with_label (_("Block reader"));
  gtk_box_append (GTK_BOX(box), gen_item_list_btn);
  gtk_box_append (GTK_BOX(box), block_reader_btn);

  gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

  return box;
}

static GtkWidget*
make_nbt_box()
{
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  GtkWidget *first_label = gtk_label_new (_("Let's load a NBT file here!"));
  gtk_box_append (GTK_BOX(box), first_label);

  GtkWidget *decision_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *load_btn = gtk_button_new_with_label (_("Load"));
  g_signal_connect(load_btn, "clicked", G_CALLBACK(load_nbt_file), NULL);
  GtkWidget *manage_btn = gtk_button_new_with_label (_("Manage NBT"));
  gtk_box_append (GTK_BOX(decision_box), load_btn);
  gtk_box_append (GTK_BOX(decision_box), manage_btn);
  gtk_widget_set_halign(decision_box, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX(box), decision_box);

  GtkWidget *second_label = gtk_label_new(_("You can do things below for the NBT file."));
  gtk_box_append (GTK_BOX(box), second_label);
  GtkWidget *nbt_reader_btn = gtk_button_new_with_label(_("NBT Reader"));
  gtk_box_append (GTK_BOX(box), nbt_reader_btn);
  gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

  return box;
}

static GtkWidget*
make_item_list_box()
{
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  GtkWidget *reader_btn = gtk_button_new_with_label(_("Item list reader and modifier"));
  GtkWidget *recipe_btn = gtk_button_new_with_label(_("Recipe combiner"));
  gtk_box_append (GTK_BOX(box), reader_btn);
  gtk_box_append (GTK_BOX(box), recipe_btn);

  gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

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
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(nbt_button), TRUE);
  
  gtk_box_append(GTK_BOX(box), nbt_button);
  gtk_box_append(GTK_BOX(box), region_button);
  gtk_box_append(GTK_BOX(box), item_list_button);
  gtk_toggle_button_set_group (GTK_TOGGLE_BUTTON(nbt_button), GTK_TOGGLE_BUTTON(region_button));
  gtk_toggle_button_set_group (GTK_TOGGLE_BUTTON(item_list_button), GTK_TOGGLE_BUTTON(region_button));

  gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header_bar), box);
  g_signal_connect (region_button, "toggled", G_CALLBACK(title_bar_button_toggled), NULL);
  g_signal_connect (nbt_button, "toggled", G_CALLBACK(title_bar_button_toggled), NULL);
  g_signal_connect (item_list_button, "toggled", G_CALLBACK(title_bar_button_toggled), NULL);

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