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
#include "../nbt_interface/nbt_interface.h"
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

static QString ret_var(NbtInstance* instance)
{
    auto type = dh_nbt_get_type(instance);
    QString ret;
    int len = 0;
    switch(type)
    {
        case DH_TYPE_Byte:
            return QString::number(dh_nbt_instance_get_byte(instance));
        case DH_TYPE_Int:
            return QString::number(dh_nbt_instance_get_int(instance));
        case DH_TYPE_Short:
            return QString::number(dh_nbt_instance_get_short(instance));
        case DH_TYPE_Long:
            return QString::number(dh_nbt_instance_get_long(instance));
        case DH_TYPE_Float:
            return QString::number(dh_nbt_instance_get_float(instance));
        case DH_TYPE_Double:
            return QString::number(dh_nbt_instance_get_double(instance));
        case DH_TYPE_String:
        {
            const char* s = dh_nbt_instance_get_string(instance);
            ret = s;
            free((void*)s);
            return ret;
        }
        case DH_TYPE_Byte_Array:
        {
            len = 0;
            auto arr_b = dh_nbt_instance_get_byte_array(instance, &len);
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
            auto arr_i = dh_nbt_instance_get_int_array(instance, &len);
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
            auto arr_i = dh_nbt_instance_get_long_array(instance, &len);
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
    uuid = common_info_list_get_uuid(DH_TYPE_NBT_INTERFACE);
    if(!uuid.isEmpty())
        instance = (NbtInstance*)common_info_get_data(DH_TYPE_NBT_INTERFACE, uuid.toUtf8());
    
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
    addModelTree(instance, modelRoot);
}

void NbtReaderUI::addModelTree(NbtInstance* instance, QStandardItem* iroot)
{
    for(; dh_nbt_instance_is_non_null(instance) ; dh_nbt_instance_next(instance))
    {
        const char* key = dh_nbt_instance_get_key(instance);
        QString str = key ? key : "(NULL)";
        
        auto item = new QStandardItem(str);
        item->setEditable(false);
        if(dh_nbt_instance_is_type(instance, DH_TYPE_Compound) || 
           dh_nbt_instance_is_type(instance, DH_TYPE_List))
        {
            auto new_instance = dh_nbt_instance_dup(instance);
            dh_nbt_instance_child(new_instance);
            addModelTree(new_instance, item);
            /* Go back */
            dh_nbt_instance_free(new_instance);
        }
        iroot->appendRow(item); /* Add item to root */
    }
}

void NbtReaderUI::treeview_clicked()
{
    auto index = ui->treeView->selectionModel()->selection();
    auto selection = proxyModel->mapSelectionToSource(index);
    auto indexx = selection.indexes()[0];
    QList<int> treeRow;
    for(; indexx.isValid() ; indexx = indexx.parent())
    {
        treeRow.prepend(indexx.row());
    }
    dh_nbt_instance_goto_root(instance);
    for(int j = 0 ; j < treeRow.length() ; j++)
    {
        if(j != 0 ) dh_nbt_instance_child(instance);
        auto row = treeRow[j];
        for(int i = 0 ; i < row ; i++)
            dh_nbt_instance_next(instance);
    }
    ui->typeLabel->setText(ret_type_instance(dh_nbt_get_type(instance)));
    const char* key = dh_nbt_instance_get_key(instance);
    ui->keyLabel->setText(key ? key : "(NULL)");
    ui->valueLabel->setText(ret_var(instance));
}

void NbtReaderUI::textChanged_cb(QString str)
{
    proxyModel->setFilterRegularExpression(str);
}