#include "nbtreaderui.h"
#include "ui_nbtreaderui.h"

#include "region.h"
#include <QFileDialog>
#include <libintl.h>
#include <qdialog.h>
#include <qnamespace.h>
#include <qstandarditemmodel.h>
#define _(str) gettext (str)

NbtReaderUI::NbtReaderUI (const void *nbt, bool fromFile, QWidget *parent)
    : QDialog (parent), ui (new Ui::NbtReaderUI), nbt (nbt),
      fromFile (fromFile)
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
              auto typeItem = gettext (model->data (index, Qt::UserRole + 2)
                                           .toString ()
                                           .toUtf8 ()
                                           .constData ());
              ui->typeLabel->setText (typeItem);
              auto keyItem = model->data (index, Qt::DisplayRole).toString ();
              ui->keyLabel->setText (keyItem);
              auto valueItem
                  = model->data (index, Qt::UserRole + 3).toString ();
              ui->valueLabel->setText (valueItem);
            }
        });
  connect (ui->closeBtn, &QPushButton::clicked, this, &NbtReaderUI::close);
  connect (ui->exportBtn, &QPushButton::clicked, this,
           [&]
             {
               // auto dir = QFileDialog::getSaveFileName (
               //     this, _ ("Export NBT To ..."));
               // if (!dir.isEmpty ())
               //   nbt_vec_to_file (this->nbt, dir.toUtf8 ().constData (),
               //                    this->fromFile);
             });
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
  if (fromFile)
    {
      auto currentRoot = const_cast<NBTRoot *> (currentNbt);
      auto item = new QStandardItem ();
      item->setEditable (false);
      auto key = nbt_root_get_string (currentRoot);
      auto nextCurrent = nbt_root_to_compound (currentRoot);
      item->setData (key, Qt::DisplayRole);
      item->setData ("Compound", Qt::UserRole + 2);
      item->setData ("", Qt::UserRole + 3);
      string_free (key);
      fromFile = false;
      addModelTree (nextCurrent, item);
      iroot->appendRow (item);
      return;
    }
  auto len = nbt_compound_len (currentNbt);
  for (int i = 0; i < len; i++)
    {
      auto key = nbt_compound_index_key (currentNbt, i);
      auto tag = nbt_compound_index_tag (currentNbt, i);
      auto type = nbt_tag_type_int (tag);
      auto valueStr = nbt_tag_value (tag);
      auto typeStr = nbt_tag_type_string (tag);

      auto item = new QStandardItem ();
      item->setEditable (false);
      item->setData (key ? key : "(NULL)", Qt::DisplayRole);
      item->setData (typeStr, Qt::UserRole + 2);
      item->setData (valueStr ? valueStr : "", Qt::UserRole + 3);

      string_free (key);
      string_free (valueStr);
      string_free (typeStr);

      if (type == 8)
        {
          auto new_nbt = nbt_tag_compound_to_compound (tag);
          addModelTree (new_nbt, item);
        }
      if (type == 9)
        {
          auto list = nbt_tag_list_to_list (tag);
          addModelTreeFromList (list, item);
        }
      iroot->appendRow (item);
    }
}

void
NbtReaderUI::addModelTreeFromList (const void *list, QStandardItem *iroot)
{
  auto len = nbt_list_len (list);
  for (int i = 0; i < len; i++)
    {
      auto tag = nbt_list_index_tag (list, i);
      /* No key */
      auto key = "";
      auto type = nbt_tag_type_int (tag);
      auto valueStr = nbt_tag_value (tag);
      auto typeStr = nbt_tag_type_string (tag);

      auto item = new QStandardItem ();
      item->setData (key, Qt::DisplayRole);
      item->setData (typeStr, Qt::UserRole + 2);
      item->setData (valueStr, Qt::UserRole + 3);

      string_free (valueStr);
      string_free (typeStr);

      if (type == 8)
        {
          auto new_nbt = nbt_tag_compound_to_compound (tag);
          addModelTree (new_nbt, item);
        }
      if (type == 9)
        {
          auto list = nbt_tag_list_to_list (tag);
          addModelTreeFromList (list, item);
        }
      iroot->appendRow (item);
    }
}
