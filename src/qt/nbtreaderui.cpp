#include "nbtreaderui.h"
#include "dhtreefilter.h"
#include "ui_nbtreaderui.h"
#include <qitemselectionmodel.h>
#include <qnamespace.h>
#include <qsortfilterproxymodel.h>
#include <qstandarditemmodel.h>
#include <qtreeview.h>
#include <qvariant.h>
#include "../common_info.h"
#include "../nbt_interface_cpp/nbt_interface.hpp"
#include <cstdlib>
#include <QDebug>
#include <QLineEdit>

static QString uuid;

static QString ret_type_instance(DhNbtType type)
{
    switch(type)
    {
        case DH_TYPE_Compound : return "Compound";
        case DH_TYPE_Int      : return "Int";
        case DH_TYPE_List     : return "List";
        case DH_TYPE_Byte     : return "Byte";
        case DH_TYPE_Byte_Array: return "Byte Array";
        case DH_TYPE_Int_Array: return "Int Array";
        case DH_TYPE_Long_Array: return "Long Array";
        case DH_TYPE_Long     : return "Long";
        case DH_TYPE_String   : return "String";
        case DH_TYPE_Double   : return "Double";
        case DH_TYPE_Float    : return "Float";
        case DH_TYPE_Short    : return "Short";
        default           : return "???";    
    }
}

static QString ret_var(DhNbtInstance instance)
{
    auto type = instance.get_type();
    QString ret;
    int len = 0;
    switch(type)
    {
        case DH_TYPE_Byte:
            return QString::number(instance.get_byte());
        case DH_TYPE_Int:
            return QString::number(instance.get_int());
        case DH_TYPE_Short:
            return QString::number(instance.get_short());
        case DH_TYPE_Long:
            return QString::number(instance.get_long());
        case DH_TYPE_Float:
            return QString::number(instance.get_float());
        case DH_TYPE_Double:
            return QString::number(instance.get_double());
        case DH_TYPE_String:
        {
            const char* s = instance.get_string();
            ret = s;
            return ret;
        }
        case DH_TYPE_Byte_Array:
        {
            len = 0;
            auto arr_b = instance.get_byte_array(len);
            for(int i = 0 ; i < len ; i ++)
            {
                ret.append(QString::number(arr_b[i]));
                ret.append('\n');
            }
            return ret;
        }
        case DH_TYPE_Int_Array:
        {
            len = 0;
            auto arr_i = instance.get_int_array(len);
            for(int i = 0 ; i < len ; i ++)
            {
                ret.append(QString::number(arr_i[i]));
                ret.append('\n');
            }
            return ret;
        }
        case DH_TYPE_Long_Array:
        {
            len = 0;
            auto arr_i = instance.get_long_array(len);
            for(int i = 0 ; i < len ; i ++)
            {
                ret.append(QString::number(arr_i[i]));
                ret.append('\n');
            }
            return ret;
        }
        case DH_TYPE_List:
        case DH_TYPE_Compound:
        default:
            return "";
    }
}

NbtReaderUI::NbtReaderUI(QWidget *parent)
    : QWidget(parent),
    ui(new Ui::NbtReaderUI)
{
    ui->setupUi(this);
    uuid = common_info_list_get_uuid(DH_TYPE_NBT_INTERFACE_CPP);
    if(!uuid.isEmpty())
        instance = (DhNbtInstance*)common_info_get_data(DH_TYPE_NBT_INTERFACE_CPP, uuid.toUtf8());
    
    model = new QStandardItemModel(this);
    proxyModel = new DhTreeFilter(this);
    initModel();
    proxyModel->setSourceModel(model);
    ui->treeView->setModel(proxyModel);
    auto selectionModel = ui->treeView->selectionModel();
    QObject::connect(selectionModel, &QItemSelectionModel::selectionChanged, this, &NbtReaderUI::treeview_clicked);
    QObject::connect(ui->lineEdit, &QLineEdit::textChanged, this, &NbtReaderUI::textChanged_cb);
}

NbtReaderUI::~NbtReaderUI()
{
    model->clear();
    delete model;
    delete ui;
}

void NbtReaderUI::initModel()
{
    auto modelRoot = model->invisibleRootItem();
    addModelTree(*instance, modelRoot);
}

void NbtReaderUI::addModelTree(DhNbtInstance instance, QStandardItem* iroot)
{
    for(; instance.is_non_null() ; instance.next())
    {
        const char* key = instance.get_key();
        QString str = key ? key : "(NULL)";
        
        auto item = new QStandardItem(str);
        item->setEditable(false);
        if(instance.is_type(DH_TYPE_Compound) || 
           instance.is_type(DH_TYPE_List))
        {
            auto new_instance(instance);
            new_instance.child();
            addModelTree(new_instance, item);
        }
        iroot->appendRow(item); /* Add item to root */
    }
}

void NbtReaderUI::treeview_clicked()
{
    auto index = ui->treeView->selectionModel()->selection();
    auto selection = proxyModel->mapSelectionToSource(index);
    if(!selection.isEmpty())
    {
        auto indexx = selection.indexes()[0];
        QList<int> treeRow;
        for(; indexx.isValid() ; indexx = indexx.parent())
        {
            treeRow.prepend(indexx.row());
        }
        auto instance_dup(*instance);
        instance_dup.goto_root();
        for(int j = 0 ; j < treeRow.length() ; j++)
        {
            if(j != 0 ) instance_dup.child();
            auto row = treeRow[j];
            for(int i = 0 ; i < row ; i++)
                instance_dup.next();
        }
        ui->typeLabel->setText(ret_type_instance(instance_dup.get_type()));
        const char* key = instance_dup.get_key();
        ui->keyLabel->setText(key ? key : "(NULL)");
        ui->valueLabel->setText(ret_var(instance_dup));
    }
}

void NbtReaderUI::textChanged_cb(QString str)
{
    proxyModel->setFilterRegularExpression(str);
}