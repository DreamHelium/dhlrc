#include "manageui.h"
#include "glib.h"
#include "ui_manageui.h"
#include <qevent.h>
#include <qfiledialog.h>
#include <qinputdialog.h>
#include <qmessagebox.h>
#include <qobject.h>
#include <qpushbutton.h>
#include <QStandardItemModel>
#include <qstandarditemmodel.h>
#include "../translation.h"
#include "../nbt_info.h"
#include <QFileDialog>
#include <qwidget.h>
#include "../nbt_litereader.h"

static void noAction()
{}

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

static DhList* uuidList = nullptr;

ManageUI::ManageUI(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ManageUI)
{
    ui->setupUi(this);
    initSignalSlots();
    initModel();
    ui->tableView->setModel(model);
    uuidList = nbt_info_list_get_uuid_list();
}

ManageUI::~ManageUI()
{
    delete ui;
}

void ManageUI::initSignalSlots()
{
    QObject::connect(ui->addBtn, &QPushButton::clicked, this, &ManageUI::addBtn_clicked);
    QObject::connect(ui->closeBtn, &QPushButton::clicked, this, &ManageUI::close);
    QObject::connect(ui->refreshBtn, &QPushButton::clicked, this, &ManageUI::updateModel);
    QObject::connect(ui->removeBtn, &QPushButton::clicked, this, &ManageUI::removeBtn_clicked);
    QObject::connect(ui->saveBtn, &QPushButton::clicked, this, &ManageUI::saveBtn_clicked);
}

void ManageUI::initModel()
{
    model = new QStandardItemModel(0, 3);
    QStringList list;
    list << _("Description") << _("UUID") << _("Time") << _("Type");
    model->setHorizontalHeaderLabels(list);
    QList<QList<QStandardItem*>> itemList = getList();
    for(int i = 0 ; i < itemList.length() ; i++)
    {
        model->appendRow(itemList[i]);
    }
}

QList<QList<QStandardItem*>> ManageUI::getList()
{
    QList<QList<QStandardItem*>> ret;

    uuidList = nbt_info_list_get_uuid_list();
    GList* fullList = uuidList ? uuidList->list : nullptr;
    uuidList ? g_rw_lock_writer_unlock(&uuidList->lock) : noAction();
    if(uuidList? g_rw_lock_reader_trylock(&uuidList->lock): true)
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
            ret.append(list);
        }
        uuidList? g_rw_lock_reader_unlock(&uuidList->lock): noAction();
    }
    else
    {
        QStandardItem* description = new QStandardItem;
        QStandardItem* uuid = new QStandardItem;
        QStandardItem* time = new QStandardItem;
        QStandardItem* type = new QStandardItem;
        description->setData(QString(_("locked")), 0);
        uuid->setData(QString(_("locked")), 0);
        time->setData(QString(_("locked")), 0);
        type->setData(QString(_("locked")), 0);
        QList<QStandardItem*> list = {description, uuid, time, type};
        ret.append(list);
    }
    uuidList ? g_rw_lock_writer_trylock(&uuidList->lock) : false;
    return ret;
}

void ManageUI::updateModel()
{
    model->clear();
    QStringList list;
    list << _("Description") << _("UUID") << _("Time") << _("Type");
    model->setHorizontalHeaderLabels(list);
    QList<QList<QStandardItem*>> itemList = getList();
    for(int i = 0 ; i < itemList.length() ; i++)
    {
        model->appendRow(itemList[i]);
    }
}

void ManageUI::addBtn_clicked()
{
    QString dir = QFileDialog::getOpenFileName(this, _("Select a file"), nullptr, _("Litematic file (*.litematic)"));
    if(!dir.isEmpty())
    {
        GFile* file = g_file_new_for_path(dir.toStdString().c_str());
        char* defaultDescription = g_file_get_basename(file);
        QString description = QInputDialog::getText(this, _("Enter Desciption"), _("Enter desciption for the NBT file."), QLineEdit::Normal, defaultDescription);

        if(description.isEmpty())
        {
            QMessageBox::critical(this, _("No Description Entered!"), _("No desciption entered! Will not add the NBT file!"));
        }
        else
        {
            char* content;
            gsize len;
            g_file_load_contents(file, NULL, &content, &len, NULL, NULL);
            NBT* newNBT = NBT_Parse((guint8*)content, len);
            if(newNBT)
            {
                uuidList ? g_rw_lock_writer_unlock(&uuidList->lock) : noAction();
                nbt_info_new(newNBT, g_date_time_new_now_local(), description.toStdString().c_str());
                uuidList = nbt_info_list_get_uuid_list();
                g_rw_lock_writer_trylock(&uuidList->lock);
            }
            else QMessageBox::critical(this, _("Not Valid File!"), _("Not a valid NBT file!"));
            g_free(content);
            updateModel();
        }
        g_object_unref(file);
    }
}

void ManageUI::removeBtn_clicked()
{
    auto index = ui->tableView->selectionModel()->currentIndex();
    if(index.isValid())
    {
        int row = index.row();
        char* uuid = (char*)g_list_nth_data(uuidList->list, row);
        g_rw_lock_writer_unlock(&uuidList->lock);
        nbt_info_list_remove_item(uuid);
        g_rw_lock_writer_trylock(&uuidList->lock);
        updateModel();
    }
    else QMessageBox::critical(this, _("No Selected Row!"), _("No row is selected!"));
}

void ManageUI::closeEvent(QCloseEvent* event)
{
    qDebug() << "closed";
    uuidList ? g_rw_lock_writer_unlock(&uuidList->lock) : noAction();
    QWidget::closeEvent(event);
}

void ManageUI::showEvent(QShowEvent* event)
{
    if(!isMinimized())
    {
        uuidList ? g_rw_lock_writer_trylock(&uuidList->lock) : false;
        qDebug() << "show";
    }
    QWidget::showEvent(event);
}

void ManageUI::saveBtn_clicked()
{
    auto index = ui->tableView->selectionModel()->currentIndex();
    if(index.isValid())
    {
        int row = index.row();
        QString filepos = QFileDialog::getSaveFileName(this, _("Save File"));
        if(!filepos.isEmpty())
        {
            NbtInfo* info = nbt_info_list_get_nbt_info((char*)g_list_nth_data(uuidList->list, row));
            if(g_rw_lock_writer_trylock(&info->info_lock))
            {
                dhlrc_nbt_save(info->root, filepos.toLocal8Bit());
                g_rw_lock_writer_unlock(&info->info_lock);
            }
            else QMessageBox::critical(this, _("Info Locked!"), _("The NBT info is locked!"));
        }
    }
    else QMessageBox::critical(this, _("No Selected Row!"), _("No row is selected!"));
}