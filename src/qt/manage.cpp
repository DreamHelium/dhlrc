#include "manage.h"
#include "dh_string_util.h"
#include "dhtableview.h"
#include "glib.h"
#include "manageui.h"
#include <qabstractitemmodel.h>
#include <qcontainerfwd.h>
#include <qevent.h>
#include <qlist.h>
#include <qmessagebox.h>
#include <qmimedata.h>
#include <qnamespace.h>
#include <qobject.h>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <qstandarditemmodel.h>
#include "../translation.h"
#include <QDebug>
#include "utility.h"
#include <QMessageBox>
#include "../common_info.h"
#include "saveregionselectui.h"

static void messageNoRow(QWidget* parent)
{
    QMessageBox::critical(parent, _("Error!"), _("No row is selected!"));
}

using namespace dh;

static void update_model(DhInfoTypes type, QStandardItemModel* model)
{
    QList<QList<QStandardItem*>> itemList;

    GList* fullList = (GList*)common_info_list_get_uuid_list(type);
    guint len = fullList ? g_list_length(fullList) : 0;
    for(int i = 0 ; i < len ; i++)
    {
        auto info = common_info_list_get_common_info(type, (gchar*)g_list_nth_data(fullList, i));
        QStandardItem* description = new QStandardItem;
        QStandardItem* uuid = new QStandardItem;
        QStandardItem* time = new QStandardItem;
        // QStandardItem* type = new QStandardItem;
        uuid->setEditable(false);
        time->setEditable(false);
        // type->setEditable(false);
        if(g_rw_lock_reader_trylock(&info->info_lock))
        {
            description->setData(QString(info->description), 2);
            uuid->setData(QString((gchar*)g_list_nth_data(fullList, i)), 0);
            time->setData(QString(g_date_time_format(info->time, "%T")), 0);
            // type->setData(getTypeOfNbt(info->type), 0);
            g_rw_lock_reader_unlock(&info->info_lock);
        }
        else 
        {
            description->setData(QString(_("locked")), 0);
            uuid->setData(QString(_("locked")), 0);
            time->setData(QString(_("locked")), 0);
            // type->setData(QString(_("locked")), 0);
        }
        QList<QStandardItem*> list = {description, uuid, time};
        itemList.append(list);
    }

    model->clear();
    QStringList list;
    list << _("Description") << _("UUID") << _("Time") /*<< _("Type")*/;
    model->setHorizontalHeaderLabels(list);

    for(int i = 0 ; i < itemList.length() ; i++)
    {
        model->appendRow(itemList[i]);
    }
}

static void remove_items(DhInfoTypes type, QList<int> rows, ManageBase* base)
{
    if(rows.length())
    {
        QList<char*> removedList;
        GList* fulllist = (GList*) common_info_list_get_uuid_list(type);
        for(auto row : rows)
        {
            char* uuid = (char*)g_list_nth_data(fulllist, row);
            removedList.append(uuid);
        }
        for(auto uuid : removedList)
            common_info_list_remove_item(type, uuid);

        base->updateModel();
        base->mui->updateModel(base->model);
    }
    else messageNoRow(base->mui);
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

ManageBase::ManageBase()
    : QObject(), mui(new ManageUI())
{
    QObject::connect(mui, &ManageUI::add, this, &ManageBase::add_triggered);
    QObject::connect(mui, &ManageUI::remove, this, &ManageBase::remove_triggered);
    QObject::connect(mui, &ManageUI::save, this, &ManageBase::save_triggered);
    QObject::connect(mui, &ManageUI::refresh, this, &ManageBase::refresh_triggered);
    QObject::connect(mui, &ManageUI::showSig, this, &ManageBase::showSig_triggered);
    QObject::connect(mui, &ManageUI::closeSig, this, &ManageBase::closeSig_triggered);
    QObject::connect(mui, &ManageUI::ok, this, &ManageBase::ok_triggered);
    QObject::connect(mui->view, &DhTableView::tableDND, this, &ManageBase::tablednd_triggered);
}

ManageBase::~ManageBase()
{
    delete mui;
}

void ManageBase::show()
{
    mui->show();
}


// void ManageNBT::save_triggered(QList<int> rows)
// {
//     if(rows.length())
//     {
//         if(rows.length() == 1)
//         {
//             auto row = rows[0];
//             QString filepos = QFileDialog::getSaveFileName(mui, _("Save File"));
//             if(!filepos.isEmpty())
//             {
//                 NbtInfo* info = nbt_info_list_get_nbt_info((char*)g_list_nth_data(uuidList->list, row));
//                 if(g_rw_lock_writer_trylock(&info->info_lock))
//                 {
//                     dhlrc_nbt_save(info->root, filepos.toUtf8());
//                     g_rw_lock_writer_unlock(&info->info_lock);
//                 }
//                 else QMessageBox::critical(mui, _("Info Locked!"), _("The NBT info is locked!"));
//             }
//         }
//         else
//         {
//             auto dir = QFileDialog::getExistingDirectory(mui, _("Save Files"));
//             if(!dir.isEmpty())
//             {
//                 QStringList lockedInfo;
//                 for(auto row : rows)
//                 {
//                     NbtInfo* info = nbt_info_list_get_nbt_info((char*)g_list_nth_data(uuidList->list, row));
//                     QString filepos = (dir + G_DIR_SEPARATOR + info->description);
//                     if(g_rw_lock_writer_trylock(&info->info_lock))
//                     {
//                         dhlrc_nbt_save(info->root, filepos.toUtf8());
//                         g_rw_lock_writer_unlock(&info->info_lock);
//                     }
//                     else lockedInfo << info->description;
//                 }
//                 if(lockedInfo.length())
//                 {
//                     QString lockedInfoStr = _("The following infos are locked:\n");
//                     for(auto str : lockedInfo)
//                     {
//                         lockedInfoStr += str;
//                         lockedInfoStr += '\n';
//                     }
//                     QMessageBox::critical(mui, _("Error!"), lockedInfoStr);
//                 }
//             }
//         }
//     }
//     else messageNoRow(mui);
// }

// void ManageNBT::ok_triggered()
// {
//     /* Update model first */
//     for(int i = 0 ; i < model->rowCount() ; i++)
//     {
//         NbtInfo* info = nbt_info_list_get_nbt_info(model->index(i, 1).data().toString().toUtf8());
//         if(g_rw_lock_writer_trylock(&info->info_lock))
//         {
//             const char* str = model->index(i,0).data().toString().toUtf8();
//             g_free(info->description);
//             info->description = g_strdup(str);
//             g_rw_lock_writer_unlock(&info->info_lock);
//         }
//     }
//     mui->close();
// }

static void update_region_model(void* main_class)
{
    auto c = (ManageRegion*)main_class;
    c->refresh_triggered();
}

ManageRegion::ManageRegion()
{
    mui->setDND(true);
    QObject::connect(mui, &ManageUI::dnd, this, &ManageRegion::dnd_triggered);
    model = new QStandardItemModel();
    mui->setWindowTitle(_("Manage Region"));
    common_info_list_add_update_notifier(DH_TYPE_Region, (void*)this, update_region_model);
}

ManageRegion::~ManageRegion()
{
    delete model;
    common_info_list_remove_update_notifier(DH_TYPE_Region, (void*)this);
}

void ManageRegion::updateModel()
{
   update_model(DH_TYPE_Region, model);
}

void ManageRegion::add_triggered()
{
    loadRegion(mui);
    updateModel();
    mui->updateModel(model);
}

void ManageRegion::remove_triggered(QList<int> rows)
{
    remove_items(DH_TYPE_Region, rows, this);
}

void ManageRegion::save_triggered(QList<int> rows)
{
    DhStrArray* arr = nullptr;
    auto uuidlist = (GList*)common_info_list_get_uuid_list(DH_TYPE_Region);
    for(auto row : rows)
        dh_str_array_add_str(&arr, (char*)g_list_nth_data(uuidlist, row));
    auto plain_array = dh_str_array_dup_to_plain(arr);
    dh_str_array_free(arr);
    common_info_list_set_multi_uuid(DH_TYPE_Region, (const char**)plain_array);
    dh_str_array_free_plain(plain_array);
    SaveRegionSelectUI* srsui = new SaveRegionSelectUI();
    srsui->setAttribute(Qt::WA_DeleteOnClose);
    srsui->exec();
}

void ManageRegion::refresh_triggered()
{
    updateModel();
    mui->updateModel(model);
}

void ManageRegion::showSig_triggered()
{
    refresh_triggered();
}

void ManageRegion::closeSig_triggered()
{ }

void ManageRegion::ok_triggered()
{
    // /* Update model first */
    // for(int i = 0 ; i < model->rowCount() ; i++)
    // {
    //     RegionInfo* info = region_info_list_get_region_info(model->index(i, 1).data().toString().toUtf8());
    //     if(g_rw_lock_writer_trylock(&info->info_lock))
    //     {
    //         const char* str = model->index(i,0).data().toString().toUtf8();
    //         g_free(info->description);
    //         info->description = g_strdup(str);
    //         g_rw_lock_writer_unlock(&info->info_lock);
    //     }
    // }
    // mui->close();
}

void ManageRegion::dnd_triggered(const QMimeData* data)
{
    if(!data->hasFormat("application/x-qabstractitemmodeldatalist"))
        QMessageBox::critical(mui, _("Error!"), _("Not a valid NBT List!"));
    else
    {
        auto newModel = new QStandardItemModel;
        newModel->dropMimeData(data, Qt::CopyAction, 0, 0, QModelIndex());
        auto text = newModel->index(0, 1).data().toString().toUtf8();
        g_message("%s", (const char*)text);
        if(g_uuid_string_is_valid(text))
        {
            loadRegion(mui, text);
            updateModel();
            mui->updateModel(model);
        }
        else QMessageBox::critical(mui, _("Error!"), _("Not a valid NBT UUID!"));
        delete newModel;
    }
}

static void update_interface_model(void* main_class)
{
    auto c = (ManageNbtInterface*)main_class;
    c->refresh_triggered();
}

ManageNbtInterface::ManageNbtInterface()
{
    mui->setDND(true);
    model = new QStandardItemModel();
    mui->setWindowTitle(_("Manage NBT Interface"));
    QObject::connect(mui, &ManageUI::dnd, this, &ManageNbtInterface::dnd_triggered);
    common_info_list_add_update_notifier(DH_TYPE_NBT_INTERFACE, (void*)this, update_interface_model);
}

ManageNbtInterface::~ManageNbtInterface()
{
    delete model;
    common_info_list_remove_update_notifier(DH_TYPE_Region, (void*)this);
}

void ManageNbtInterface::add_triggered()
{
    auto dirs = QFileDialog::getOpenFileNames(mui, _("Select a file"), nullptr, _("Litematic file (*.litematic);;NBT File (*.nbt)"));
    dh::loadNbtInstances(mui, dirs);
    updateModel();
    mui->updateModel(model);
}

void ManageNbtInterface::updateModel()
{
    update_model(DH_TYPE_NBT_INTERFACE, model);
}

void ManageNbtInterface::refresh_triggered()
{
    updateModel();
    mui->updateModel(model);
}

void ManageNbtInterface::showSig_triggered()
{
    refresh_triggered();
}

void ManageNbtInterface::closeSig_triggered()
{}

void ManageNbtInterface::remove_triggered(QList<int> rows)
{
    remove_items(DH_TYPE_NBT_INTERFACE, rows, this);
}

void ManageNbtInterface::save_triggered(QList<int> rows)
{}

void ManageNbtInterface::ok_triggered()
{}
void ManageNbtInterface::dnd_triggered(const QMimeData* data)
{
    auto urls = data->urls();
    QStringList filelist;
    for(int i = 0 ; i < urls.length() ; i++)
    {
        filelist << urls[i].toLocalFile();
    }
    dh::loadNbtInstances(mui, filelist);
    updateModel();
    mui->updateModel(model);
}