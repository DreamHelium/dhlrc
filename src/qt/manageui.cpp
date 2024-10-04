#include "manageui.h"
#include "ui_manageui.h"
#include <qinputdialog.h>
#include <qmessagebox.h>
#include <qobject.h>
#include <qpushbutton.h>
#include <QStandardItemModel>
#include <qstandarditemmodel.h>
#include "../translation.h"
#include "../nbt_info.h"
#include <QFileDialog>

ManageUI::ManageUI(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ManageUI)
{
    ui->setupUi(this);
    initSignalSlots();
    initModel();
    ui->tableView->setModel(model);
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
}

void ManageUI::initModel()
{
    model = new QStandardItemModel(0, 3);
    QStringList list;
    list << _("Description") << _("UUID") << _("Time");
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

    GList* fullList = nbt_info_list_get_uuid_list();
    guint len = fullList ? g_list_length(fullList) : 0;
    for(int i = 0 ; i < len ; i++)
    {
        NbtInfo* info = nbt_info_list_get_nbt_info((gchar*)g_list_nth_data(fullList, i));
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
        ret.append(list);
    }

    return ret;
}

void ManageUI::updateModel()
{
    model->clear();
    QStringList list;
    list << _("Description") << _("UUID") << _("Time");
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
            nbt_info_new(newNBT, g_date_time_new_now_local(), description.toStdString().c_str());
            g_free(content);
            updateModel();
        }
        g_object_unref(file);
    }
}