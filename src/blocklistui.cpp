#include "blocklistui.h"
#include "ui_blocklistui.h"

#include "mainwindow.h"
#include "regionclass.h"
#include "resourcegetter.h"

#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QtGlobal>
#include <ctime>
#include <libintl.h>
#include <region.h>
#include <utility>

#define _(str) gettext (str)

BlockListUI::BlockListUI (std::shared_ptr<RegionClass> region,
                          bool translationReady_, QWidget *parent)
    : QWidget (parent), regionClass (std::move (region)),
      translationReady (translationReady_), ui (new Ui::BlockListUI)
{
  MainWindow::ownWindow (this);
  model = new QStandardItemModel (this);
  proxyModel = new QSortFilterProxyModel (this);
  auto btn = QMessageBox::question (this, _ ("Ignore Air?"),
                                    _ ("Do you want to ignore air?"));
  ignoreAir = btn == QMessageBox::Yes;
  ui->setupUi (this);
  QObject::connect (ui->lineEdit, &QLineEdit::textChanged, this,
                    &BlockListUI::textChanged_cb);
  connect (this, &BlockListUI::setModel, this,
           [this]
             {
               proxyModel->setSourceModel (model);
               ui->tableView->setModel (proxyModel);
               proxyModel->setFilterKeyColumn (-1);
               ui->tableView->horizontalHeader ()->setSectionResizeMode (
                   1, QHeaderView::Stretch);
             });
  ui->tableView->setDND (false);
}

void
BlockListUI::setTranslationReady (bool ready)
{
  if (translationReady == ready)
    return;
  translationReady = ready;
  updateBlockList ();
}

BlockListUI::~BlockListUI ()
{
  /* The scan may still be running; stop it and wait for the current block, so
   * it cannot touch the model or the region after this object is gone. */
  stop = true;
  if (worker.joinable ())
    worker.join ();
  model->clear ();
  delete model;
  delete ui;
}

void
BlockListUI::updateBlockList ()
{
  /* A second scan (the names can arrive after the window opened) replaces the
   * rows instead of adding to them. */
  model->removeRows (0, model->rowCount ());
  QStringList stringList;
  if (translationReady)
    stringList << _ ("id") << _ ("id Name") << _ ("Block Name") << _ ("x")
               << _ ("y") << _ ("z") << _ ("Palette");
  else
    stringList << _ ("id") << _ ("id Name") << _ ("x") << _ ("y") << _ ("z")
               << _ ("Palette");
  model->setHorizontalHeaderLabels (stringList);
  connect (this, &BlockListUI::setValue, ui->progressBar,
           &QProgressBar::setValue);

  /* Translations are looked up here, on the GUI thread, once per palette
   * entry; the worker then needs no access to the core's translation state. */
  QVector<QString> translations;
  if (translationReady)
    {
      AutoLocker locker (*regionClass);
      void *region = regionClass->get_region ();
      const auto paletteLen = region_get_palette_len (region);
      translations.reserve (static_cast<qsizetype> (paletteLen));
      for (size_t id = 0; id < paletteLen; id++)
        {
          auto name = region_get_palette_id_name (region, id);
          const QString text = name ? QString::fromUtf8 (name) : QString ();
          string_free (name);
          translations.append (get_translation_from_object (text));
        }
    }

  /* A second scan (the button can be pressed again) waits for the first. */
  stop = true;
  if (worker.joinable ())
    worker.join ();
  stop = false;
  const bool skipAir = ignoreAir;
  worker = std::thread (
      [this, translations = std::move (translations), skipAir]
      { scanBlocks (translations, skipAir); });
}

void
BlockListUI::scanBlocks (QVector<QString> translations, bool ignoreAir)
{
  /* The region is held while it is read: the GUI cannot edit it under the
   * scan, and the row in the region list shows that it is in use. */
  AutoLocker locker (*regionClass);
  void *region = regionClass->get_region ();

  const int regionX = region_get_x (region);
  const int regionY = region_get_y (region);
  const int regionZ = region_get_z (region);
  const int size = regionX * regionY * regionZ;
  int x = 0;
  int y = 0;
  int z = 0;
  const auto advance = [&x, &y, &z, regionX, regionY, regionZ]
    {
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
    };

  /* Batches of rows are handed to the GUI thread; the worker itself never
   * appends to the model. */
  const auto post = [this] (QList<QList<QStandardItem *>> rows)
  {
    auto batch = std::make_shared<ItemBatch> ();
    batch->rows = std::move (rows);
    QMetaObject::invokeMethod (
        this,
        [this, batch]
          {
            if (!model)
              return;
            for (auto &row : batch->rows)
              model->appendRow (row);
            batch->rows.clear ();
          },
        Qt::QueuedConnection);
  };

  void *systemInfo = get_system_info_object ();
  clock_t last = clock ();
  QList<QList<QStandardItem *>> rows;
  for (int i = 0; i < size && !stop.load (); i++)
    {
      const clock_t now = clock ();
      if ((now - last) > 1000)
        {
          last = now;
          Q_EMIT setValue ((i + 1) * 100 / size);
          if (get_free_memory (systemInfo) < 500 * 1024 * 1024)
            {
              system_info_object_free (systemInfo);
              Q_EMIT stopProcess ();
              return;
            }
        }

      const auto id = region_get_block_id_by_index (region, i);
      auto idName = region_get_palette_id_name (region, id);
      const QString name = idName ? QString::fromUtf8 (idName)
                                  : QStringLiteral ("minecraft:air");
      if (ignoreAir && name == QLatin1String ("minecraft:air"))
        {
          string_free (idName);
          advance ();
          continue;
        }

      QList<QStandardItem *> row;
      row << new QStandardItem (QString::number (i));
      row << new QStandardItem (name);
      if (translationReady)
        row << new QStandardItem (
            id < translations.size () ? translations[id] : QString ());
      row << new QStandardItem (QString::number (x));
      row << new QStandardItem (QString::number (y));
      row << new QStandardItem (QString::number (z));
      auto *paletteItem = new QStandardItem (QString::number (id));
      row << paletteItem;

      const auto count = region_get_palette_property_len (region, id);
      if (count)
        {
          QStringList properties;
          for (int j = 0; j < count; j++)
            {
              auto propertyName
                  = region_get_palette_property_name (region, id, j);
              auto propertyData
                  = region_get_palette_property_data (region, id, j);
              properties << QStringLiteral ("%1: %2")
                                .arg (propertyName ? QString::fromUtf8 (
                                                         propertyName)
                                                   : QString ())
                                .arg (propertyData ? QString::fromUtf8 (
                                                         propertyData)
                                                   : QString ());
              string_free (propertyName);
              string_free (propertyData);
            }
          paletteItem->setToolTip (properties.join (QLatin1Char ('\n')));
        }

      for (auto *item : row)
        item->setEditable (false);
      string_free (idName);
      rows.append (row);
      if (rows.size () >= 64)
        post (std::move (rows));
      advance ();
    }

  if (!rows.isEmpty ())
    post (std::move (rows));
  system_info_object_free (systemInfo);
  if (!stop.load ())
    {
      Q_EMIT setValue (100);
      Q_EMIT setModel ();
    }
}

void
BlockListUI::textChanged_cb (const QString &str)
{
  proxyModel->setFilterRegularExpression (str);
}
