#include "blocklistui.h"
#include "../translation.h"
#include "ui_blocklistui.h"
#include <QList>
#include <QMessageBox>
#include <qline.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qsortfilterproxymodel.h>
#include <qstandarditemmodel.h>

#include <utility.h>

static gboolean find_block(gconstpointer a, gconstpointer b)
{
    BlockInfo* info = (BlockInfo*)a;
    return g_str_equal(info->id_name, b);
}

BlockListUI::BlockListUI(Region* region, QWidget *parent)
    : QWidget(parent),
    ui(new Ui::BlockListUI)
{
    this->region = region;
    dhlrc_update_transfile (dh::getVersion(region->data_version).toUtf8());
    model = new QStandardItemModel(this);
    proxyModel = new QSortFilterProxyModel(this);
    auto btn = QMessageBox::question(this, _("Ignore Air?"), _("Do you want to ignore air?"));
    if(btn == QMessageBox::Yes) ignoreAir = true;
    ui->setupUi(this);
    QObject::connect(ui->lineEdit, &QLineEdit::textChanged, this, &BlockListUI::textChanged_cb);
    drawList();
}

BlockListUI::~BlockListUI()
{
    delete ui;
}

void BlockListUI::drawList()
{
    QStringList stringList;
    stringList << _("id") << _("id Name") << _("Block Name") << _("x")
               << _("y") << _("z") << _("Palette");
    model->setHorizontalHeaderLabels(stringList);
    for(int i = 0 ; i < region->block_info_array->len ; i++)
    {
        BlockInfo* info = (BlockInfo*)region->block_info_array->pdata[i];
        if(!strcmp(info->id_name, "minecraft:air") && ignoreAir)
            continue;
        QStandardItem* item0 = new QStandardItem(QString::number(info->index));
        QStandardItem* item1 = new QStandardItem(info->id_name);
        QStandardItem* item2 = new QStandardItem(trm(info->id_name));
        QStandardItem* item3 = new QStandardItem(QString::number(info->pos->x));
        QStandardItem* item4 = new QStandardItem(QString::number(info->pos->y));
        QStandardItem* item5 = new QStandardItem(QString::number(info->pos->z));
        QStandardItem* item6 = new QStandardItem(QString::number(info->palette));
        Palette* palette = (Palette*)region->palette_array->pdata[info->palette];\
        QString str = QString();
        if(palette->property_data)
        {
            for(int j = 0 ; j < palette->property_data->num ; j++)
            {
                /* `tr()` is same as `gettext()` in this program...... */
                str += tr(palette->property_name->val[j]);
                str += ": ";
                str += tr(palette->property_data->val[j]);
                if(j != palette->property_name->num - 1)
                    str += "\n";
            }
        }

        item6->setToolTip(str);
        QList<QStandardItem*> itemList = {item0, item1, item2, item3, item4, item5, item6};
        model->appendRow(itemList);
    }
    proxyModel->setSourceModel(model);
    ui->tableView->setModel(proxyModel);
    proxyModel->setFilterKeyColumn(-1);
}

void BlockListUI::textChanged_cb(const QString & str)
{
    proxyModel->setFilterRegularExpression(str);
}