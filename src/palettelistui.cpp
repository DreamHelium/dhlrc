#include "palettelistui.h"

#include "ui_palettelistui.h"

#include <region.h>

PaletteListUI::PaletteListUI (void *region, char *&large_version,
                              QWidget *parent)
    : QDialog (parent), ui (new Ui::PaletteListUI),
      large_version (large_version), region (region)
{
  ui->setupUi (this);
  model = new QStandardItemModel (this);
  proxyModel = new QSortFilterProxyModel (this);
  drawList ();
  initUI ();
  QObject::connect (ui->buttonBox, &QDialogButtonBox::rejected, this,
                    &PaletteListUI::close);
}

PaletteListUI::~PaletteListUI ()
{
  model->clear ();
  delete model;
  delete ui;
}

void
PaletteListUI::initUI ()
{
  ui->tableView->setModel (proxyModel);
}

void
PaletteListUI::drawList ()
{
  QStringList stringlist;
  if (large_version)
    stringlist << "Id" << "Name" << "Translation name" << "Properties";
  else
    stringlist << "Id" << "Name" << "Properties";
  model->setHorizontalHeaderLabels (stringlist);
  auto palette_len = region_get_palette_len (region);
  for (int i = 0; i < palette_len; i++)
    {
      QStandardItem *item2 = nullptr;
      QStandardItem *item0 = new QStandardItem (QString::number (i));
      auto name = region_get_palette_id_name (region, i);
      QStandardItem *item1 = new QStandardItem (name);
      string_free (name);
      // if (dhlrc_mcdata_enabled () && large_version)
      //     item2 = new QStandardItem (
      //         mctr (palette->id_name, large_version));
      QStandardItem *item3;

      auto len = region_get_palette_property_len (region, i);
      if (len)
        {
          QString line = "%1=%2";
          QString str{};
          for (int j = 0; j < len; j++)
            {
              str += line.arg (region_get_palette_property_name (region, i, j))
                         .arg (
                             region_get_palette_property_data (region, i, j));
              if (j != len - 1)
                str += ", ";
            }
          item3 = new QStandardItem (str);
        }
      else
        item3 = new QStandardItem ("");
      QList<QStandardItem *> items;
      if (item2)
        items = { item0, item1, item2, item3 };
      else
        items = { item0, item1, item3 };
      model->appendRow (items);
    }
  proxyModel->setSourceModel (model);
  proxyModel->setFilterKeyColumn (-1);
}