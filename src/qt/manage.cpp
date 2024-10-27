#include "manage.h"
#include "dh_string_util.h"
#include "dhtableview.h"
#include "glib.h"
#include "glibconfig.h"
#include "manageui.h"
#include <cstddef>
#include <qabstractitemmodel.h>
#include <qcontainerfwd.h>
#include <qevent.h>
#include <qmessagebox.h>
#include <qmimedata.h>
#include <qnamespace.h>
#include <qobject.h>
#include "../nbt_info.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <qstandarditemmodel.h>
#include "../translation.h"
#include "../nbt_litereader.h"
#include <QDebug>
#include "../region_info.h"
#include "saveregionselectui.h"
#include "utility.h"
#include <QMessageBox>

static void messageNoRow(QWidget* parent)
{
    QMessageBox::critical(parent, _("Error!"), _("No row is selected!"));
}

using namespace dh;

static QString getTypeOfNbt(DhNbtType type)
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

ManageNBT::ManageNBT()
{
    mui->setDND(true);
    model = new QStandardItemModel();
    uuidList = nbt_info_list_get_uuid_list();
    mui->setWindowTitle(_("Manage NBT"));
    QObject::connect(mui, &ManageUI::dnd, this, &ManageNBT::dnd_triggered);
}

ManageNBT::~ManageNBT()
{
    delete model;
}

void ManageNBT::dnd_triggered(const QMimeData* data)
{
    auto urls = data->urls();
    QStringList filelist;
    for(int i = 0 ; i < urls.length() ; i++)
    {
        filelist << urls[i].toLocalFile();
    }
    dh::loadNbtFiles(mui, filelist);
    updateModel();
    mui->updateModel(model);
}

void ManageNBT::add_triggered()
{
    auto dirs = QFileDialog::getOpenFileNames(mui, _("Select a file"), nullptr, _("Litematic file (*.litematic);;NBT File (*.nbt)"));
    dh::loadNbtFiles(mui, dirs);
    updateModel();
    mui->updateModel(model);
}

void ManageNBT::remove_triggered(QList<int> rows)
{
    if(rows.length())
    {
        uuidList = nbt_info_list_get_uuid_list();
        QList<char*> removedList;
        for(auto row : rows)
        {
            char* uuid = (char*)g_list_nth_data(uuidList->list, row);
            removedList.append(uuid);
        }
        for(auto uuid : removedList)
            nbt_info_list_remove_item(uuid);

        updateModel();
        mui->updateModel(model);
    }
    else messageNoRow(mui);
}

void ManageNBT::refresh_triggered()
{
    updateModel();
    mui->updateModel(model);
}

void ManageNBT::updateModel()
{
    QList<QList<QStandardItem*>> itemList;

    uuidList = nbt_info_list_get_uuid_list();
    GList* fullList = uuidList->list;
    g_rw_lock_writer_unlock(&uuidList->lock);
    if(g_rw_lock_reader_trylock(&uuidList->lock))
    {
        guint len = fullList ? g_list_length(fullList) : 0;
        for(int i = 0 ; i < len ; i++)
        {
            NbtInfo* info = nbt_info_list_get_nbt_info((gchar*)g_list_nth_data(fullList, i));
            QStandardItem* description = new QStandardItem;
            QStandardItem* uuid = new QStandardItem;
            QStandardItem* time = new QStandardItem;
            QStandardItem* type = new QStandardItem;
            uuid->setEditable(false);
            time->setEditable(false);
            type->setEditable(false);
            if(g_rw_lock_reader_trylock(&info->info_lock))
            {
                description->setData(QString(info->description), 2);
                uuid->setData(QString((gchar*)g_list_nth_data(fullList, i)), 0);
                time->setData(QString(g_date_time_format(info->time, "%T")), 0);
                type->setData(getTypeOfNbt(info->type), 0);
                g_rw_lock_reader_unlock(&info->info_lock);
            }
            else 
            {
                description->setData(QString(_("locked")), 0);
                uuid->setData(QString(_("locked")), 0);
                time->setData(QString(_("locked")), 0);
                type->setData(QString(_("locked")), 0);
            }
            QList<QStandardItem*> list = {description, uuid, time, type};
            itemList.append(list);
        }
        g_rw_lock_reader_unlock(&uuidList->lock);
    }
    else
    {
        /* This is a rare situation, if occured, contact the programmer. */
        QStandardItem* description = new QStandardItem;
        QStandardItem* uuid = new QStandardItem;
        QStandardItem* time = new QStandardItem;
        QStandardItem* type = new QStandardItem;
        description->setData(QString(_("List locked")), 0);
        uuid->setData(QString(_("List locked")), 0);
        time->setData(QString(_("List locked")), 0);
        type->setData(QString(_("List locked")), 0);
        QList<QStandardItem*> list = {description, uuid, time, type};
        itemList.append(list);
    }
    g_rw_lock_writer_trylock(&uuidList->lock);

    model->clear();
    QStringList list;
    list << _("Description") << _("UUID") << _("Time") << _("Type");
    model->setHorizontalHeaderLabels(list);

    for(int i = 0 ; i < itemList.length() ; i++)
    {
        model->appendRow(itemList[i]);
    }
}

void ManageNBT::save_triggered(QList<int> rows)
{
    if(rows.length())
    {
        if(rows.length() == 1)
        {
            auto row = rows[0];
            QString filepos = QFileDialog::getSaveFileName(mui, _("Save File"));
            if(!filepos.isEmpty())
            {
                NbtInfo* info = nbt_info_list_get_nbt_info((char*)g_list_nth_data(uuidList->list, row));
                if(g_rw_lock_writer_trylock(&info->info_lock))
                {
                    dhlrc_nbt_save(info->root, filepos.toUtf8());
                    g_rw_lock_writer_unlock(&info->info_lock);
                }
                else QMessageBox::critical(mui, _("Info Locked!"), _("The NBT info is locked!"));
            }
        }
        else
        {
            auto dir = QFileDialog::getExistingDirectory(mui, _("Save Files"));
            if(!dir.isEmpty())
            {
                QStringList lockedInfo;
                for(auto row : rows)
                {
                    NbtInfo* info = nbt_info_list_get_nbt_info((char*)g_list_nth_data(uuidList->list, row));
                    QString filepos = (dir + G_DIR_SEPARATOR + info->description);
                    if(g_rw_lock_writer_trylock(&info->info_lock))
                    {
                        dhlrc_nbt_save(info->root, filepos.toUtf8());
                        g_rw_lock_writer_unlock(&info->info_lock);
                    }
                    else lockedInfo << info->description;
                }
                if(lockedInfo.length())
                {
                    QString lockedInfoStr = _("The following infos are locked:\n");
                    for(auto str : lockedInfo)
                    {
                        lockedInfoStr += str;
                        lockedInfoStr += '\n';
                    }
                    QMessageBox::critical(mui, _("Error!"), lockedInfoStr);
                }
            }
        }
    }
    else messageNoRow(mui);
}

void ManageNBT::showSig_triggered()
{
    uuidList = nbt_info_list_get_uuid_list();
    g_rw_lock_writer_lock(&uuidList->lock);
}

void ManageNBT::closeSig_triggered()
{
    uuidList = nbt_info_list_get_uuid_list();
    g_rw_lock_writer_unlock(&uuidList->lock);
}

void ManageNBT::ok_triggered()
{
    /* Update model first */
    for(int i = 0 ; i < model->rowCount() ; i++)
    {
        NbtInfo* info = nbt_info_list_get_nbt_info(model->index(i, 1).data().toString().toUtf8());
        if(g_rw_lock_writer_trylock(&info->info_lock))
        {
            const char* str = model->index(i,0).data().toString().toUtf8();
            g_free(info->description);
            info->description = g_strdup(str);
            g_rw_lock_writer_unlock(&info->info_lock);
        }
    }
    mui->close();
}

void ManageBase::tablednd_triggered(QDropEvent* event)
{
    int throwedRow = -1;
    auto obj = event->source();
    if(obj == mui->view)
    {
        #if QT_VERSION_MAJOR >= 6
        auto pos = event->position();
        #else
        auto pos = event->posF();
        #endif
        throwedRow = mui->view->indexAt(pos.toPoint()).row();
        /* We delete this and add this */
        auto selectedModel = mui->view->selectionModel();
        auto rowList = selectedModel->selectedRows();
        QList<char*> deletedList;
        for(auto row : rowList)
        {
            auto rowNum = row.row();
            char* uuid = (char*)g_list_nth_data(uuidList->list, rowNum);
            deletedList.append(uuid);
        }
        GList* afterList =  throwedRow != -1 ? g_list_nth(uuidList->list, throwedRow) : NULL;
        int i = 0;
        for(auto uuid : deletedList)
        {
            uuidList->list = g_list_remove(uuidList->list, uuid);
            uuidList->list = g_list_insert_before(uuidList->list, afterList, uuid);
            i++;
        }
        updateModel();
        mui->updateModel(model);
    }
    else
    {
        dnd_triggered(event->mimeData());
    }
}


ManageRegion::ManageRegion()
{
    mui->setDND(true);
    QObject::connect(mui, &ManageUI::dnd, this, &ManageRegion::dnd_triggered);
    model = new QStandardItemModel();
    uuidList = region_info_list_get_uuid_list();
    mui->setWindowTitle(_("Manage Region"));
}

ManageRegion::~ManageRegion()
{
    delete model;
}

void ManageRegion::updateModel()
{
    QList<QList<QStandardItem*>> itemList;

    uuidList = region_info_list_get_uuid_list();
    GList* fullList = uuidList->list;
    g_rw_lock_writer_unlock(&uuidList->lock);
    if(g_rw_lock_reader_trylock(&uuidList->lock))
    {
        guint len = fullList ? g_list_length(fullList) : 0;
        for(int i = 0 ; i < len ; i++)
        {
            RegionInfo* info = region_info_list_get_region_info((gchar*)g_list_nth_data(fullList, i));
            QStandardItem* description = new QStandardItem;
            QStandardItem* uuid = new QStandardItem;
            QStandardItem* time = new QStandardItem;
            uuid->setEditable(false);
            time->setEditable(false);
            if(g_rw_lock_reader_trylock(&info->info_lock))
            {
                description->setData(QString(info->description), 2);
                uuid->setData(QString((gchar*)g_list_nth_data(fullList, i)), 0);
                time->setData(QString(g_date_time_format(info->time, "%T")), 0);
                g_rw_lock_reader_unlock(&info->info_lock);
            }
            else 
            {
                description->setData(QString(_("locked")), 0);
                uuid->setData(QString(_("locked")), 0);
                time->setData(QString(_("locked")), 0);
            }
            QList<QStandardItem*> list = {description, uuid, time};
            itemList.append(list);
        }
        g_rw_lock_reader_unlock(&uuidList->lock);
    }
    else
    {
        /* This is a rare situation, if occured, contact the programmer. */
        QStandardItem* description = new QStandardItem;
        QStandardItem* uuid = new QStandardItem;
        QStandardItem* time = new QStandardItem;
        description->setData(QString(_("List locked")), 0);
        uuid->setData(QString(_("List locked")), 0);
        time->setData(QString(_("List locked")), 0);
        QList<QStandardItem*> list = {description, uuid, time};
        itemList.append(list);
    }
    g_rw_lock_writer_trylock(&uuidList->lock);

    model->clear();
    QStringList list;
    list << _("Description") << _("UUID") << _("Time");
    model->setHorizontalHeaderLabels(list);

    for(int i = 0 ; i < itemList.length() ; i++)
    {
        model->appendRow(itemList[i]);
    }
}

void ManageRegion::add_triggered()
{
    loadRegion(mui);
    updateModel();
    mui->updateModel(model);
}

void ManageRegion::remove_triggered(QList<int> rows)
{
    if(rows.length())
    {
        uuidList = nbt_info_list_get_uuid_list();
        QList<char*> removedList;
        for(auto row : rows)
        {
            char* uuid = (char*)g_list_nth_data(uuidList->list, row);
            removedList.append(uuid);
        }
        for(auto uuid : removedList)
            region_info_list_remove_item(uuid);

        updateModel();
        mui->updateModel(model);
    }
    else messageNoRow(mui);
}

void ManageRegion::save_triggered(QList<int> rows)
{
    DhStrArray* arr = nullptr;
    for(auto row : rows)
        dh_str_array_add_str(&arr, (char*)g_list_nth_data(uuidList->list, row));
    region_info_list_set_multi_uuid(arr);
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
    uuidList = region_info_list_get_uuid_list();
    g_rw_lock_writer_lock(&uuidList->lock);
}

void ManageRegion::closeSig_triggered()
{
    uuidList = region_info_list_get_uuid_list();
    g_rw_lock_writer_unlock(&uuidList->lock);
}

void ManageRegion::ok_triggered()
{
    /* Update model first */
    for(int i = 0 ; i < model->rowCount() ; i++)
    {
        RegionInfo* info = region_info_list_get_region_info(model->index(i, 1).data().toString().toUtf8());
        if(g_rw_lock_writer_trylock(&info->info_lock))
        {
            const char* str = model->index(i,0).data().toString().toUtf8();
            g_free(info->description);
            info->description = g_strdup(str);
            g_rw_lock_writer_unlock(&info->info_lock);
        }
    }
    mui->close();
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