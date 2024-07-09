#include "blocklistui.h"
#include "ui_blocklistui.h"
#include <QMessageBox>
#include <qmessagebox.h>

static bool ignoreAir = false;

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
    for(int i = 0; i < list.length() ; i++)
    {
        delete list[i];
    }
}

void BlockListUI::setList(LiteRegion* lr)
{
    for(int y = 0 ; y < lr->region_size.y ; y++)
    {
        for(int z = 0 ; z < lr->region_size.z ; z++)
        {
            for(int x = 0; x < lr->region_size.x ; x++)
            {
                Block* block = new Block();
                block->id = lite_region_block_index(lr, x, y, z);
                block->palette = lite_region_block_id(lr, block->id);
                if(block->palette == 0 && ignoreAir )
                {
                    delete block;
                    continue;
                }
                block->x = x;
                block->y = y;
                block->z = z;
                block->idName = lr->replaced_blocks->val[block->palette];
                block->trName = trm(block->idName.toStdString().c_str());
                list.append(block);
            }
        }
    }
}

void BlockListUI::drawList()
{
    ui->tableWidget->setRowCount(list.length());
    for(int i = 0 ; i < list.length() ; i++)
    {
        QTableWidgetItem* item0 = new QTableWidgetItem(QString::number(list[i]->id));
        QTableWidgetItem* item1 = new QTableWidgetItem(list[i]->idName);
        QTableWidgetItem* item2 = new QTableWidgetItem(list[i]->trName);
        QTableWidgetItem* item3 = new QTableWidgetItem(QString::number(list[i]->x));
        QTableWidgetItem* item4 = new QTableWidgetItem(QString::number(list[i]->y));
        QTableWidgetItem* item5 = new QTableWidgetItem(QString::number(list[i]->z));
        QTableWidgetItem* item6 = new QTableWidgetItem(QString::number(list[i]->palette));
        ui->tableWidget->setItem(i, 0, item0);
        ui->tableWidget->setItem(i, 1, item1);
        ui->tableWidget->setItem(i, 2, item2);
        ui->tableWidget->setItem(i, 3, item3);
        ui->tableWidget->setItem(i, 4, item4);
        ui->tableWidget->setItem(i, 5, item5);
        ui->tableWidget->setItem(i, 6, item6);
    }
}