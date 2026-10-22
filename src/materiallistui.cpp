#include "materiallistui.h"

#include "dhcore.h"
#include "dhdownloadmanager.h"
#include "mainwindow.h"
#include "resourcegetter.h"

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
#include <QStringList>
#include <QTableView>
#include <QVBoxLayout>
#include <dhlrc_core.h>
#include <dhlrc_render.h>
#include <libintl.h>
#include <qtconcurrentrun.h>
#include <region.h>
#define _(str) gettext (str)

/* One row's icon, read from the client jar. `QImage` is the one image class
 * that may be used off the GUI thread, which is what lets a recount render its
 * icons on the worker; the finished handler only wraps the images in `QIcon`.
 *
 * A block item (a stair, a slab, a torch) is drawn from its model the way the
 * game draws it: an isometric view of the block, not a flat texture. NULL from
 * that call means the item is a sprite, so its texture is the icon. */
static QImage
renderItemIcon (DhlrcCore *core, const QString &id)
{
  constexpr uint32_t renderSize = 64;
  uint32_t width = 0;
  uint32_t height = 0;
  auto *pixels = dhlrc_render_item_icon (core, id.toUtf8 ().constData (),
                                         renderSize, &width, &height);
  if (pixels && width && height)
    {
      const QImage rendered (pixels, static_cast<int> (width),
                             static_cast<int> (height),
                             static_cast<int> (width) * 4,
                             QImage::Format_RGBA8888);
      /* Copy while scaling, then hand the buffer back: `QImage` only borrows
       * it. */
      const auto scaled = rendered.scaled (20, 20, Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation);
      dhlrc_core_bytes_free (pixels,
                             static_cast<size_t> (width) * height * 4);
      return scaled;
    }

  size_t length = 0;
  auto *bytes
      = dhlrc_core_item_texture (core, id.toUtf8 ().constData (), &length);
  QImage image;
  if (bytes && length)
    {
      image.loadFromData (bytes, static_cast<int> (length), "PNG");
      dhlrc_core_bytes_free (bytes, length);
    }
  return image;
}

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
  /* The page's own option lives in the settings dialog, whose Material List
   * group the core schema describes. The dialog belongs to the main window; a
   * page built without one (the interface self-check) only finds a no-op
   * there, which keeps the button harmless. */
  auto *settingsButton = new QPushButton (_ ("Material List Settings..."));
  settingsButton->setIcon (QIcon::fromTheme (QStringLiteral ("configure")));
  settingsButton->setToolTip (
      _ ("Open the material list setting: whether a missing client jar is "
         "downloaded for the item icons."));
  connect (settingsButton, &QPushButton::clicked, this,
           []
             {
               /* The main window may be gone by the time the button is
                * clicked; `openSettings ()` is a no-op then. */
               MainWindow::openSettings (QStringLiteral ("Material List"));
             });
  tools->addWidget (settingsButton);
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

  /* A version's client jar is shared with the 3D preview: the manager holds one
   * download per version, so the finish of a fetch that page started is the
   * answer this page waits for as well. */
  connect (DhDownloadManager::instance (), &DhDownloadManager::finished, this,
           [this] (const QString &tag, const QString &error)
             {
               if (tag != jarTag ())
                 return;
               /* Nothing is on its way any more, so a later recount is allowed
                * to ask again — that is how a failed or cancelled download is
                * retried. */
               jarRequestedVersion = -1;
               if (error.isEmpty ())
                 refresh ();
               else
                 summary->setText (
                     QString (_ ("The client jar could not be downloaded: %1"))
                         .arg (error));
             });

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

MaterialListUI::~MaterialListUI ()
{
  /* A recount may still be running: stop it between icons and wait for it, so
   * it cannot outlive the core it calls. The worker touches no widget, so
   * waiting is all this destructor has to do. */
  if (cancel)
    cancel->store (true);
  if (watcher)
    watcher->future ().waitForFinished ();
}

void
MaterialListUI::setTranslationReady (bool ready)
{
  if (translationReady == ready)
    return;
  translationReady = ready;
  /* Only the displayed names change; the counts the core returned are still
   * the right ones, so the rows are rebuilt from the last answer. */
  populate ();
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
  /* Each request gets a generation, and an answer is only used when it belongs
   * to the newest one: an edit that arrives while a recount runs must not be
   * overwritten by the older answer coming in after it. */
  ++generation;
  summary->setText (_ ("Loading the material list..."));
  if (loading)
    {
      /* One recount is already walking the block array; this request is folded
       * into it instead of queueing a second walk. */
      refreshPending = true;
      return;
    }
  startLoad ();
}

void
MaterialListUI::startLoad ()
{
  loading = true;
  const quint64 requested = generation;

  /* The worker holds its own copies: the page may be closed while the count
   * runs, and the region must stay alive until the worker is done with it. The
   * ignored switch is read here, on the GUI thread, and only its value travels
   * to the worker. */
  auto regionCopy = region;
  DhlrcCore *core = nullptr;
  if (auto *instance = DhCore::instance ())
    core = instance->handle ();
  const bool wantIgnored = ignoredBox->isChecked ();

  cancel = std::make_shared<std::atomic_bool> (false);
  const auto cancelFlag = cancel;

  watcher = new QFutureWatcher<LoadResult> (this);
  connect (watcher, &QFutureWatcher<LoadResult>::finished, this,
           [this, requested]
           {
             auto *done = watcher;
             watcher = nullptr;
             if (done)
               loadFinished (done, requested);
           });
  watcher->setFuture (
      QtConcurrent::run ([regionCopy, core, wantIgnored, cancelFlag]
                         {
                           return buildMaterialList (
                               regionCopy, core, wantIgnored, cancelFlag);
                         }));
}

void
MaterialListUI::loadFinished (QFutureWatcher<LoadResult> *done,
                              quint64 requested)
{
  /* The result is read before the watcher goes away; `results ()` would block,
   * and this handler only runs once the future has finished anyway. */
  const LoadResult result = done->result ();
  done->deleteLater ();
  loading = false;

  if (requested != generation)
    {
      /* A newer refresh replaced this answer while it ran. That refresh is
       * either already running or waiting here. */
      if (refreshPending)
        {
          refreshPending = false;
          startLoad ();
        }
      return;
    }

  material = result.document;
  if (material.isNull ())
    error = result.coreEmpty ? _ ("The core returned no material list.")
                             : result.error;
  else
    error.clear ();
  /* A recount renders every row's icon from the jar, so all of them coming
   * back empty is how a missing jar shows itself here. */
  bool anyIcon = false;
  for (auto it = result.icons.cbegin (); it != result.icons.cend (); ++it)
    {
      iconCache.insert (it.key (), QIcon (QPixmap::fromImage (it.value ())));
      if (!it.value ().isNull ())
        anyIcon = true;
    }

  populate ();
  requestClientJar (!result.icons.isEmpty () && !anyIcon);
}

QString
MaterialListUI::jarTag () const
{
  return DhDownloadManager::clientJarTag (
      static_cast<int> (region->dataVersion ()));
}

void
MaterialListUI::requestClientJar (bool iconsMissing)
{
  if (!iconsMissing)
    return;
  auto *core = DhCore::instance ();
  if (!core)
    return;
  const int version = static_cast<int> (region->dataVersion ());
  /* A jar that is there needs no download, whatever the icons say: the
   * resource layer may just not have been pointed at it. */
  if (dhlrc_core_client_jar_state (core->handle (),
                                   static_cast<int32_t> (version))
      != 0)
    return;
  /* Off means the list keeps showing what it has; the summary below says what
   * is missing, and the settings button is one click away. */
  if (core->settingValue (QStringLiteral ("DownloadClientJarForMaterials"))
      == QLatin1String ("false"))
    return;

  summary->setText (_ ("Downloading the client jar for the item icons..."));
  /* Once per version: a recount that runs while this download is on its way
   * only says it is still going. The manager would also refuse a second
   * download of the tag, but not asking at all keeps the reason clear. */
  if (jarRequestedVersion == version)
    return;
  jarRequestedVersion = version;
  DhDownloadManager::instance ()->ensureClientJar (version);
}

MaterialListUI::LoadResult
MaterialListUI::buildMaterialList (std::shared_ptr<RegionClass> region,
                                   DhlrcCore *core, bool wantIgnored,
                                   const std::shared_ptr<std::atomic_bool>
                                       &cancel)
{
  LoadResult result;

  {
    /* Counting walks the whole block array, so the region is held while it
     * runs; a write cannot move the blocks under it. */
    AutoLocker locker (*region);
    void *raw = region->get_region ();
    char *json = dhlrc_core_material_list (raw);
    if (!json)
      {
        result.coreEmpty = true;
        return result;
      }
    QJsonParseError parseError;
    result.document = QJsonDocument::fromJson (QByteArray (json), &parseError);
    if (result.document.isNull ())
      result.error = parseError.errorString ();
    string_free (json);
  }

  if (!core || result.document.isNull () || cancel->load ())
    return result;

  /* The icons are the slow part — each one renders a model or decodes a PNG —
   * so they are made here as well. The core serialises its resource set, which
   * makes these calls safe off the GUI thread. The ignored rows are only
   * rendered when the switch shows them; the rest would be thrown away. */
  const auto root = result.document.object ();
  QStringList ids;
  for (const auto &value : root.value ("items").toArray ())
    ids << value.toObject ().value ("id").toString ();
  if (wantIgnored)
    for (const auto &value : root.value ("ignored").toArray ())
      ids << value.toObject ().value ("block").toString ();

  for (const auto &id : ids)
    {
      if (cancel->load ())
        {
          /* The page is going away; the result is discarded anyway. */
          result.icons.clear ();
          break;
        }
      if (!id.isEmpty () && !result.icons.contains (id))
        result.icons.insert (id, renderItemIcon (core, id));
    }
  return result;
}

void
MaterialListUI::populate ()
{
  model->removeRows (0, model->rowCount ());
  model->setHorizontalHeaderLabels ({ _ ("Item"), _ ("ID"), _ ("Count") });
  detailView->setPlainText (QString ());

  if (material.isNull ())
    {
      if (loading)
        summary->setText (_ ("Loading the material list..."));
      else
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
        summary->setText (
            summary->text () + "\n"
            + _ ("Item icons need this version's client jar, which has not "
                 "been downloaded yet."));
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
   * looked for again. The icons of a fresh answer are already in the cache: the
   * recount rendered them on its worker. */
  auto cached = iconCache.constFind (id);
  if (cached != iconCache.constEnd ())
    return cached.value ();

  QIcon icon;
  if (auto *core = DhCore::instance ())
    {
      const QImage image = renderItemIcon (core->handle (), id);
      if (!image.isNull ())
        icon = QIcon (QPixmap::fromImage (image));
    }
  iconCache.insert (id, icon);
  return icon;
}
