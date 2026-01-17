#include "blocklistui.h"
#define _(str) gettext (str)
#include "ui_blocklistui.h"
#include <QList>
#include <QScrollBar>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qsortfilterproxymodel.h>
#include <qstandarditemmodel.h>
#include <region.h>

#include <utility.h>

BlockListUI::BlockListUI (void *region, const char *large_version,
                          QWidget *parent)
    : QWidget (parent), ui (new Ui::BlockListUI), region (region)
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
  if (large_version)
    stringList << _ ("id") << _ ("id Name") << _ ("Block Name") << _ ("x")
               << _ ("y") << _ ("z") << _ ("Palette");
  else
    stringList << _ ("id") << _ ("id Name") << _ ("x") << _ ("y") << _ ("z")
               << _ ("Palette");
  model->setHorizontalHeaderLabels (stringList);
  int regionX = region_get_x (region);
  int regionY = region_get_y (region);
  int regionZ = region_get_z (region);
  int size = regionX * regionY * regionZ;
  int x = 0;
  int y = 0;
  int z = 0;
  for (int i = 0; i < size; i++)
    {
      auto id = region_get_block_id_by_index (region, i);
      auto id_name = region_get_palette_id_name (region, id);
      if (!strcmp (id_name, "minecraft:air") && ignoreAir)
        {
          string_free (id_name);
          continue;
        }
      QStandardItem *item2 = nullptr;
      QStandardItem *item0 = new QStandardItem (QString::number (i));
      QStandardItem *item1 = new QStandardItem (id_name);

      // if ( large_version)
        // item2 = new QStandardItem (mctr (id_name, large_version));
      QStandardItem *item3 = new QStandardItem (QString::number (x));
      QStandardItem *item4 = new QStandardItem (QString::number (y));
      QStandardItem *item5 = new QStandardItem (QString::number (z));
      QStandardItem *item6 = new QStandardItem (QString::number (id));
      auto palette_index = region_get_palette_property_len (region, id);
      QString str = QString ();
      if (palette_index)
        {
          for (int j = 0; j < palette_index; j++)
            {
              auto name = region_get_palette_property_name (region, id, j);
              str += name;
              string_free (name);
              str += ": ";
              auto data = region_get_palette_property_data (region, id, j);
              str += data;
              string_free (data);
              if (j != palette_index - 1)
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
      if (x < regionX - 1)
        x++;
      else if (z < regionZ - 1)
        {
          x = 0;
          z++;
        }
      else if (y < regionY - 1)
        {
          x = 0;
          z = 0;
          y++;
        }
      string_free (id_name);
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