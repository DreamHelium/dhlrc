#include "nbtreaderui.h"
#include "ui_nbtreaderui.h"

#include <region.h>

NbtReaderUI::NbtReaderUI (const void *nbt, QWidget *parent)
    : QWidget (parent), ui (new Ui::NbtReaderUI), nbt (nbt)
{
  ui->setupUi (this);
  model = new QStandardItemModel (this);
  proxyModel = new DhTreeFilter (this);
  initModel ();
  proxyModel->setSourceModel (model);
  ui->treeView->setModel (proxyModel);
  auto selectionModel = ui->treeView->selectionModel ();
  connect (ui->lineEdit, &QLineEdit::textChanged, this,
           [&] (const QString &text)
             { proxyModel->setFilterRegularExpression (text); });
  connect (
      selectionModel, &QItemSelectionModel::selectionChanged, this,
      [&] (const QItemSelection &selected, const QItemSelection &deselected)
        {
          auto selection = proxyModel->mapSelectionToSource (selected);
          if (!selection.isEmpty ())
            {
              auto index = selection.indexes ()[0];
              auto typeItem
                  = model->data (index, Qt::UserRole + 2).toString ();
              ui->typeLabel->setText (typeItem);
              auto keyItem = model->data (index, Qt::DisplayRole).toString ();
              ui->keyLabel->setText (keyItem);
              auto valueItem
                  = model->data (index, Qt::UserRole + 3).toString ();
              ui->valueLabel->setText (valueItem);
            }
        });
  connect (ui->closeBtn, &QPushButton::clicked, this, &NbtReaderUI::close);
}

NbtReaderUI::~NbtReaderUI ()
{
  model->clear ();
  delete model;
  delete ui;
}

void
NbtReaderUI::disableClose ()
{
  ui->closeBtn->hide ();
}

void
NbtReaderUI::initModel ()
{
  auto root = model->invisibleRootItem ();
  addModelTree (nbt, root);
}

void
NbtReaderUI::addModelTree (const void *currentNbt, QStandardItem *iroot)
{
  auto len = nbt_vec_get_len (currentNbt);
  for (int i = 0; i < len; i++)
    {
      auto key = nbt_vec_get_key (currentNbt, i);
      auto type = nbt_vec_get_value_type_int (currentNbt, i);
      auto valueStr = nbt_vec_get_value_string (currentNbt, i);
      auto typeStr = nbt_vec_get_value_type (currentNbt, i);

      auto item = new QStandardItem ();
      item->setEditable (false);
      item->setData (key ? key : "(NULL)", Qt::DisplayRole);
      item->setData (typeStr, Qt::UserRole + 2);
      item->setData (valueStr ? valueStr : "", Qt::UserRole + 3);

      string_free (key);
      string_free (valueStr);
      string_free (typeStr);

      if (type == 12)
        {
          auto new_nbt = nbt_vec_get_value_to_child (currentNbt, i);
          addModelTree (new_nbt, item);
        }
      if (type == 11)
        {
          auto list = nbt_vec_get_value_list_to_child (currentNbt, i);
          addModelTreeFromList (list, item);
        }
      iroot->appendRow (item);
    }
}

void
NbtReaderUI::addModelTreeFromList (const void *list, QStandardItem *iroot)
{
  auto len = nbt_vec_tree_value_get_len (list);
  for (int i = 0; i < len; i++)
    {
      auto item = nbt_vec_tree_value_get_tree_value (list, i);
      /* No key */
      auto key = "";
      auto type = nbt_tree_value_get_type_int (item);
      auto typeStr = nbt_tree_value_get_type_string (item);
      auto valueStr = nbt_tree_value_get_value_string (item);

      auto standardItem = new QStandardItem ();
      standardItem->setData (key, Qt::DisplayRole);
      standardItem->setData (typeStr, Qt::UserRole + 2);
      standardItem->setData (valueStr, Qt::UserRole + 3);

      string_free (valueStr);
      string_free (typeStr);

      if (type == 12)
        {
          auto new_nbt = nbt_tree_value_get_value_to_child (item);
          addModelTree (new_nbt, standardItem);
        }
      if (type == 11)
        {
          auto new_list = nbt_tree_value_get_value_list_to_child (item);
          addModelTreeFromList (new_list, standardItem);
        }
      iroot->appendRow (standardItem);
    }
}