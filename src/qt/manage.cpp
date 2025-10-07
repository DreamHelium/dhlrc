#include "manage.h"
#include "../common_info.h"
#include "../feature/dh_module.h"
#include "../nbt_interface_cpp/nbt_interface.hpp"
#include "../region.h"
#include "../translation.h"
#include "dh_string_util.h"
#include "dhtableview.h"
#include "glib.h"
#include "manageui.h"
#include "saveregionselectui.h"
#include "utility.h"
#include <QDebug>
#include <QFileDialog>
#include <generalchoosedialog.h>
#include <qabstractitemmodel.h>
#include <qcontainerfwd.h>
#include <qevent.h>
#include <qlist.h>
#include <qmessagebox.h>
#include <qmimedata.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qstandarditemmodel.h>

static void
messageNoRow (QWidget *parent)
{
    QMessageBox::critical (parent, _ ("Error!"), _ ("No row is selected!"));
}

using namespace dh;

static void
update_model (int type, QStandardItemModel *model)
{
    QList<QList<QStandardItem *>> itemList;

    auto fullList = dh_info_get_all_uuid (type);
    for (int i = 0; fullList && i < fullList->num; i++)
        {
            auto uuid_s = fullList->val[i];
            QStandardItem *description = new QStandardItem;
            QStandardItem *uuid = new QStandardItem;
            QStandardItem *time = new QStandardItem;
            // QStandardItem* type = new QStandardItem;
            uuid->setEditable (false);
            time->setEditable (false);
            // type->setEditable(false);
            if (dh_info_reader_trylock (type, uuid_s))
                {
                    description->setData (
                        QString (dh_info_get_description (type, uuid_s)), 2);
                    uuid->setData (QString (uuid_s), 0);
                    time->setData (QString (g_date_time_format (
                                       dh_info_get_time (type, uuid_s), "%T")),
                                   0);
                    // type->setData(getTypeOfNbt(info->type), 0);
                    dh_info_reader_unlock (type, uuid_s);
                }
            else
                {
                    description->setData (QString (_ ("locked")), 0);
                    uuid->setData (QString (_ ("locked")), 0);
                    time->setData (QString (_ ("locked")), 0);
                    // type->setData(QString(_("locked")), 0);
                }
            QList<QStandardItem *> list = { description, uuid, time };
            itemList.append (list);
        }

    model->clear ();
    QStringList list;
    list << _ ("Description") << _ ("UUID") << _ ("Time") /*<< _("Type")*/;
    model->setHorizontalHeaderLabels (list);

    for (int i = 0; i < itemList.length (); i++)
        model->appendRow (itemList[i]);
}

static void
remove_items (int type, const QList<int> &rows, ManageBase *base)
{
    if (rows.length ())
        {
            QList<char *> removedList;
            auto fulllist = dh_info_get_all_uuid (type);
            for (auto row : rows)
                {
                    auto uuid = fulllist->val[row];
                    removedList.append (uuid);
                }
            for (auto uuid : removedList)
                dh_info_remove (type, uuid);
        }
    else
        messageNoRow (base->mui);
}

// static QString getTypeOfNbt(DhNbtTypes type)
// {
//     switch(type)
//     {
//         case Litematica: return _("Litematica");
//         case NBTStruct : return _("NBT Struct");
//         case Schematics: return _("Schematics");
//         case Others    : return _("Others");
//         default        : return "???";
//     }
// }

ManageBase::ManageBase (QWidget *parent) : ManageUI (parent), mui (this)
{
    model = new QStandardItemModel ();
    QObject::connect (mui, &ManageUI::add, this, &ManageBase::add_triggered);
    QObject::connect (mui, &ManageUI::remove, this,
                      &ManageBase::remove_triggered);
    QObject::connect (mui, &ManageUI::save, this, &ManageBase::save_triggered);
    QObject::connect (mui, &ManageUI::refresh, this,
                      &ManageBase::refresh_triggered);
    QObject::connect (mui, &ManageUI::showSig, this,
                      &ManageBase::showSig_triggered);
    QObject::connect (mui, &ManageUI::closeSig, this,
                      &ManageBase::closeSig_triggered);
    QObject::connect (mui, &ManageUI::ok, this, &ManageBase::ok_triggered);
    QObject::connect (mui->view, &DhTableView::tableDND, this,
                      &ManageBase::tablednd_triggered);
    g_mutex_init (&lock);
}

ManageBase::~ManageBase ()
{
    dh_info_remove_notifier (type, this);
    delete model;
    g_mutex_clear (&lock);
}

void
ManageBase::deleteItems (const QList<int> &rows)
{
    remove_items (type, rows, this);
}

void
ManageBase::updateModel ()
{
    update_model (type, model);
}

ManageRegion::ManageRegion ()
{
    type = DH_TYPE_REGION;
    setDND (true);
    QObject::connect (mui, &ManageUI::dnd, this, &ManageRegion::dnd_triggered);
    setWindowTitle (_ ("Manage Region"));
}

void
ManageRegion::add_triggered ()
{
    loadRegion (mui);
}

void
ManageRegion::save_triggered (QList<int> rows)
{
    DhStrArray *arr = nullptr;
    auto uuidlist = dh_info_get_all_uuid (DH_TYPE_REGION);
    for (auto row : rows)
        dh_str_array_add_str (&arr, (*uuidlist)[row]);
    if (arr)
        {
            dh_info_set_uuid (DH_TYPE_REGION, arr);
            dh_str_array_free (arr);
            auto srsui = new SaveRegionSelectUI ();
            srsui->setAttribute (Qt::WA_DeleteOnClose);
            srsui->exec ();
        }
    else
        QMessageBox::critical (mui, ERROR_TITLE, _ ("No item selected!"));
}

void
ManageRegion::dnd_triggered (const QMimeData *data)
{
    if (!data->hasFormat ("application/x-qabstractitemmodeldatalist"))
        QMessageBox::critical (mui, _ ("Error!"), _ ("Not a valid NBT List!"));
    else
        {
            auto newModel = new QStandardItemModel;
            newModel->dropMimeData (data, Qt::CopyAction, 0, 0,
                                    QModelIndex ());
            auto text = newModel->index (0, 1).data ().toString ().toUtf8 ();
            g_message ("%s", (const char *)text);
            if (g_uuid_string_is_valid (text))
                {
                    loadRegion (mui, text);
                    updateModel ();
                    mui->updateModel (model);
                }
            else
                QMessageBox::critical (mui, _ ("Error!"),
                                       _ ("Not a valid NBT UUID!"));
            delete newModel;
        }
}

ManageNbtInterface::ManageNbtInterface ()
{
    type = DH_TYPE_NBT_INTERFACE_CPP;
    setDND (true);
    setWindowTitle (_ ("Manage NBT Interface"));
    QObject::connect (mui, &ManageUI::dnd, this,
                      &ManageNbtInterface::dnd_triggered);
}

void
ManageNbtInterface::add_triggered ()
{
    auto dirs = QFileDialog::getOpenFileNames (
        mui, _ ("Select a file"), nullptr,
        _ ("Litematic file (*.litematic);;NBT File (*.nbt)"));
    dh::loadNbtInstances (mui, dirs);
}

void
ManageNbtInterface::save_triggered (QList<int> rows)
{
    auto save_option = [] (int ret, const char *uuid, const QString &filepos) {
        if (dh_info_reader_trylock (DH_TYPE_NBT_INTERFACE_CPP, uuid))
            {
                auto instance = static_cast<DhNbtInstance *> (
                    dh_info_get_data (DH_TYPE_NBT_INTERFACE_CPP, uuid));
                DhModule *conv_module = dh_search_inited_module ("conv");
                QList<Region *> regions;
                if (lite_region_num_instance (instance))
                    {
                        for (int i = 0;
                             i < lite_region_num_instance (instance); i++)
                            {
                                auto lr
                                    = lite_region_create_from_root_instance_cpp (
                                        *instance, i);
                                regions << region_new_from_lite_region (lr);
                            }
                    }
                else
                    regions << region_new_from_nbt_instance_ptr (instance);
                typedef void *(*trFunc) (Region *, gboolean);
                switch (ret)
                    {
                    case 0:
                        instance->save_to_file (filepos.toUtf8 ());
                        break;
                    case 1:
                        if (conv_module)
                            {
                                trFunc func = reinterpret_cast<trFunc> (
                                    conv_module->module_functions->pdata[1]);
                                for (auto region : regions)
                                    {
                                        auto temp
                                            = static_cast<DhNbtInstance *> (
                                                func (region, false));
                                        QString realFile;
                                        if (regions.length () == 1)
                                            realFile = filepos + ".nbt";
                                        else
                                            realFile
                                                = filepos + "_"
                                                  + region->data->description
                                                  + ".nbt";
                                        temp->save_to_file (
                                            realFile.toUtf8 ());
                                        delete temp;
                                    }
                            }
                        break;
                    case 2:
                        if (conv_module)
                            {
                                typedef void *(*s_trFunc) (Region *, gboolean,
                                                           gboolean);
                                s_trFunc func = reinterpret_cast<s_trFunc> (
                                    conv_module->module_functions->pdata[5]);

                                for (auto region : regions)
                                    {
                                        auto temp
                                            = static_cast<DhNbtInstance *> (
                                                func (region, false, true));
                                        QString realFile;
                                        if (regions.length () == 1)
                                            realFile = filepos + ".nbt";
                                        else
                                            realFile
                                                = filepos + "_"
                                                  + region->data->description
                                                  + ".nbt";
                                        temp->save_to_file (
                                            realFile.toUtf8 ());
                                        delete temp;
                                    }
                            }
                        break;
                    case 3:
                        if (conv_module)
                            {
                                typedef void *(*ss_trFunc) (Region *, gboolean,
                                                            int);
                                ss_trFunc func = reinterpret_cast<ss_trFunc> (
                                    conv_module->module_functions->pdata[4]);

                                for (auto region : regions)
                                    {
                                        auto temp
                                            = static_cast<DhNbtInstance *> (
                                                func (region, false, 5));
                                        QString realFile;
                                        if (regions.length () == 1)
                                            realFile = filepos + ".litematic";
                                        else
                                            realFile
                                                = filepos + "_"
                                                  + region->data->description
                                                  + ".litematic";
                                        temp->save_to_file (
                                            realFile.toUtf8 ());
                                        delete temp;
                                    }
                            }
                        break;
                    case 4:
                        if (conv_module)
                            {
                                trFunc func = reinterpret_cast<trFunc> (
                                    conv_module->module_functions->pdata[3]);
                                for (auto region : regions)
                                    {
                                        auto temp
                                            = static_cast<DhNbtInstance *> (
                                                func (region, false));
                                        QString realFile;
                                        if (regions.length () == 1)
                                            realFile = filepos + ".schem";
                                        else
                                            realFile
                                                = filepos + "_"
                                                  + region->data->description
                                                  + ".schem";
                                        temp->save_to_file (
                                            realFile.toUtf8 ());
                                        delete temp;
                                    }
                            }
                        break;
                    default:
                        break;
                    }
            }
    };

    if (!rows.empty ())
        {
            auto ret = GeneralChooseDialog::getIndex (
                _ ("Select an option"), _ ("Please select an option."), this,
                _ ("Save as original NBT."), _ ("Convert to NBT struct."),
                _ ("Convert to NBT struct (ignore air)."),
                _ ("Convert to litematic."), _ ("Convert to new schematics."));

            QList<int> lockedRows;
            for (auto row : rows)
                {
                    auto uuidlist
                        = dh_info_get_all_uuid (DH_TYPE_NBT_INTERFACE_CPP);
                    auto uuid = uuidlist->val[row];
                    if (dh_info_reader_trylock (DH_TYPE_NBT_INTERFACE_CPP,
                                                uuid))
                        dh_info_reader_unlock (DH_TYPE_NBT_INTERFACE_CPP,
                                               uuid);
                    else
                        lockedRows << row;
                }

            if (!lockedRows.isEmpty ())
                QMessageBox::warning (
                    this, _ ("Warning!"),
                    _ ("Some items are locked, which will be ignored."));

            if (rows.length () == 1 && lockedRows.isEmpty ())
                {
                    auto row = rows[0];
                    QString filepos
                        = QFileDialog::getSaveFileName (mui, _ ("Save File"));
                    if (!filepos.isEmpty ())
                        {
                            auto uuidlist = dh_info_get_all_uuid (
                                DH_TYPE_NBT_INTERFACE_CPP);
                            auto uuid = uuidlist->val[row];
                            save_option (ret, uuid, filepos);
                        }
                }
            else
                {
                    auto dir = QFileDialog::getExistingDirectory (
                        mui, _ ("Save Files"));
                    if (!dir.isEmpty ())
                        for (auto row : rows)
                            {
                                bool repeated = false;
                                for (auto lockedRow : lockedRows)
                                    if (row == lockedRow)
                                        repeated = true;
                                if (!repeated)
                                    {
                                        auto uuidlist = dh_info_get_all_uuid (
                                            DH_TYPE_NBT_INTERFACE_CPP);
                                        auto uuid = uuidlist->val[row];
                                        auto description
                                            = dh_info_get_description (
                                                DH_TYPE_NBT_INTERFACE_CPP,
                                                uuid);
                                        QString filepos
                                            = (dir + G_DIR_SEPARATOR
                                               + description);
                                        save_option (ret, uuid, filepos);
                                    }
                            }
                }
        }
    else
        messageNoRow (mui);
}

void
ManageNbtInterface::dnd_triggered (const QMimeData *data)
{
    auto urls = data->urls ();
    QStringList filelist;
    for (int i = 0; i < urls.length (); i++)
        {
            filelist << urls[i].toLocalFile ();
        }
    dh::loadNbtInstances (mui, filelist);
}

ManageModule::ManageModule () { type = DH_TYPE_MODULE; }

void
ManageModule::refresh_triggered ()
{
    ManageBase::refresh_triggered ();
    auto columns = model->takeColumn (0);
    for (auto item : columns)
        {
            item->setEditable (false);
        }
    model->insertColumn (0, columns);
    model->setHorizontalHeaderItem (0, new QStandardItem (_ ("Description")));
    QList<QStandardItem *> items;
    auto list = dh_info_get_all_uuid (type);
    for (int i = 0; list && i < **list; i++)
        {
            auto module = static_cast<DhModule *> (
                dh_info_get_data (type, (*list)[i]));
            auto item = new QStandardItem (module->module_description);
            item->setEditable (false);
            items.append (item);
        }

    model->appendColumn (items);
    model->setHorizontalHeaderItem (3,
                                    new QStandardItem ("Module Description"));
}