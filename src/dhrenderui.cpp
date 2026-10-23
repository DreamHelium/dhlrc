#include "dhrenderui.h"
#include "blockpickerui.h"
#include "propertymodifyui.h"

#include "blockreaderui.h"
#include "dhcore.h"
#include "dhdownloadmanager.h"
#include "dhrenderwidget.h"
#include "flowlayout.h"
#include "mainwindow.h"
#include "region.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QIcon>
#include <QMessageBox>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <algorithm>
#include <cstring>
#include <vector>
#include <cstdint>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <libintl.h>
#include <optional>
#include <qtconcurrentrun.h>
#include <utility>

#define _(str) gettext (str)

namespace
{
/* Whether a palette entry names air: a cell holding one is not drawn, so a
 * band selection leaves it out. The namespace is whatever the version calls
 * it, so only the last component of the name is compared. */
bool
isAirName (const char *name)
{
  if (!name || !*name)
    return true;
  const char *last = std::strrchr (name, ':');
  const char *base = last ? last + 1 : name;
  return std::strcmp (base, "air") == 0 || std::strcmp (base, "cave_air") == 0
         || std::strcmp (base, "void_air") == 0;
}

/* Called by the renderer from the worker thread: it only stores the
 * percentage the window polls. */
void
sceneProgress (void *user, std::int32_t value)
{
  if (user)
    static_cast<std::atomic<int> *> (user)->store (value);
}
} // namespace

DhRenderUI::DhRenderUI (std::shared_ptr<RegionClass> region, QWidget *parent)
    : DhWidget (parent), regionClass (std::move (region))
{
  setWindowTitle (
      QString (_ ("3D Preview - %1")).arg (regionClass->displayName ()));
  resize (900, 640);

  /* The jar download is shared: the material list may already be fetching the
   * very same file, and the manager holds one download per tag. Every signal is
   * filtered by this window's tag, so a download for another version (or an
   * object) does not move this window's status. */
  auto *manager = DhDownloadManager::instance ();
  connect (manager, &DhDownloadManager::progress, this,
           [this] (const QString &tag, int value)
             {
               if (tag != jarDownloadTag () || !fetching)
                 return;
               summary->setText (
                   QString (_ ("Downloading the client jar: %1%")).arg (value));
             });
  connect (manager, &DhDownloadManager::info, this,
           [this] (const QString &tag, const QString &message)
             {
               if (tag != jarDownloadTag () || !fetching)
                 return;
               picked->setText (message);
             });
  connect (manager, &DhDownloadManager::finished, this,
           [this] (const QString &tag, const QString &error)
             {
               if (tag != jarDownloadTag ())
                 return;
               finishFetch (error);
             });

  auto layout = new QVBoxLayout (this);

  /* The toolbar rows are FlowLayouts, not QHBoxLayouts: a row keeps its buttons
   * on one line while the window is wide enough and wraps them onto further
   * lines when it is not. A QHBoxLayout does the opposite — it never wraps and
   * stops the window from becoming narrower than the sum of its buttons (see
   * FlowLayout).
   *
   * Every row sits in a widget of its own: a layout added to a layout is not
   * asked how tall it needs to be for a given width, while a widget's item is,
   * and the wrapping is exactly such a height. */

  /* The first row changes what is drawn without touching the region: where the
   * camera looks and which part of the region is in the picture. */
  auto *viewRowWidget = new QWidget (this);
  auto *viewRow = new FlowLayout (viewRowWidget, 0, 6, 2);
  auto *fitButton = new QPushButton (_ ("&Fit"), this);
  auto *orthoButton = new QPushButton (_ ("&Orthographic"), this);
  orthoButton->setCheckable (true);
  sliceButton = new QPushButton (_ ("&Slice"), this);
  sliceButton->setObjectName ("sliceButton");
  sliceButton->setCheckable (true);
  sliceButton->setToolTip (_ ("Draw only the blocks in this range of Y"));
  sliceLowSpin = new QSpinBox (this);
  sliceLowSpin->setObjectName ("sliceLowSpin");
  sliceLowSpin->setPrefix (QStringLiteral ("Y "));
  sliceLowSpin->setToolTip (_ ("Lowest Y drawn"));
  sliceHighSpin = new QSpinBox (this);
  sliceHighSpin->setObjectName ("sliceHighSpin");
  sliceHighSpin->setPrefix (QStringLiteral ("Y "));
  sliceHighSpin->setToolTip (_ ("Highest Y drawn"));
  auto sliceDash = new QLabel (QStringLiteral ("-"), this);
  boundsButton = new QPushButton (_ ("Region &box"), this);
  boundsButton->setObjectName ("boundsButton");
  boundsButton->setCheckable (true);
  boundsButton->setToolTip (_ ("Show the box around the region being drawn"));
  /* The preview's own option lives in the settings dialog, which belongs to the
   * main window. A render window built by the headless self-check has no main
   * window, so there is no dialog to open and the button is left out. */
  if (MainWindow::instance ())
    {
      auto *settingsButton = new QPushButton (_ ("Preview Settings..."), this);
      settingsButton->setIcon (QIcon::fromTheme (QStringLiteral ("configure")));
      settingsButton->setToolTip (
          _ ("Open the 3D preview setting: whether a missing client jar is "
             "downloaded automatically."));
      connect (settingsButton, &QPushButton::clicked, this,
               []
                 {
                   /* The main window may be gone by the time the button is
                    * clicked; `openSettings ()` is a no-op then. */
                   MainWindow::openSettings (QStringLiteral ("3D Preview"));
                 });
      viewRow->addWidget (settingsButton);
    }
  /* The multi-selection lives in this row too: it changes what the picture
   * shows, but not the region. The buttons still move the view while it is
   * on. */
  selectButton = new QPushButton (_ ("&Multi-select"), this);
  selectButton->setObjectName ("selectButton");
  selectButton->setCheckable (true);
  selectButton->setIcon (QIcon::fromTheme (
      QStringLiteral ("select-rectangular"),
      QIcon::fromTheme (QStringLiteral ("edit-select"))));
  selectButton->setToolTip (
      _ ("Click blocks with the left button to add them to the selection or "
         "take them out; a click off the scene clears it. The middle and right "
         "buttons still move the view."));
  selectionLabel = new QLabel (QString (_ ("No blocks selected")), this);
  selectionLabel->setObjectName ("selectionLabel");
  viewRow->addWidget (fitButton);
  viewRow->addWidget (orthoButton);
  viewRow->addWidget (sliceButton);
  viewRow->addWidget (sliceLowSpin);
  viewRow->addWidget (sliceDash);
  viewRow->addWidget (sliceHighSpin);
  viewRow->addWidget (boundsButton);
  viewRow->addWidget (selectButton);
  viewRow->addWidget (selectionLabel);
  layout->addWidget (viewRowWidget);

  /* The last row acts on the region itself: a fresh scene, its resources, and
   * what is written into it. */
  auto *editRowWidget = new QWidget (this);
  auto *editRow = new FlowLayout (editRowWidget, 0, 6, 2);
  reloadButton = new QPushButton (_ ("&Reload"), this);
  reloadButton->setObjectName ("reloadButton");
  fetchButton = new QPushButton (_ ("Get &Resources"), this);
  modifyButton = new QPushButton (_ ("&Modify Properties..."), this);
  replaceButton = new QPushButton (_ ("&Replace Block..."), this);
  addButton = new QPushButton (_ ("&Add Block..."), this);
  clearSelectionButton = new QPushButton (_ ("Clear &Selection"), this);
  clearSelectionButton->setObjectName ("clearSelectionButton");
  clearSelectionButton->setToolTip (
      _ ("Drop the outline around the selected blocks"));
  clearSelectionButton->setEnabled (false);
  selectedBlocksButton = new QPushButton (_ ("Selected &Blocks..."), this);
  selectedBlocksButton->setObjectName ("selectedBlocksButton");
  selectedBlocksButton->setToolTip (
      _ ("List the selected blocks and mark the one clicked in the list"));
  selectedBlocksButton->setEnabled (false);
  buildProgressBar = new QProgressBar (this);
  buildProgressBar->setObjectName ("buildProgressBar");
  buildProgressBar->setRange (0, 100);
  buildProgressBar->setValue (0);
  buildProgressBar->setTextVisible (true);
  buildProgressBar->setFixedWidth (180);
  buildProgressBar->setVisible (false);
  /* Nothing to change until a click has named a block, and nothing at all for
   * a block that has no properties — see `updateEditButtons ()`. */
  modifyButton->setEnabled (false);
  replaceButton->setEnabled (false);
  addButton->setEnabled (false);
  modifyButton->setToolTip (
      _ ("Change the properties of the block that was clicked"));
  replaceButton->setToolTip (
      _ ("Put another block in place of the block that was clicked"));
  addButton->setToolTip (
      _ ("Put a block into a cell: type its position, or tick \"one block "
         "above\" to drop it on top of what was clicked"));
  editRow->addWidget (reloadButton);
  editRow->addWidget (fetchButton);
  editRow->addWidget (modifyButton);
  editRow->addWidget (replaceButton);
  editRow->addWidget (addButton);
  editRow->addWidget (clearSelectionButton);
  editRow->addWidget (selectedBlocksButton);
  editRow->addWidget (buildProgressBar);

  /* Both rows live in one band whose height is fixed below: the rows wrap
   * inside it, so the picture underneath never moves while the window is
   * dragged. */
  auto *rowsWidget = new QWidget (this);
  auto *rowsLayout = new QVBoxLayout (rowsWidget);
  rowsLayout->setContentsMargins (0, 0, 0, 0);
  rowsLayout->setSpacing (2);
  rowsLayout->addWidget (viewRowWidget);
  rowsLayout->addWidget (editRowWidget);
  layout->addWidget (rowsWidget);

  /* The band is measured once, at the narrowest width this page supports, and
   * keeps that height: if it grew and shrank with the wrapping, the picture
   * below would jump every time a row changed its number of lines — which is
   * what made dragging the window feel shaky. The page never gets narrower
   * than the measured width, so the rows always fit. */
  constexpr int narrowWidth = 420;
  rowsWidget->ensurePolished ();
  const int rowsHeight = viewRow->heightForWidth (narrowWidth)
                         + editRow->heightForWidth (narrowWidth)
                         + rowsLayout->spacing ();
  rowsWidget->setFixedHeight (
      std::clamp (rowsHeight, 64, static_cast<int> (width () * 0.45)));
  setMinimumWidth (narrowWidth);

  /* The status lines are long texts, so they sit on lines of their own below
   * the rows: a wrapping row would push the buttons around as they change. */
  summary = new QLabel (this);
  /* The status line can be longer than a narrow window, and a label asks for as
   * much width as its text needs; letting that decide the window's minimum width
   * would stop the panel from ever reaching the width the rows were measured
   * at. Its width is ignored for the layout's minimum instead, so a long line
   * is simply cut at the right edge. */
  summary->setMinimumWidth (0);
  summary->setSizePolicy (QSizePolicy::Ignored, QSizePolicy::Preferred);
  layout->addWidget (summary);

  /* Read once, before any button can be asked about: refreshing the buttons
   * must not take the region's lock — see `regionHeight`. */
  regionWidth = regionClass->x ();
  regionHeight = regionClass->y ();
  regionDepth = regionClass->z ();

  /* The view is created empty and filled when the scene is ready; building it
   * here would freeze the window for as long as a big region takes. */
  view = new DhRenderWidget (this);
  buildingLabel = new QLabel (_ ("Building the 3D scene..."), this);
  buildingLabel->setAlignment (Qt::AlignCenter);
  layout->addWidget (buildingLabel, 1);
  layout->addWidget (view, 1);
  view->hide ();

  picked = new QLabel (this);
  picked->setWordWrap (true);
  layout->addWidget (picked);

  connect (fitButton, &QPushButton::clicked, view, &DhRenderWidget::resetView);
  connect (orthoButton, &QPushButton::toggled, view,
           &DhRenderWidget::setOrthographic);
  /* The slice and the box update the picture at once: the box the view holds
   * is a toggle on the scene it already has, and a new slice only asks for a
   * scene of that range, without the user pressing Reload. */
  connect (sliceButton, &QPushButton::toggled, this, &DhRenderUI::applySlice);
  connect (sliceLowSpin, &QSpinBox::valueChanged, this,
           [this]
             {
               if (sliceButton->isChecked ())
                 applySlice ();
             });
  connect (sliceHighSpin, &QSpinBox::valueChanged, this,
           [this]
             {
               if (sliceButton->isChecked ())
                 applySlice ();
             });
  connect (boundsButton, &QPushButton::toggled, view,
           &DhRenderWidget::setBoundsVisible);
  /* A scene that came off the worker thread is not the view's own, so the view
   * asks for another one instead of building it here. */
  connect (view, &DhRenderWidget::sliceChanged, this, &DhRenderUI::rebuild);
  connect (reloadButton, &QPushButton::clicked, this, &DhRenderUI::rebuild);
  connect (fetchButton, &QPushButton::clicked, this, &DhRenderUI::toggleFetch);
  connect (modifyButton, &QPushButton::clicked, this, &DhRenderUI::modifyPicked);
  connect (replaceButton, &QPushButton::clicked, this,
           &DhRenderUI::replaceSelection);
  connect (addButton, &QPushButton::clicked, this, [this] { pickBlock (true); });
  /* A write is refused while a job holds the region, so the buttons that open
   * an editor are disabled then instead of failing afterwards. */
  connect (regionClass.get (), &RegionClass::lockedChanged, this,
           [this] { updateEditButtons (); });
  updateEditButtons ();
  /* A click outside the multi-selection names its cell here, including the
   * (-1, -1, -1) of empty space, so the description can be cleared. */
  connect (view, &DhRenderWidget::cellPicked, this, &DhRenderUI::describe);
  /* The view finds the block; this window is the one that holds the region,
   * so the tooltip's text is built here. */
  connect (view, &DhRenderWidget::blockHovered, this,
           &DhRenderUI::describeHover);
  /* A failed GL frame drops the view back to the rasteriser, so the line
   * saying which one is in use has to follow. */
  connect (view, &DhRenderWidget::backendChanged, this,
           &DhRenderUI::updateSummary);

  /* The view knows no region, so a multi-selection asks this window which
   * cells it may pick from; the label follows the outline the view reports. */
  view->setBlockEnumerator (
      [this] (const std::function<void (int, int, int)> &visit)
        { enumerateCells (visit); });
  connect (selectButton, &QPushButton::toggled, this,
           &DhRenderUI::setSelectMode);
  connect (clearSelectionButton, &QPushButton::clicked, this,
           &DhRenderUI::clearSelection);
  connect (selectedBlocksButton, &QPushButton::clicked, this,
           &DhRenderUI::showSelectedBlocks);
  connect (view, &DhRenderWidget::selectionChanged, this,
           &DhRenderUI::updateSelectionStatus);

  /* The colours belong to the settings page; the view keeps what it is given
   * here for every scene it adopts. */
  loadColourSettings ();
  updateSelectionStatus ();

  /* The worker reports progress into an atomic; the window polls it, so the
   * bar moves without the worker touching a widget. */
  buildProgressTimer = new QTimer (this);
  connect (buildProgressTimer, &QTimer::timeout, this,
           [this]
             {
               if (building && buildProgress)
                 buildProgressBar->setValue (buildProgress->load ());
             });

  /* The spin boxes offer the region's own Y range, which is also what a
   * freshly checked "Slice" draws: the whole height. */
  updateSliceRange ();

  /* The window paints first, then the build starts: the label above says what
   * is going on while it runs. */
  QTimer::singleShot (0, this, &DhRenderUI::startSceneBuild);
}

void
DhRenderUI::showEvent (QShowEvent *event)
{
  DhWidget::showEvent (event);
  /* The colours are set on the settings page, which may have been used while
   * this panel was hidden; reading them again on every show is cheap and makes
   * a change take effect as soon as the picture comes back. */
  if (view)
    loadColourSettings ();
}

void
DhRenderUI::startSceneBuild ()
{
  if (building)
    {
      /* The reload cannot run concurrently with the build, but it must not be
       * lost either: it runs as soon as the build finishes. */
      rebuildPending = true;
      return;
    }
  building = true;
  setBuilding (true);
  /* The resource layer has to name the version's jar before the scene is
   * built, and it is core-global state, so it is set here on the GUI thread. */
  configureResources ();
  /* A reload is also what a changed region or slice comes through, so the
   * controls are brought up to date before the slice is read. Telling the view
   * a clamped range changed would only ask for this very build again. */
  updateSliceRange ();
  applySlice ();
  rebuildPending = false;

  auto region = regionClass;
  void *core = DhCore::instance ()->handle ();
  buildProgress = std::make_shared<std::atomic<int>> (0);
  auto progress = buildProgress;
  /* What the worker is to draw: the view's slice at the moment the build
   * starts. The worker holds the region's lock while it copies, and a slice
   * build reports no progress of its own, so the bar only knows about the
   * whole-region build. */
  const std::optional<std::pair<int, int>> slice = view->slice ();
  buildProgressBar->setRange (0, slice ? 0 : 100);
  auto *watcher = new QFutureWatcher<std::shared_ptr<DhlrcScene>> (this);
  sceneWatcher = watcher;
  connect (watcher, &QFutureWatcher<std::shared_ptr<DhlrcScene>>::finished,
           this,
           [this, watcher]
             {
               sceneWatcher = nullptr;
               watcher->deleteLater ();
               sceneBuilt (watcher->result ());
             });
  watcher->setFuture (QtConcurrent::run (
      [region, core, progress, slice] () -> std::shared_ptr<DhlrcScene>
        {
          /* The region is locked while everything the scene needs is copied out
           * of it: the GUI cannot change it under the build, and the copy runs
           * off the GUI thread. */
          AutoLocker locker (*region);
          DhlrcScene *raw = nullptr;
          if (slice)
            raw = dhlrc_render_scene_new_slice (core, region->get_region (),
                                                slice->first, slice->second);
          else
            raw = dhlrc_render_scene_new_reported (core, region->get_region (),
                                                   &sceneProgress,
                                                   progress.get ());
          return std::shared_ptr<DhlrcScene> (raw, dhlrc_render_scene_free);
        }));
  buildProgressTimer->start (100);
}

void
DhRenderUI::setBuilding (bool value)
{
  /* The big label stands in for the view only while there is no picture yet.
   * A reload keeps the old picture up until the new scene is ready, so the
   * layout does not jump and the window does not look emptied while it
   * works. */
  buildingLabel->setVisible (value && !view->ready ());
  reloadButton->setEnabled (!value);
  /* The bar is the report while the build runs; it starts from zero each
   * time and goes away with the last of it. */
  buildProgressBar->setVisible (value);
  if (value)
    {
      buildProgressBar->setValue (0);
      summary->setText (_ ("Building the 3D scene..."));
    }
  else if (buildProgressTimer)
    buildProgressTimer->stop ();
}

void
DhRenderUI::sceneBuilt (std::shared_ptr<DhlrcScene> scene)
{
  building = false;
  /* Puts the label and the bar away and enables "Reload" again; a reload asked
   * for during the build runs right after this. */
  setBuilding (false);
  buildProgress.reset ();
  /* The bounds box wraps the region as it is now, which is also the size the
   * scene was just built from. */
  view->setRegionExtent (regionClass->x (), regionClass->y (),
                         regionClass->z ());
  view->setScene (std::move (scene));
  view->show ();
  /* A scene adopted later keeps the colours the view was given, but a setting
   * may have been edited while the build ran; pushing the cached ones again is
   * one call and cannot go stale. */
  applyColours ();
  updateSummary ();

  /* Nothing drawn and the version's jar is missing: fetch it by itself, but
   * only when the setting allows it. */
  const bool allowed = DhCore::instance ()->settingValue ("DownloadClientJar")
                       != QLatin1String ("false");
  if (view->quadCount () == 0 && allowed && jarState () == 0)
    {
      startFetch ();
      return;
    }
  if (rebuildPending)
    {
      rebuildPending = false;
      startSceneBuild ();
    }
}

void
DhRenderUI::configureResources ()
{
  /* The block models and textures are only in the version's client jar, so the
   * resource layer is pointed at the version this region belongs to before a
   * scene is built from it. A frontend that had already configured its own
   * packs still wins: this only decides which version's jar is used. */
  dhlrc_core_resources_configure_for_data_version (
      DhCore::instance ()->handle (),
      static_cast<int32_t> (regionClass->dataVersion ()), nullptr, 0);
}

int
DhRenderUI::jarState () const
{
  return dhlrc_core_client_jar_state (
      DhCore::instance ()->handle (),
      static_cast<int32_t> (regionClass->dataVersion ()));
}

QString
DhRenderUI::jarPath () const
{
  char *path = dhlrc_core_client_jar_path (
      DhCore::instance ()->handle (),
      static_cast<int32_t> (regionClass->dataVersion ()));
  const QString text = path ? QString::fromUtf8 (path) : QString ();
  if (path)
    dhlrc_string_free (path);
  return text;
}

QString
DhRenderUI::jarDownloadTag () const
{
  return DhDownloadManager::clientJarTag (
      static_cast<int> (regionClass->dataVersion ()));
}

void
DhRenderUI::toggleFetch ()
{
  if (fetching)
    {
      /* The worker stops between blocks and reports the cancellation as the
       * download's error, so the `finished` that reaches `finishFetch ()`
       * puts the button back and says why. Another page that joined the same
       * jar download hears that end as well. */
      DhDownloadManager::instance ()->cancel (jarDownloadTag ());
      return;
    }
  startFetch ();
}

void
DhRenderUI::startFetch ()
{
  if (fetching || jarState () != 0)
    return;
  setFetching (true);
  /* A `false` means that download is already on its way (another window or the
   * material list asked first): nothing is started here, and the progress and
   * the finish still arrive through the manager. */
  DhDownloadManager::instance ()->ensureClientJar (
      static_cast<int> (regionClass->dataVersion ()));
}

void
DhRenderUI::finishFetch (const QString &error)
{
  setFetching (false);
  if (error.isEmpty ())
    {
      /* The jar is in place now, so the resource layer has something to read
       * and the scene can be built again. */
      rebuild ();
      return;
    }
  picked->setText (QString (_ ("The client jar could not be downloaded: %1"))
                       .arg (error));
  updateSummary ();
}

void
DhRenderUI::setFetching (bool value)
{
  fetching = value;
  fetchButton->setText (value ? _ ("&Cancel") : _ ("Get &Resources"));
}

void
DhRenderUI::rebuild ()
{
  /* A fresh snapshot of the region, built off the GUI thread like the first
   * one. `startSceneBuild ()` ignores a second request while one is running. */
  startSceneBuild ();
}

void
DhRenderUI::applySlice ()
{
  if (sliceButton->isChecked ())
    view->setSlice (std::make_pair (sliceLowSpin->value (),
                                    sliceHighSpin->value ()));
  else
    view->setSlice (std::nullopt);
}

void
DhRenderUI::updateSliceRange ()
{
  /* The region's own height is the only range that can draw anything, so the
   * boxes cannot name a layer outside it. Signals are blocked: this only
   * adjusts the offered range, and the caller pushes the slice afterwards. */
  const qint32 high = std::max (0, regionClass->y () - 1);
  const QSignalBlocker lowBlocker (sliceLowSpin);
  const QSignalBlocker highBlocker (sliceHighSpin);
  sliceLowSpin->setRange (0, high);
  sliceHighSpin->setRange (0, high);
  /* The first range offered is the whole region, which is what an unchecked
   * slice draws. */
  if (!sliceRangeKnown)
    {
      sliceRangeKnown = true;
      sliceLowSpin->setValue (0);
      sliceHighSpin->setValue (high);
    }
}

void
DhRenderUI::updateSummary ()
{
  const qint32 x = regionClass->x ();
  const qint32 y = regionClass->y ();
  const qint32 z = regionClass->z ();
  /* Which backend is drawing is part of the status: the GPU path and the
   * rasteriser can give slightly different pictures, and a failed GL frame
   * changes the answer while the window is open. */
  const QString backend = view->usingGl () ? _ ("drawn with OpenGL")
                                           : _ ("drawn by the CPU");
  QString text
      = QString (_ ("%1 x %2 x %3, %4 quads, %5 blocks not drawn, %6"))
            .arg (x)
            .arg (y)
            .arg (z)
            .arg (view->quadCount ())
            .arg (view->skippedCount ())
            .arg (backend);
  /* The slice is part of what the line describes: the sizes above are the
   * region's, while the picture holds one range of its Y. */
  const std::optional<std::pair<int, int>> slice = view->slice ();
  if (slice)
    text = QString (_ ("Slice Y%1-%2. %3"))
               .arg (slice->first)
               .arg (slice->second)
               .arg (text);
  summary->setText (text);

  if (fetching)
    return;
  if (view->quadCount () > 0)
    {
      picked->setText (
          _ ("Drag with the left button to turn, with the middle or right "
             "button to move, and use the wheel to zoom. A click names the "
             "block under it."));
      return;
    }

  /* Nothing was drawn: a version's models and textures are only in its client
   * jar, so say where one belongs — and offer to fetch it when the setting
   * says that is allowed. */
  const QString path = jarPath ();
  if (path.isEmpty ())
    picked->setText (
        _ ("Nothing could be drawn: the block models and textures of this "
           "region's version could not be placed, and the version is not one "
           "the client jar table knows."));
  else
    picked->setText (
        QString (_ ("Nothing could be drawn. The block models and textures of a "
                    "version live in its client jar, which belongs at %1. "
                    "Press \"Get Resources\" to download it (about 30 MB)."))
            .arg (path));
}

void
DhRenderUI::describe (int x, int y, int z)
{
  AutoLocker locker (*regionClass);
  void *raw = regionClass->get_region ();
  const qint32 index = regionClass->index (x, y, z);
  if (index < 0)
    {
      hasPicked = false;
      pickedPropertyCount = 0;
      updateEditButtons ();
      /* A click off the scene is reported as (-1, -1, -1): that is a cleared
       * description, not a coordinate the user might be puzzled by. */
      if (x < 0 || y < 0 || z < 0)
        picked->clear ();
      else
        picked->setText (QString (_ ("(%1, %2, %3) is outside the region"))
                             .arg (x)
                             .arg (y)
                             .arg (z));
      return;
    }

  pickedX = x;
  pickedY = y;
  pickedZ = z;
  hasPicked = true;
  updateEditButtons ();
  /* The scene may have been replaced since the window was built, and the
   * outline colours belong to it. */
  applyColours ();

  const quint32 id = regionClass->blockId (index);
  /* Whether the property dialog has anything to show for this block: a block
   * without properties, or one whose palette entry names none, cannot be
   * modified. */
  pickedPropertyCount = static_cast<int> (regionClass->palettePropertyLen (id));
  const char *name = region_get_palette_id_name (raw, id);
  picked->setText (QString (_ ("Block (%1, %2, %3): %4"))
                       .arg (x)
                       .arg (y)
                       .arg (z)
                       .arg (QString::fromUtf8 (name ? name : "")));
  if (name)
    string_free (name);
}

void
DhRenderUI::modifyPicked ()
{
  if (!hasPicked)
    return;

  /* The dialog writes through the region and locks it itself. The picture is a
   * snapshot taken while the scene was built, so a change only shows once a
   * fresh one is taken — which is what `rebuild ()` does. */
  PropertyModifyUI dialog (regionClass, pickedX, pickedY, pickedZ, this);
  bool written = false;
  connect (&dialog, &PropertyModifyUI::propertiesModified, this,
           [&written] { written = true; });
  dialog.exec ();
  if (!written)
    return;

  const qint32 x = pickedX;
  const qint32 y = pickedY;
  const qint32 z = pickedZ;
  rebuild ();
  describe (x, y, z);
}

void
DhRenderUI::updateEditButtons ()
{
  /* Nothing here may take the region's lock: see `regionHeight` in the
   * header. */
  const bool unlocked = !regionClass->locked ();
  const bool picked = hasPicked && unlocked;
  /* A block without properties has nothing to change, so the property dialog
   * stays closed for it; replacing it still works. */
  const bool modifiable = picked && pickedPropertyCount > 0;
  if (updatingButtons)
    return;
  updatingButtons = true;
  if (modifyButton)
    {
      modifyButton->setEnabled (modifiable);
      /* Say why it cannot be used: a block state without properties has
       * nothing for the dialog to show. */
      modifyButton->setToolTip (
          picked && pickedPropertyCount == 0
              ? _ ("This block has no properties to change")
              : _ ("Change the properties of the block that was clicked"));
    }
  if (replaceButton)
    {
      replaceButton->setEnabled (picked);
      /* With a multi-selection the button puts one block into every selected
       * cell, so its tooltip says so. */
      replaceButton->setToolTip (
          view && !view->selectedCells ().isEmpty ()
              ? _ ("Put one block into every selected cell")
              : _ ("Put another block in place of the block that was clicked"));
    }
  /* Adding never needs a picked block: the position is typed into the dialog,
   * which is the only way to reach an empty cell. */
  if (addButton)
    addButton->setEnabled (unlocked);
  updatingButtons = false;
}

void
DhRenderUI::pickBlock (bool above)
{
  /* The chooser needs a cell to start from, not a picked block: without one it
   * starts in the middle of the region, and the dialog's own position fields
   * are what gets the block exactly where it belongs. */
  const qint32 startX = hasPicked ? pickedX : regionWidth / 2;
  const qint32 startY = hasPicked ? pickedY : regionHeight / 2;
  const qint32 startZ = hasPicked ? pickedZ : regionDepth / 2;

  /* The chooser prefills itself from the block at the position it is given, so
   * a replacement of the same kind keeps its properties. */
  BlockPickerUI dialog (regionClass, startX, startY, startZ, this);
  if (above)
    dialog.setPlaceAbove (true);
  bool written = false;
  connect (&dialog, &BlockPickerUI::blockModified, this,
           [&written] { written = true; });
  dialog.exec ();
  if (!written)
    return;

  /* Where the block really went, which the position fields may have moved. */
  qint32 x = startX;
  qint32 y = startY;
  qint32 z = startZ;
  dialog.target (&x, &y, &z);
  rebuild ();
  describe (x, y, z);
}

void
DhRenderUI::enumerateCells (const std::function<void (int, int, int)> &visit)
{
  if (!visit)
    return;
  /* The walk reads the whole block array, which a loading job may be writing,
   * so it holds the lock the way a scene build does: the click waits for the
   * job instead of racing it. */
  AutoLocker locker (*regionClass);
  void *raw = regionClass->get_region ();
  if (!raw)
    return;

  /* Which palette entries are air: a cell holding one is not drawn, and an
   * outline around empty space is not what a band selection means. An empty
   * palette (a region nothing was written to yet) has no entry to skip. */
  const std::size_t paletteLength = region_get_palette_len (raw);
  /* No palette at all: nothing has ever been written into the region, so there
   * is no cell to select. */
  if (paletteLength == 0)
    return;
  std::vector<bool> air (paletteLength, false);
  for (std::size_t id = 0; id < paletteLength; id++)
    {
      const char *name = region_get_palette_id_name (raw, id);
      air[id] = isAirName (name);
      if (name)
        string_free (name);
    }

  /* The block array is ordered y, then z, then x (see `region_get_index`), so
   * the walk follows the array and needs no index call per cell. */
  const qint32 sizeX = region_get_x (raw);
  const qint32 sizeY = region_get_y (raw);
  const qint32 sizeZ = region_get_z (raw);
  std::size_t index = 0;
  for (qint32 y = 0; y < sizeY; y++)
    {
      for (qint32 z = 0; z < sizeZ; z++)
        {
          for (qint32 x = 0; x < sizeX; x++, index++)
            {
              const quint32 id = region_get_block_id_by_index (raw, index);
              if (id < air.size () && air[id])
                continue;
              visit (x, y, z);
            }
        }
    }
}

void
DhRenderUI::loadColourSettings ()
{
  /* The settings hold the colours as "#rrggbb"; a value that is not a colour
   * falls back to the default, so a hand-edited setting cannot make the
   * preview unreadable. The pickers themselves live on the settings page,
   * which writes these keys. */
  highlightColour = colourSetting (QStringLiteral ("HighlightColour"),
                                   QColor (0, 0, 0));
  backgroundColor = colourSetting (QStringLiteral ("BackgroundColour"),
                                   QColor (0x20, 0x28, 0x30));
  boundsColour = colourSetting (QStringLiteral ("BoundsColour"),
                                QColor (0x00, 0xc8, 0xff));
  markerColour = colourSetting (QStringLiteral ("MarkerColour"),
                                QColor (0xff, 0x20, 0x60));

  applyColours ();
}

void
DhRenderUI::applyColours ()
{
  view->setHighlightColour (highlightColour);
  view->setBackgroundColour (backgroundColor);
  view->setBoundsColour (boundsColour);
  view->setMarkerColour (markerColour);
}

QColor
DhRenderUI::colourSetting (const QString &key, const QColor &fallback) const
{
  const QColor colour (DhCore::instance ()->settingValue (key));
  return colour.isValid () ? colour : fallback;
}

void
DhRenderUI::setSelectMode (bool on)
{
  view->setSelectMode (on);
  /* The two buttons that work on the selection only make sense in this mode;
   * the toggle itself stays usable so the mode can always be left. */
  clearSelectionButton->setEnabled (on);
  updateSelectionStatus ();
}

void
DhRenderUI::clearSelection ()
{
  /* Outside select mode the row is inactive with the rest of the feature. */
  if (!selectButton->isChecked ())
    return;
  /* The view does not report this one: it is the owner's own change. */
  view->setSelectedCells ({});
  updateSelectionStatus ();
}

void
DhRenderUI::updateSelectionStatus ()
{
  const auto count = view->selectedCells ().size ();
  selectionLabel->setText (
      count == 0
          ? QString (_ ("No blocks selected"))
          : QString (_ ("%1 block(s) selected")).arg (count));
  /* The list is worth opening only when it would have rows. */
  selectedBlocksButton->setEnabled (count > 0);
  /* The buttons write into the region, and a selection does not change what
   * "the last clicked block" is; keeping them in step here means a disabled
   * button never stays disabled for a stale reason. */
  updateEditButtons ();
}

void
DhRenderUI::selectCell (const std::array<int, 3> &cell)
{
  /* The marker is the view's own outline for one cell; the description makes
   * that cell the panel's picked block, so the property and replace buttons
   * act on what was singled out here. */
  view->setMarkerCell (cell);
  describe (cell[0], cell[1], cell[2]);
}

void
DhRenderUI::showSelectedBlocks ()
{
  const QVector<DhRenderWidget::BlockCell> cells = view->selectedCells ();
  if (cells.isEmpty ())
    return;

  QDialog dialog (this);
  dialog.setWindowTitle (QString (_ ("Selected Blocks")));
  dialog.resize (380, 420);
  auto *layout = new QVBoxLayout (&dialog);

  auto *table = new QTableWidget (static_cast<int> (cells.size ()), 2,
                                  &dialog);
  table->setObjectName ("selectedBlocksTable");
  table->setHorizontalHeaderLabels ({ QString (_ ("Position")),
                                      QString (_ ("Block")) });
  table->setEditTriggers (QAbstractItemView::NoEditTriggers);
  table->setSelectionBehavior (QAbstractItemView::SelectRows);
  table->setSelectionMode (QAbstractItemView::SingleSelection);
  table->verticalHeader ()->setVisible (false);
  table->horizontalHeader ()->setStretchLastSection (true);

  {
    /* The names come from the palette, which a background job may be
     * rewriting, so the table is filled under the region's lock. */
    AutoLocker locker (*regionClass);
    void *raw = regionClass->get_region ();
    for (qsizetype i = 0; i < cells.size (); i++)
      {
        const auto &cell = cells.at (i);
        QString name;
        const qint32 index = regionClass->index (cell[0], cell[1], cell[2]);
        if (index >= 0 && raw)
          {
            const quint32 id = region_get_block_id_by_index (
                raw, static_cast<size_t> (index));
            const char *text = region_get_palette_id_name (raw, id);
            name = QString::fromUtf8 (text ? text : "");
            if (text)
              string_free (text);
          }
        table->setItem (
            static_cast<int> (i), 0,
            new QTableWidgetItem (QStringLiteral ("(%1, %2, %3)")
                                      .arg (cell[0])
                                      .arg (cell[1])
                                      .arg (cell[2])));
        table->setItem (static_cast<int> (i), 1,
                        new QTableWidgetItem (name));
      }
  }
  /* The row that the marker box in the picture belongs to carries the
   * marker's colour, so the list says which cell that small box is on even
   * when the selection is long. */
  if (const auto marker = view->markerCell ())
    for (qsizetype i = 0; i < cells.size (); i++)
      if (cells.at (i) == *marker)
        for (int column = 0; column < 2; column++)
          if (auto *item = table->item (static_cast<int> (i), column))
            item->setBackground (QBrush (markerColour.lighter (150)));

  table->resizeColumnsToContents ();
  layout->addWidget (table);

  auto *buttons = new QDialogButtonBox (QDialogButtonBox::Close, &dialog);
  connect (buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget (buttons);

  /* The rows are a snapshot taken above, so a row index names its cell even if
   * the selection changes while the list is open. */
  connect (table, &QTableWidget::cellClicked, &dialog,
           [this, cells] (int row, int)
             {
               if (row < 0 || row >= cells.size ())
                 return;
               selectCell (cells.at (row));
             });

  dialog.exec ();
}

void
DhRenderUI::replaceSelection ()
{
  const QVector<DhRenderWidget::BlockCell> cells = view->selectedCells ();
  if (cells.isEmpty ())
    {
      /* Without a multi-selection a replace still means the one cell clicked
       * in the picture. */
      pickBlock (false);
      return;
    }

  /* The chooser writes the cell it is pointed at, which is how the chosen
   * state is spelled out; the batch below then puts that same state into every
   * selected cell and records the whole operation once. */
  const qint32 startX = hasPicked ? pickedX : cells.first ()[0];
  const qint32 startY = hasPicked ? pickedY : cells.first ()[1];
  const qint32 startZ = hasPicked ? pickedZ : cells.first ()[2];
  BlockPickerUI dialog (regionClass, startX, startY, startZ, this);
  const qsizetype changesBefore = regionClass->changes ().size ();
  /* Accepting means a block was chosen and the target cell either got it or
   * already held it — in both cases the state at the target is the one to
   * paint. Cancelling writes nothing to paint. */
  if (dialog.exec () != QDialog::Accepted)
    return;

  /* Where the chooser really wrote, and the state it left there. */
  qint32 x = startX;
  qint32 y = startY;
  qint32 z = startZ;
  dialog.target (&x, &y, &z);
  const RegionClass::BlockState state = regionClass->blockState (x, y, z);

  /* If the chooser wrote its one cell it recorded that write; the batch is the
   * one action the user asked for, so that entry is folded away and the single
   * entry below covers the whole batch — the undo list stays one step for one
   * action. */
  if (regionClass->changes ().size () == changesBefore + 1)
    regionClass->revertChange (changesBefore);

  /* The ABI copies the strings out of these arrays while the lock is held, so
   * they have to outlive the write loop. */
  QList<QByteArray> nameBytes;
  QList<QByteArray> dataBytes;
  for (const auto &property : state.properties)
    {
      nameBytes.append (property.first.toUtf8 ());
      dataBytes.append (property.second.toUtf8 ());
    }
  QList<const char *> namePtrs;
  QList<const char *> dataPtrs;
  for (const auto &bytes : nameBytes)
    namePtrs.append (bytes.constData ());
  for (const auto &bytes : dataBytes)
    dataPtrs.append (bytes.constData ());
  const QByteArray nameUtf8 = state.name.toUtf8 ();

  QVector<DhRenderWidget::BlockCell> writtenCells;
  QVector<RegionClass::BlockState> previousStates;
  QString error;
  {
    /* One lock for the whole batch: reading what each cell held and writing
     * its replacement are two halves of the same undo step. */
    AutoLocker locker (*regionClass);
    void *raw = regionClass->get_region ();
    if (!raw)
      return;
    for (const auto &cell : cells)
      {
        const qint32 index = regionClass->index (cell[0], cell[1], cell[2]);
        if (index < 0)
          continue;
        previousStates.append (
            regionClass->blockState (cell[0], cell[1], cell[2]));
        quint32 id = 0;
        const char *message = region_set_block_state (
            raw, static_cast<size_t> (index), nameUtf8.constData (),
            namePtrs.constData (), dataPtrs.constData (),
            static_cast<size_t> (namePtrs.size ()), &id);
        if (message)
          {
            error = QString::fromUtf8 (message);
            string_free (message);
            break;
          }
        writtenCells.append (cell);
      }
  }

  if (writtenCells.isEmpty ())
    {
      if (!error.isEmpty ())
        QMessageBox::critical (this, _ ("Error!"), error);
      return;
    }

  const QString description
      = QString (_ ("Replaced %1 block(s)")).arg (writtenCells.size ());
  regionClass->recordChange (
      description, state.name,
      [cells = writtenCells, states = previousStates] (RegionClass &region)
        {
          for (qsizetype i = 0; i < cells.size (); i++)
            {
              const auto &cell = cells.at (i);
              if (!region.setBlockState (cell[0], cell[1], cell[2],
                                         states.at (i).name,
                                         states.at (i).properties)
                       .isEmpty ())
                return false;
            }
          return true;
        });

  /* The picture is a snapshot from before the write, and its outlines are the
   * cells the user still has selected. */
  view->setSelectedCells (cells);
  rebuild ();
  if (hasPicked)
    describe (pickedX, pickedY, pickedZ);
  else
    describe (x, y, z);

  if (!error.isEmpty ())
    QMessageBox::critical (this, _ ("Error!"), error);
}

void
DhRenderUI::describeHover (int x, int y, int z)
{
  AutoLocker locker (*regionClass);
  void *raw = regionClass->get_region ();
  const qint32 index = regionClass->index (x, y, z);
  if (index < 0)
    {
      view->setHoverInfo (QString ());
      return;
    }

  /* The very text the block reader shows, so the wording stays in one place;
   * the coordinate is put in front because the tooltip has no other way to say
   * where the block is. */
  const QString info
      = BlockReaderUI::getBlockInfo (raw, static_cast<quint32> (index), false);
  view->setHoverInfo (QString (_ ("Block (%1, %2, %3)")).arg (x).arg (y).arg (z)
                      + QLatin1Char ('\n') + info);
}
