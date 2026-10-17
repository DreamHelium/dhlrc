#include "nbtreaderui.h"
#include "mainwindow.h"
#include "ui_nbtreaderui.h"

#include "region.h"
#include <QFileDialog>
#include <libintl.h>
#include <qdialog.h>
#include <qnamespace.h>
#include <qstandarditemmodel.h>
#define _(str) gettext (str)

namespace
{
/* The whole tree is built with plain reads of the NBT object, so a worker
 * thread may run `buildItems ()`. */
void addCompound (const void *currentNbt, QStandardItem *iroot);
void addList (const void *list, QStandardItem *iroot);

QStandardItem *
compoundEntry (const void *currentNbt, int i)
{
  auto key = nbt_compound_index_key (currentNbt, i);
  auto tag = nbt_compound_index_tag (currentNbt, i);
  auto type = nbt_tag_type_int (tag);
  auto valueStr = nbt_tag_value (tag);
  auto typeStr = nbt_tag_type_string (tag);

  auto *item = new QStandardItem ();
  item->setEditable (false);
  item->setData (key ? key : "(NULL)", Qt::DisplayRole);
  item->setData (typeStr, Qt::UserRole + 2);
  item->setData (valueStr ? valueStr : "", Qt::UserRole + 3);

  string_free (key);
  string_free (valueStr);
  string_free (typeStr);

  if (type == 8)
    addCompound (nbt_tag_compound_to_compound (tag), item);
  if (type == 9)
    addList (nbt_tag_list_to_list (tag), item);
  return item;
}

QStandardItem *
listEntry (const void *list, int i)
{
  auto tag = nbt_list_index_tag (list, i);
  auto type = nbt_tag_type_int (tag);
  auto valueStr = nbt_tag_value (tag);
  auto typeStr = nbt_tag_type_string (tag);

  auto *item = new QStandardItem ();
  item->setData ("", Qt::DisplayRole);
  item->setData (typeStr, Qt::UserRole + 2);
  item->setData (valueStr, Qt::UserRole + 3);

  string_free (valueStr);
  string_free (typeStr);

  if (type == 8)
    addCompound (nbt_tag_compound_to_compound (tag), item);
  if (type == 9)
    addList (nbt_tag_list_to_list (tag), item);
  return item;
}

void
addCompound (const void *currentNbt, QStandardItem *iroot)
{
  if (!currentNbt)
    return;
  auto len = nbt_compound_len (currentNbt);
  for (int i = 0; i < len; i++)
    iroot->appendRow (compoundEntry (currentNbt, i));
}

void
addList (const void *list, QStandardItem *iroot)
{
  if (!list)
    return;
  auto len = nbt_list_len (list);
  for (int i = 0; i < len; i++)
    iroot->appendRow (listEntry (list, i));
}
} // namespace

QList<QStandardItem *>
NbtReaderUI::buildItems (const void *nbt, bool fromFile)
{
  QList<QStandardItem *> roots;
  if (!nbt)
    return roots;
  if (fromFile)
    {
      auto *currentRoot = const_cast<NBTRoot *> (nbt);
      auto *item = new QStandardItem ();
      item->setEditable (false);
      auto key = nbt_root_get_string (currentRoot);
      item->setData (key, Qt::DisplayRole);
      item->setData ("Compound", Qt::UserRole + 2);
      item->setData ("", Qt::UserRole + 3);
      string_free (key);
      addCompound (nbt_root_to_compound (currentRoot), item);
      roots.append (item);
      return roots;
    }
  /* An entity's NBT is shown as a flat set of entries. */
  auto len = nbt_compound_len (nbt);
  for (int i = 0; i < len; i++)
    roots.append (compoundEntry (nbt, i));
  return roots;
}

NbtReaderUI::NbtReaderUI (const void *nbt, bool fromFile, QWidget *parent)
    : QDialog (parent), ui (new Ui::NbtReaderUI), nbt (nbt), model (nullptr),
      proxyModel (nullptr)
{
  initUi (buildItems (nbt, fromFile));
}

NbtReaderUI::NbtReaderUI (QList<QStandardItem *> roots, QWidget *parent)
    : QDialog (parent), ui (new Ui::NbtReaderUI), nbt (nullptr),
      model (nullptr), proxyModel (nullptr)
{
  initUi (std::move (roots));
}

void
NbtReaderUI::initUi (QList<QStandardItem *> roots)
{
  MainWindow::ownWindow (this);
  ui->setupUi (this);
  model = new QStandardItemModel (this);
  for (auto *item : roots)
    model->invisibleRootItem ()->appendRow (item);
  proxyModel = new DhTreeFilter (this);
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
