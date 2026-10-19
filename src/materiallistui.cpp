#include "materiallistui.h"

#include "dhcore.h"
#include "resourcegetter.h"

#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QImage>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>
#include <dhlrc_core.h>
#include <dhlrc_render.h>
#include <libintl.h>
#include <region.h>
#define _(str) gettext (str)

MaterialListUI::MaterialListUI (std::shared_ptr<RegionClass> region_,
                                bool translationReady_, QWidget *parent)
    : DhWidget (parent), region (std::move (region_)),
      translationReady (translationReady_)
{
  auto *layout = new QVBoxLayout (this);

  auto *title = new QLabel (region->displayName ());
  QFont titleFont = title->font ();
  titleFont.setBold (true);
  title->setFont (titleFont);
  layout->addWidget (title);

  summary = new QLabel ();
  summary->setWordWrap (true);
  layout->addWidget (summary);

  auto *tools = new QHBoxLayout ();
  filterEdit = new QLineEdit ();
  filterEdit->setPlaceholderText (_ ("Filter by item or block id"));
  filterEdit->setClearButtonEnabled (true);
  tools->addWidget (filterEdit, 1);
  ignoredBox = new QCheckBox (_ ("Show &ignored blocks"));
  ignoredBox->setToolTip (
      _ ("Blocks that need nothing prepared, like air, fire or a portal."));
  tools->addWidget (ignoredBox);
  auto *refreshButton = new QPushButton (_ ("&Refresh"));
  refreshButton->setIcon (QIcon::fromTheme ("view-refresh"));
  tools->addWidget (refreshButton);
  layout->addLayout (tools);

  model = new QStandardItemModel (this);
  proxy = new QSortFilterProxyModel (this);
  proxy->setSourceModel (model);
  proxy->setFilterCaseSensitivity (Qt::CaseInsensitive);
  proxy->setFilterKeyColumn (-1);
  table = new QTableView ();
  table->setModel (proxy);
  table->setSelectionBehavior (QAbstractItemView::SelectRows);
  table->setSelectionMode (QAbstractItemView::SingleSelection);
  table->setEditTriggers (QAbstractItemView::NoEditTriggers);
  table->setAlternatingRowColors (true);
  table->verticalHeader ()->setVisible (false);
  table->setIconSize (QSize (20, 20));
  table->horizontalHeader ()->setSectionResizeMode (0, QHeaderView::Stretch);
  table->horizontalHeader ()->setSectionResizeMode (1,
                                                    QHeaderView::ResizeToContents);
  table->horizontalHeader ()->setSectionResizeMode (2,
                                                    QHeaderView::ResizeToContents);
  layout->addWidget (table, 1);

  /* What the selected row is made of: the detail behind the total. */
  detailView = new QPlainTextEdit (this);
  detailView->setObjectName ("materialDetail");
  detailView->setReadOnly (true);
  detailView->setMaximumHeight (160);
  detailView->setPlaceholderText (
      _ ("Select an item to see which blocks make it up."));
  layout->addWidget (detailView);

  connect (filterEdit, &QLineEdit::textChanged, this,
           [this] (const QString &text)
           { proxy->setFilterFixedString (text); });
  connect (ignoredBox, &QCheckBox::toggled, this, [this] { populate (); });
  connect (refreshButton, &QPushButton::clicked, this,
           [this] { refresh (); });
  connect (table->selectionModel (), &QItemSelectionModel::selectionChanged,
           this, [this] { updateDetail (); });
  /* An edit shows in the list at once, like the change history. */
  connect (region.get (), &RegionClass::changesChanged, this,
           [this] { refresh (); });

  /* An item's texture lives in the version's client jar, and the resource set
   * only has the downloaded language files until someone points it at a
   * version. The 3D preview does the same before it draws; without this every
   * icon lookup misses. */
  if (auto *core = DhCore::instance ())
    dhlrc_core_resources_configure_for_data_version (
        core->handle (), static_cast<int32_t> (region->dataVersion ()), nullptr,
        0);

  refresh ();
}

MaterialListUI::~MaterialListUI () = default;

void
MaterialListUI::setTranslationReady (bool ready)
{
  if (translationReady == ready)
    return;
  translationReady = ready;
  refresh ();
}

QString
MaterialListUI::displayName (const QString &id) const
{
  if (!translationReady)
    return QString ();
  return get_translation_from_object (id);
}

QString
MaterialListUI::describeRule (const QString &rule) const
{
  if (rule == QLatin1String ("identity"))
    return _ ("same id as the block");
  if (rule == QLatin1String ("exact"))
    return _ ("exact row of the table");
  if (rule.startsWith (QLatin1String ("pattern:")))
    return QString (_ ("pattern %1")).arg (rule.mid (8));
  return rule;
}

void
MaterialListUI::refresh ()
{
  model->removeRows (0, model->rowCount ());
  material = QJsonDocument ();
  error.clear ();

  QApplication::setOverrideCursor (Qt::WaitCursor);
  {
    /* Counting walks the whole block array, so the region is held while it
     * runs; a write cannot move the blocks under it. */
    AutoLocker locker (*region);
    void *raw = region->get_region ();
    char *json = dhlrc_core_material_list (raw);
    if (json)
      {
        QJsonParseError parseError;
        material = QJsonDocument::fromJson (QByteArray (json), &parseError);
        if (material.isNull ())
          error = parseError.errorString ();
        string_free (json);
      }
    else
      error = _ ("The core returned no material list.");
  }
  QApplication::restoreOverrideCursor ();

  populate ();
}

void
MaterialListUI::populate ()
{
  model->removeRows (0, model->rowCount ());
  model->setHorizontalHeaderLabels ({ _ ("Item"), _ ("ID"), _ ("Count") });
  detailView->setPlainText (QString ());

  if (material.isNull ())
    {
      summary->setText (error.isEmpty () ? _ ("No material list.") : error);
      return;
    }

  const auto root = material.object ();
  const auto items = root.value ("items").toArray ();
  const auto ignored = root.value ("ignored").toArray ();
  const auto blocksTotal = static_cast<qint64> (root.value ("blocks_total").toDouble ());

  for (const auto &value : items)
    {
      const auto object = value.toObject ();
      const auto id = object.value ("id").toString ();
      const auto count
          = static_cast<qint64> (object.value ("count").toDouble ());
      const auto name = displayName (id);
      /* Two block positions can make one item (a door, a bed, a tall plant),
       * and one block can stand for several items (a double slab, snow layers,
       * candles). Both are visible here so the total is explainable. */
      const auto blocks
          = static_cast<qint64> (object.value ("blocks").toDouble ());
      const bool pair = object.value ("pair").toBool ();

      /* The detail is built once per row instead of on every selection. */
      QString detail
          = QString (_ ("Item: %1 (%2)\nCount: %3, from %4 block positions\n"))
                .arg (name.isEmpty () ? id : name, id)
                .arg (count)
                .arg (blocks);
      if (pair)
        detail += _ ("Note: two block positions make one item here (a door, a "
                     "bed, a tall plant).\n");
      else if (count > blocks)
        detail += _ ("Note: one block stands for several items here (a double "
                     "slab, snow layers, candles).\n");
      detail += QString (_ ("\nSources:\n"));
      for (const auto &sourceValue : object.value ("sources").toArray ())
        {
          const auto source = sourceValue.toObject ();
          const auto block = source.value ("block").toString ();
          const auto blockName = displayName (block);
          detail += QString ("  %1 (%2)  × %3  — %4\n")
                        .arg (blockName.isEmpty () ? block : blockName, block)
                        .arg (static_cast<qint64> (
                            source.value ("blocks").toDouble ()))
                        .arg (describeRule (source.value ("rule").toString ()));
        }

      QList<QStandardItem *> row;
      auto *nameItem = new QStandardItem (iconFor (id),
                                          name.isEmpty () ? id : name);
      nameItem->setData (detail, Qt::UserRole);
      nameItem->setToolTip (id);
      row << nameItem;
      row << new QStandardItem (id);
      auto *countItem = new QStandardItem (QString::number (count));
      countItem->setTextAlignment (Qt::AlignRight | Qt::AlignVCenter);
      row << countItem;
      model->appendRow (row);
    }

  if (ignoredBox->isChecked ())
    {
      for (const auto &value : ignored)
        {
          const auto object = value.toObject ();
          const auto block = object.value ("block").toString ();
          const auto count
              = static_cast<qint64> (object.value ("count").toDouble ());
          const auto name = displayName (block);
          const QString detail
              = QString (_ ("Block: %1 (%2)\nCount: %3\n\n"
                            "This block needs nothing prepared: the "
                            "block-to-item table ignores it."))
                    .arg (name.isEmpty () ? block : name, block)
                    .arg (count);

          QList<QStandardItem *> row;
          auto *nameItem = new QStandardItem (iconFor (block),
                                              name.isEmpty () ? block : name);
          nameItem->setData (detail, Qt::UserRole);
          nameItem->setForeground (QColor (128, 128, 128));
          row << nameItem;
          auto *idItem = new QStandardItem (block);
          idItem->setForeground (QColor (128, 128, 128));
          row << idItem;
          auto *countItem = new QStandardItem (QString::number (count));
          countItem->setForeground (QColor (128, 128, 128));
          countItem->setTextAlignment (Qt::AlignRight | Qt::AlignVCenter);
          row << countItem;
          model->appendRow (row);
        }
    }

  qint64 ignoredBlocks = 0;
  for (const auto &value : ignored)
    ignoredBlocks
        += static_cast<qint64> (value.toObject ().value ("count").toDouble ());
  summary->setText (
      QString (_ ("%1 items from %2 blocks; %3 ignored block kinds (%4 blocks)."))
          .arg (items.size ())
          .arg (blocksTotal)
          .arg (ignored.size ())
          .arg (ignoredBlocks));

  /* The icons come from the client jar; say so when it is not downloaded yet
   * instead of leaving the rows blank for no visible reason. */
  if (auto *core = DhCore::instance ())
    {
      const auto state = dhlrc_core_client_jar_state (
          core->handle (), static_cast<int32_t> (region->dataVersion ()));
      if (state == 0)
        summary->setText (summary->text () + "\n"
                          + _ ("Item icons need this version's client jar; "
                               "download it from the 3D preview."));
    }
}

void
MaterialListUI::updateDetail ()
{
  const auto rows = table->selectionModel ()->selectedRows ();
  if (rows.isEmpty ())
    {
      detailView->setPlainText (QString ());
      return;
    }
  const auto index = proxy->mapToSource (rows.first ());
  const auto *item = model->item (index.row (), 0);
  detailView->setPlainText (item ? item->data (Qt::UserRole).toString ()
                                 : QString ());
}

QIcon
MaterialListUI::iconFor (const QString &id)
{
  /* A row is drawn every time the filter changes, so an icon is read from the
   * jar once and kept. A miss is cached too, so an item with no icon is not
   * looked for again. */
  auto cached = iconCache.constFind (id);
  if (cached != iconCache.constEnd ())
    return cached.value ();

  QIcon icon;
  if (auto *core = DhCore::instance ())
    {
      /* A block item (a stair, a slab, a torch) is drawn from its model the way
       * the game draws it: an isometric view of the block, not a flat texture.
       * NULL means the item is a sprite, so the texture below is the icon. */
      constexpr uint32_t renderSize = 64;
      uint32_t width = 0;
      uint32_t height = 0;
      auto *pixels = dhlrc_render_item_icon (
          core->handle (), id.toUtf8 ().constData (), renderSize, &width,
          &height);
      if (pixels && width && height)
        {
          const QImage rendered (pixels, static_cast<int> (width),
                                 static_cast<int> (height),
                                 static_cast<int> (width) * 4,
                                 QImage::Format_RGBA8888);
          /* Copy while scaling, then hand the buffer back: `QImage` only
           * borrows it. */
          const auto scaled = rendered.scaled (
              20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation);
          icon = QIcon (QPixmap::fromImage (scaled));
          dhlrc_core_bytes_free (
              pixels, static_cast<size_t> (width) * height * 4);
        }

      if (icon.isNull ())
        {
          size_t length = 0;
          auto *bytes = dhlrc_core_item_texture (
              core->handle (), id.toUtf8 ().constData (), &length);
          if (bytes && length)
            {
              QImage image;
              if (image.loadFromData (bytes, static_cast<int> (length), "PNG"))
                icon = QIcon (QPixmap::fromImage (image));
              dhlrc_core_bytes_free (bytes, length);
            }
        }
    }
  iconCache.insert (id, icon);
  return icon;
}
