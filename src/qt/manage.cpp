#include "manage.h"
#include "../common_info.h"
#include "../nbt_interface_cpp/nbt_interface.hpp"
#include "../translation.h"
#include "dh_string_util.h"
#include "dhtableview.h"
#include "glib.h"
#include "manageui.h"
#include "saveregionselectui.h"
#include "utility.h"
#include <QDebug>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QProgressDialog>
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
        {
            model->appendRow (itemList[i]);
        }
}

static void
remove_items (int type, QList<int> rows, ManageBase *base)
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
}

ManageBase::~ManageBase () {}

static void
update_region_model (void *main_class)
{
    auto c = (ManageRegion *)main_class;
    c->refresh_triggered ();
}

ManageRegion::ManageRegion ()
{
    type = DH_TYPE_REGION;
    mui->setDND (true);
    QObject::connect (mui, &ManageUI::dnd, this, &ManageRegion::dnd_triggered);
    model = new QStandardItemModel ();
    mui->setWindowTitle (_ ("Manage Region"));
    dh_info_add_notifier (DH_TYPE_REGION, update_region_model, this);
}

ManageRegion::~ManageRegion ()
{
    dh_info_remove_notifier (DH_TYPE_REGION, this);
    delete model;
}

void
ManageRegion::updateModel ()
{
    update_model (DH_TYPE_REGION, model);
}

void
ManageRegion::add_triggered ()
{
    loadRegion (mui);
}

void
ManageRegion::remove_triggered (QList<int> rows)
{
    remove_items (DH_TYPE_REGION, rows, this);
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
            SaveRegionSelectUI *srsui = new SaveRegionSelectUI ();
            srsui->setAttribute (Qt::WA_DeleteOnClose);
            srsui->exec ();
        }
    else
        QMessageBox::critical (mui, ERROR_TITLE, _ ("No item selected!"));
}

void
ManageRegion::refresh_triggered ()
{
    updateModel ();
    mui->updateModel (model);
}

void
ManageRegion::showSig_triggered ()
{
    refresh_triggered ();
}

void
ManageRegion::closeSig_triggered ()
{
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

static void
update_interface_model (void *main_class)
{
    auto c = (ManageNbtInterface *)main_class;
    c->refresh_triggered ();
}

ManageNbtInterface::ManageNbtInterface ()
{
    type = DH_TYPE_NBT_INTERFACE_CPP;
    mui->setDND (true);
    model = new QStandardItemModel ();
    mui->setWindowTitle (_ ("Manage NBT Interface"));
    QObject::connect (mui, &ManageUI::dnd, this,
                      &ManageNbtInterface::dnd_triggered);
    dh_info_add_notifier (DH_TYPE_NBT_INTERFACE_CPP, update_interface_model,
                          this);
}

ManageNbtInterface::~ManageNbtInterface ()
{
    dh_info_remove_notifier (DH_TYPE_NBT_INTERFACE_CPP, this);
    delete model;
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
ManageNbtInterface::updateModel ()
{
    update_model (DH_TYPE_NBT_INTERFACE_CPP, model);
}

void
ManageNbtInterface::refresh_triggered ()
{
    updateModel ();
    mui->updateModel (model);
}

void
ManageNbtInterface::showSig_triggered ()
{
    refresh_triggered ();
}

void
ManageNbtInterface::closeSig_triggered ()
{
}

void
ManageNbtInterface::remove_triggered (QList<int> rows)
{
    remove_items (DH_TYPE_NBT_INTERFACE_CPP, rows, this);
}

void
ManageNbtInterface::save_triggered (QList<int> rows)
{
    if (rows.length ())
        {
            if (rows.length () == 1)
                {
                    auto row = rows[0];
                    QString filepos
                        = QFileDialog::getSaveFileName (mui, _ ("Save File"));
                    if (!filepos.isEmpty ())
                        {
                            auto uuidlist
                                = dh_info_get_uuid (DH_TYPE_NBT_INTERFACE_CPP);
                            auto uuid = uuidlist->val[row];
                            if (dh_info_reader_trylock (
                                    DH_TYPE_NBT_INTERFACE_CPP, uuid))
                                {
                                    auto instance
                                        = (DhNbtInstance *)dh_info_get_data (
                                            DH_TYPE_NBT_INTERFACE_CPP, uuid);
                                    instance->save_to_file (filepos.toUtf8 ());
                                    dh_info_reader_unlock (
                                        DH_TYPE_NBT_INTERFACE_CPP, uuid);
                                }
                            else
                                QMessageBox::critical (
                                    mui, _ ("Info Locked!"),
                                    _ ("The NBT info is locked!"));
                        }
                }
            else
                {
                    auto dir = QFileDialog::getExistingDirectory (
                        mui, _ ("Save Files"));
                    if (!dir.isEmpty ())
                        {
                            QStringList lockedInfo;
                            for (auto row : rows)
                                {
                                    auto uuidlist = dh_info_get_uuid (
                                        DH_TYPE_NBT_INTERFACE_CPP);
                                    auto uuid = uuidlist->val[row];
                                    auto description
                                        = dh_info_get_description (
                                            DH_TYPE_NBT_INTERFACE_CPP, uuid);
                                    QString filepos = (dir + G_DIR_SEPARATOR
                                                       + description);
                                    if (dh_info_reader_trylock (
                                            DH_TYPE_NBT_INTERFACE_CPP, uuid))
                                        {
                                            auto instance = (DhNbtInstance *)
                                                dh_info_get_data (
                                                    DH_TYPE_NBT_INTERFACE_CPP,
                                                    uuid);
                                            qDebug ()
                                                << instance->save_to_file (
                                                       filepos.toUtf8 ());
                                            dh_info_reader_unlock (
                                                DH_TYPE_NBT_INTERFACE_CPP,
                                                uuid);
                                        }
                                    else
                                        lockedInfo << description;
                                }
                            if (lockedInfo.length ())
                                {
                                    QString lockedInfoStr = _ (
                                        "The following infos are locked:\n");
                                    for (auto str : lockedInfo)
                                        {
                                            lockedInfoStr += str;
                                            lockedInfoStr += '\n';
                                        }
                                    QMessageBox::critical (mui, _ ("Error!"),
                                                           lockedInfoStr);
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