#include "blocklistui.h"
#include "ui_blocklistui.h"
#include <QMessageBox>
#include <qmessagebox.h>

static bool ignoreAir = false;

static gboolean find_block(gconstpointer a, gconstpointer b)
{
    BlockInfo* info = (BlockInfo*)a;
    return g_str_equal(info->id_name, b);
}

BlockListUI::BlockListUI(LiteRegion* lr, QWidget *parent)
    : QWidget(parent),
    ui(new Ui::BlockListUI)
{
    auto btn = QMessageBox::question(this, _("Ignore Air?"), _("Do you want to ignore air?"));
    if(btn == QMessageBox::Yes) ignoreAir = true;
    ui->setupUi(this);
    setList(lr);
    drawList();
}

BlockListUI::~BlockListUI()
{
    delete ui;
    region_free(region);
}

void BlockListUI::setList(LiteRegion* lr)
{
    region = region_new_from_lite_region(lr);
    if(ignoreAir)
    {
        bool not_found = false;
        do 
        {
            guint index = 0;
            bool found = g_ptr_array_find_with_equal_func(region->block_info_array, "minecraft:air", find_block, &index);
            if(found)
                g_ptr_array_remove_index(region->block_info_array, index);
            else not_found = true;
        }while (!not_found);
    }
}

void BlockListUI::drawList()
{
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
        ui->tableWidget->setItem(i, 0, item0);
        ui->tableWidget->setItem(i, 1, item1);
        ui->tableWidget->setItem(i, 2, item2);
        ui->tableWidget->setItem(i, 3, item3);
        ui->tableWidget->setItem(i, 4, item4);
        ui->tableWidget->setItem(i, 5, item5);
        ui->tableWidget->setItem(i, 6, item6);
    }
}