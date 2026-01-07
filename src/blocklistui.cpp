#include "blocklistui.h"
#include "../translation.h"
#include "ui_blocklistui.h"
#include <QList>
#include <QScrollBar>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qsortfilterproxymodel.h>
#include <qstandarditemmodel.h>

#include <utility.h>

BlockListUI::BlockListUI (Region *region, const char *large_version,
                          QWidget *parent)
    : QWidget (parent), ui (new Ui::BlockListUI)
{
  this->region = region;
  this->large_version = large_version;
  model = new QStandardItemModel (this);
  proxyModel = new QSortFilterProxyModel (this);
  auto btn = QMessageBox::question (this, _ ("Ignore Air?"),
                                    _ ("Do you want to ignore air?"));
  if (btn == QMessageBox::Yes)
    ignoreAir = true;
  ui->setupUi (this);
  QObject::connect (ui->lineEdit, &QLineEdit::textChanged, this,
                    &BlockListUI::textChanged_cb);
  drawList ();
  ui->tableView->setDND (false);
}

BlockListUI::~BlockListUI ()
{
  model->clear ();
  delete model;
  delete ui;
}

void
BlockListUI::drawList ()
{
  QStringList stringList;
  if (dhlrc_mcdata_enabled () && large_version)
    stringList << _ ("id") << _ ("id Name") << _ ("Block Name") << _ ("x")
               << _ ("y") << _ ("z") << _ ("Palette");
  else
    stringList << _ ("id") << _ ("id Name") << _ ("x") << _ ("y") << _ ("z")
               << _ ("Palette");
  model->setHorizontalHeaderLabels (stringList);
  int size = region->region_size->x * region->region_size->y
             * region->region_size->z;
  int x = 0;
  int y = 0;
  int z = 0;
  for (int i = 0; i < size; i++)
    {
      auto id_name = region_get_id_name (region, i);
      auto palette_num = region_get_block_palette (region, i);
      if (!strcmp (id_name, "minecraft:air") && ignoreAir)
        continue;
      QStandardItem *item2 = nullptr;
      QStandardItem *item0 = new QStandardItem (QString::number (i));
      QStandardItem *item1 = new QStandardItem (id_name);

      if (dhlrc_mcdata_enabled () && large_version)
        item2 = new QStandardItem (mctr (id_name, large_version));
      QStandardItem *item3 = new QStandardItem (QString::number (x));
      QStandardItem *item4 = new QStandardItem (QString::number (y));
      QStandardItem *item5 = new QStandardItem (QString::number (z));
      QStandardItem *item6 = new QStandardItem (QString::number (palette_num));
      Palette *palette = (Palette *)region->palette_array->pdata[palette_num];
      QString str = QString ();
      if (palette->property_data)
        {
          for (int j = 0; j < palette->property_data->num; j++)
            {
              str += gettext (palette->property_name->val[j]);
              str += ": ";
              str += gettext (palette->property_data->val[j]);
              if (j != palette->property_name->num - 1)
                str += "\n";
            }
        }

      item6->setToolTip (str);
      QList<QStandardItem *> itemList;
      if (item2)
        itemList = { item0, item1, item2, item3, item4, item5, item6 };
      else
        itemList = { item0, item1, item3, item4, item5, item6 };

      for (auto item : itemList)
        item->setEditable (false);
      model->appendRow (itemList);
      if (x < region->region_size->x - 1)
        x++;
      else if (z < region->region_size->z - 1)
        {
          x = 0;
          z++;
        }
      else if (y < region->region_size->y - 1)
        {
          x = 0;
          z = 0;
          y++;
        }
    }
  proxyModel->setSourceModel (model);
  ui->tableView->setModel (proxyModel);
  proxyModel->setFilterKeyColumn (-1);

  ui->tableView->horizontalHeader ()->setSectionResizeMode (
      1, QHeaderView::Stretch);
}

void
BlockListUI::resizeRow ()
{
}

void
BlockListUI::textChanged_cb (const QString &str)
{
  proxyModel->setFilterRegularExpression (str);
}