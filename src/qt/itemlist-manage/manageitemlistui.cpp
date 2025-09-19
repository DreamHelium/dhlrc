#include "manageitemlistui.h"

#include "../../common_info.h"

static ManageItemList *mil = nullptr;

static void *
newWin ()
{
    if (!mil)
        mil = new ManageItemList ();
    return mil;
}

void
init (DhModule *module)
{
    module->module_name = g_strdup ("itemlist-manage-qt");
    module->module_type = g_strdup ("qt-shortcut");
    module->module_description = g_strdup ("Manage Item List");
    module->module_functions = g_ptr_array_new ();
    g_ptr_array_add (module->module_functions,
                     reinterpret_cast<gpointer> (newWin));
}

ManageItemList::ManageItemList () { type = DH_TYPE_ITEM_LIST; }

void
ManageItemList::add_triggered ()
{
}

void
ManageItemList::save_triggered (QList<int> rows)
{
}
void
ManageItemList::dnd_triggered (const QMimeData *data)
{
}