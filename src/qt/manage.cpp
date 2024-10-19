#include "manage.h"
#include "glib.h"
#include "manageui.h"
#include <qobject.h>
#include "../nbt_info.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <qstandarditemmodel.h>
#include "../translation.h"
#include "../nbt_litereader.h"

using namespace dh;

static QString getTypeOfNbt(DhNbtType type)
{
    switch(type)
    {
        case Litematica: return _("Litematica");
        case NBTStruct : return _("NBT Struct");
        case Schematics: return _("Schematics");
        case Others    : return _("Others");
    }
}

ManageNBT::ManageNBT()
    : QObject(), mui(new ManageUI())
{
    uuidList = nbt_info_list_get_uuid_list();
    mui->setWindowTitle(_("Manage NBT"));
    model = new QStandardItemModel();
    QObject::connect(mui, &ManageUI::add, this, &ManageNBT::add_triggered);
    QObject::connect(mui, &ManageUI::remove, this, &ManageNBT::remove_triggered);
    QObject::connect(mui, &ManageUI::save, this, &ManageNBT::save_triggered);
    QObject::connect(mui, &ManageUI::refresh, this, &ManageNBT::refresh_triggered);
    QObject::connect(mui, &ManageUI::showSig, this, &ManageNBT::showSig_triggered);
    QObject::connect(mui, &ManageUI::closeSig, this, &ManageNBT::closeSig_triggered);
}

ManageNBT::~ManageNBT()
{
    delete mui;
    delete model;
}

void ManageNBT::show()
{
    mui->show();
}

void ManageNBT::add_triggered()
{
    QString dir = QFileDialog::getOpenFileName(mui, _("Select a file"), nullptr, _("Litematic file (*.litematic);;NBT File (*.nbt)"));
    if(!dir.isEmpty())
    {
        GFile* file = g_file_new_for_path(dir.toStdString().c_str());
        char* defaultDescription = g_file_get_basename(file);
        QString description = QInputDialog::getText(mui, _("Enter Desciption"), _("Enter desciption for the NBT file."), QLineEdit::Normal, defaultDescription);

        if(description.isEmpty())
        {
            QMessageBox::critical(mui, _("No Description Entered!"), _("No desciption entered! Will not add the NBT file!"));
        }
        else
        {
            char* content;
            gsize len;
            g_file_load_contents(file, NULL, &content, &len, NULL, NULL);
            NBT* newNBT = NBT_Parse((guint8*)content, len);
            if(newNBT)
            {
                nbt_info_new(newNBT, g_date_time_new_now_local(), description.toStdString().c_str());
            }
            else QMessageBox::critical(mui, _("Not Valid File!"), _("Not a valid NBT file!"));
            g_free(content);
            updateModel();
            mui->updateModel(model);
        }
        g_object_unref(file);
    }
}

void ManageNBT::remove_triggered(int row)
{
    uuidList = nbt_info_list_get_uuid_list();
    char* uuid = (char*)g_list_nth_data(uuidList->list, row);
    g_rw_lock_writer_unlock(&uuidList->lock);
    nbt_info_list_remove_item(uuid);
    g_rw_lock_writer_trylock(&uuidList->lock);
    updateModel();
    mui->updateModel(model);
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

void ManageNBT::save_triggered(int row)
{
    QString filepos = QFileDialog::getSaveFileName(mui, _("Save File"));
    if(!filepos.isEmpty())
    {
        NbtInfo* info = nbt_info_list_get_nbt_info((char*)g_list_nth_data(uuidList->list, row));
        if(g_rw_lock_writer_trylock(&info->info_lock))
        {
            dhlrc_nbt_save(info->root, filepos.toLocal8Bit());
            g_rw_lock_writer_unlock(&info->info_lock);
        }
        else QMessageBox::critical(mui, _("Info Locked!"), _("The NBT info is locked!"));
    }
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