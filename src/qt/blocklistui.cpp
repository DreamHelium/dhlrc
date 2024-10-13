#include "blocklistui.h"
#include "ui_blocklistui.h"
#include <QMessageBox>
#include <qline.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qobject.h>
#include <QList>
#include "../translation.h"
#include "../region_info.h"

static bool ignoreAir = false;

static gboolean find_block(gconstpointer a, gconstpointer b)
{
    BlockInfo* info = (BlockInfo*)a;
    return g_str_equal(info->id_name, b);
}

BlockListUI::BlockListUI(QWidget *parent)
    : QWidget(parent),
    ui(new Ui::BlockListUI)
{
    // auto btn = QMessageBox::question(this, _("Ignore Air?"), _("Do you want to ignore air?"));
    // if(btn == QMessageBox::Yes) ignoreAir = true;
    ui->setupUi(this);
    QObject::connect(ui->lineEdit, &QLineEdit::textChanged, this, &BlockListUI::textChanged_cb);
    // setList(region);
    drawList();
}

BlockListUI::~BlockListUI()
{
    ui->tableWidget->clear();
    delete ui;
}

void BlockListUI::setList(Region* region)
{
    // if(ignoreAir)
    // {
    //     bool not_found = false;
    //     do 
    //     {
    //         guint index = 0;
    //         bool found = g_ptr_array_find_with_equal_func(region->block_info_array, "minecraft:air", find_block, &index);
    //         if(found)
    //             g_ptr_array_remove_index(region->block_info_array, index);
    //         else not_found = true;
    //     }while (!not_found);
    // }
}

void BlockListUI::drawList()
{
    Region* region = region_info_list_get_region_info(region_info_list_get_uuid())->root;
    ui->tableWidget->setRowCount(region->block_info_array->len);
    for(int i = 0 ; i < region->block_info_array->len ; i++)
    {
        BlockInfo* info = (BlockInfo*)region->block_info_array->pdata[i];
        QTableWidgetItem* item0 = new QTableWidgetItem(QString::number(info->index));
        QTableWidgetItem* item1 = new QTableWidgetItem(info->id_name);
        QTableWidgetItem* item2 = new QTableWidgetItem(trm(info->id_name));
        QTableWidgetItem* item3 = new QTableWidgetItem(QString::number(info->pos->x));
        QTableWidgetItem* item4 = new QTableWidgetItem(QString::number(info->pos->y));
        QTableWidgetItem* item5 = new QTableWidgetItem(QString::number(info->pos->z));
        QTableWidgetItem* item6 = new QTableWidgetItem(QString::number(info->palette));
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
        ui->tableWidget->setItem(i, 0, item0);
        ui->tableWidget->setItem(i, 1, item1);
        ui->tableWidget->setItem(i, 2, item2);
        ui->tableWidget->setItem(i, 3, item3);
        ui->tableWidget->setItem(i, 4, item4);
        ui->tableWidget->setItem(i, 5, item5);
        ui->tableWidget->setItem(i, 6, item6);
    }
}

void BlockListUI::textChanged_cb(const QString & str)
{
    auto items = ui->tableWidget->findItems(str, Qt::MatchContains);
    for(int i = 0 ; i < ui->tableWidget->rowCount() ; i++)
    {
        bool hide = true;
        for(int j = 0 ; j < items.length() ; j++)
        {
            if(items[j]->row() == i && (items[j]->column() == 1 || items[j]->column() == 2))
            {
                hide = false;
                break;
            }
        }
        ui->tableWidget->setRowHidden(i, hide);
    }
}