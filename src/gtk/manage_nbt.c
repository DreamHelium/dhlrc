#include "manage_nbt.h"
#include "dh_string_util.h"
#include "gio/gio.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "gtk/gtkshortcut.h"
#include "manage_window.h"
#include "../translation.h"
#include "input_dialog.h"
#include "../nbt_info.h"

static GtkWidget* window = NULL;

static char* getTypeOfNbt(DhNbtTypes type)
{
    switch(type)
    {
        case Litematica: return _("Litematica");
        case NBTStruct : return _("NBT Struct");
        case Schematics: return _("Schematics");
        case Others    : return _("Others");
        default        : return "???";
    }
}

static GListModel*
get_model ()
{
    GListStore* store;
    DhList* list = nbt_info_list_get_uuid_list();
    GList* uuid_list = list->list;
    store = g_list_store_new(GTK_TYPE_STRING_LIST);
    for(; uuid_list ; uuid_list = uuid_list->next)
    {
        DhStrArray* arr = NULL;
        NbtInfo* info = nbt_info_list_get_nbt_info(uuid_list->data);
        dh_str_array_add_str(&arr, info->description);
        dh_str_array_add_str(&arr, uuid_list->data);
        char* time_literal = g_date_time_format(info->time, "%T");
        dh_str_array_add_str(&arr, time_literal);
        dh_str_array_add_str(&arr, getTypeOfNbt(info->type));
        g_free(time_literal);
        GtkStringList* str_list = gtk_string_list_new((const char**)arr->val);
        dh_str_array_free(arr);
        g_list_store_append(store, str_list);
    }
    return G_LIST_MODEL(store);
}

static void
setup_listitem_cb (GtkListItemFactory *factory,
                   GtkListItem        *list_item)
{
    GtkWidget* box;
    GtkWidget* des_label;
    GtkWidget* uuid_label;
    GtkWidget* time_label;
    GtkWidget* type_label;

    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    des_label = gtk_label_new("");
    uuid_label = gtk_label_new("");
    time_label = gtk_label_new("");
    type_label = gtk_label_new("");

    gtk_box_append(GTK_BOX(box), des_label);
    gtk_box_append(GTK_BOX(box), uuid_label);
    gtk_box_append(GTK_BOX(box), time_label);
    gtk_box_append(GTK_BOX(box), type_label);

    gtk_list_item_set_child(list_item, box);
}

static void
bind_listitem_cb (GtkListItemFactory *factory,
                  GtkListItem        *list_item)
{
    GtkWidget* des_label;
    GtkWidget* uuid_label;
    GtkWidget* time_label;
    GtkWidget* type_label;
    GtkStringList* list;

    des_label = gtk_widget_get_first_child (gtk_list_item_get_child (list_item));
    uuid_label = gtk_widget_get_next_sibling(des_label);
    time_label = gtk_widget_get_next_sibling(uuid_label);
    type_label = gtk_widget_get_next_sibling(time_label);
    list = gtk_list_item_get_item(list_item);

    gtk_label_set_label(GTK_LABEL(des_label), gtk_string_list_get_string(list, 0));
    g_message("%s", gtk_string_list_get_string(list, 0));
    gtk_label_set_label(GTK_LABEL(uuid_label), gtk_string_list_get_string(list, 1));
    gtk_label_set_label(GTK_LABEL(time_label), gtk_string_list_get_string(list, 2));
    gtk_label_set_label(GTK_LABEL(type_label), gtk_string_list_get_string(list, 3));
}

static GtkListItemFactory*
set_row_factory()
{
    GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
    g_signal_connect (factory, "setup", G_CALLBACK (setup_listitem_cb), NULL);
    g_signal_connect (factory, "bind", G_CALLBACK (bind_listitem_cb), NULL);
    return factory;
}

static void
refresh_model(GtkListView* view)
{
    GtkSelectionModel* model = GTK_SELECTION_MODEL(gtk_single_selection_new(get_model()));
    g_message("%d", GTK_IS_SELECTION_MODEL(model));
    gtk_list_view_set_model(view, model);
    gtk_list_view_set_factory(view, set_row_factory());
}

static void
nbt_open_response (GtkDialog *dialog,
                  int        response,
                  gpointer user_data)
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
        NBT* new_nbt = NBT_Parse((guint8*)content, len);

        char* description = dh_input_dialog_new_no_func(_("Enter Desciption"), _("Please enter description for the NBT."), _("Desciption"), name, GTK_WINDOW(window));

        if(description)
        {
            nbt_info_new(new_nbt, g_date_time_new_now_local(), description);
        }
        else
        {
            NBT_Free(new_nbt);
    #ifdef GDK_AVAILABLE_IN_4_10
            GtkAlertDialog* dialog = gtk_alert_dialog_new(_("No desciption entered! The NBT will not be added!"));
            gtk_alert_dialog_show(dialog, GTK_WINDOW(window));
    #else
            GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(window), 
    GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, _("No desciption entered! The NBT will not be added!"));
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
    #endif
        }

        g_free(description);

        g_object_unref(file);
    }

    gtk_window_destroy (GTK_WINDOW (dialog));
    refresh_model(GTK_LIST_VIEW(user_data));
}

static void add_cb(DhManageWindow* self, GtkListView* view)
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
    g_signal_connect(dialog, "response", G_CALLBACK(nbt_open_response), view);
}

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
    g_signal_connect(window, "add", G_CALLBACK(add_cb), NULL);
    gtk_window_present(GTK_WINDOW(window));
}