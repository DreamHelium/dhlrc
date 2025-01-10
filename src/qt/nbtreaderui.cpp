#include "nbtreaderui.h"
#include "ui_nbtreaderui.h"
#include "../nbt_info.h"
#include <qitemselectionmodel.h>
#include <qstandarditemmodel.h>
#include <qtreeview.h>
#include <qvariant.h>
#include "../common_info.h"
#include "../nbt_interface/nbt_interface.h"

static QString uuid;

static QString ret_type(NBT* root)
{
    auto type = root->type;
    switch(type)
    {
        case TAG_Compound : return "Compound";
        case TAG_Int      : return "Int";
        case TAG_List     : return "List";
        case TAG_Byte     : return "Byte";
        case TAG_Byte_Array: return "Byte Array";
        case TAG_Int_Array: return "Int Array";
        case TAG_Long_Array: return "Long Array";
        case TAG_Long     : return "Long";
        case TAG_String   : return "String";
        case TAG_Double   : return "Double";
        case TAG_Float    : return "Float";
        case TAG_Short    : return "Short";
        default           : return "???";    
    }
}

static QString ret_var(NBT* root)
{
    auto type = root->type;
    if(type >= TAG_Byte && type <= TAG_Long)
        return QString("%1").arg(root->value_i);
    else if(type == TAG_Float || type == TAG_Double)
        return QString("%1").arg(root->value_d);
    else if(type == TAG_String)
        return QString((char*)root->value_a.value);
    else return QString("");
}

NbtReaderUI::NbtReaderUI(QWidget *parent)
    : QWidget(parent),
    ui(new Ui::NbtReaderUI)
{
    ui->setupUi(this);
    uuid = common_info_list_get_uuid(DH_TYPE_NBT_INTERFACE);
    if(!uuid.isEmpty())
    {
        auto instance = (NbtInstance*)common_info_get_data(DH_TYPE_NBT_INTERFACE, uuid.toUtf8());
        root = (NBT*)dh_nbt_instance_get_real_original_nbt(instance);
    } 
    
    model = new QStandardItemModel();
    initModel();
    ui->treeView->setModel(model);
    auto selectionModel = ui->treeView->selectionModel();
    QObject::connect(selectionModel, &QItemSelectionModel::selectionChanged, this, &NbtReaderUI::treeview_clicked);
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
    addModelTree(root, modelRoot);
}

void NbtReaderUI::addModelTree(NBT* root, QStandardItem* iroot)
{
    for(; root ; root = root = root->next)
    {
        QString str = QString((char*)root->key ? (char*)(root->key) : "(null)");
        auto item = new QStandardItem(str);
        item->setEditable(false);
        if(root->type == TAG_Compound || root->type == TAG_List)
        {
            addModelTree(root->child, item);
        }
        iroot->appendRow(item); /* Add item to root */
    }
}

void NbtReaderUI::treeview_clicked()
{
    auto indexx = ui->treeView->selectionModel()->currentIndex();
    QList<int> treeRow;
    for(; indexx.isValid() ; indexx = indexx.parent())
    {
        treeRow.prepend(indexx.row());
    }
    NBT* child = root;
    for(int j = 0 ; j < treeRow.length() ; j++)
    {
        if(j != 0 ) child = child->child;
        auto row = treeRow[j];
        for(int i = 0 ; i < row ; i++)
            child = child->next;
    }
    ui->typeLabel->setText(ret_type(child));
    ui->keyLabel->setText(QString((char*)child->key ? (char*)(child->key) : "(null)"));
    ui->valueLabel->setText(ret_var(child));
}