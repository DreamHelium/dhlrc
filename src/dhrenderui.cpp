#include "dhrenderui.h"
#include "blockpickerui.h"
#include "propertymodifyui.h"

#include "blockreaderui.h"
#include "dhcore.h"
#include "dhrenderwidget.h"
#include "region.h"

#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QSpinBox>
#include <algorithm>
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

  downloader = std::make_shared<DhDownloader> ();
  connect (downloader.get (), &DhDownloader::finished, this,
           &DhRenderUI::finishFetch);
  connect (downloader.get (), &DhDownloader::progress, this,
           [&] (int value)
             {
               if (fetching)
                 summary->setText (
                     QString (_ ("Downloading the client jar: %1%")).arg (value));
             });
  connect (downloader.get (), &DhDownloader::info, this,
           [&] (const QString &message)
             {
               if (fetching)
                 picked->setText (message);
             });

  auto layout = new QVBoxLayout (this);

  /* The first row changes what is drawn without touching the region: where the
   * camera looks and which part of the region is in the picture. */
  auto controls = new QHBoxLayout ();
  auto fitButton = new QPushButton (_ ("&Fit"), this);
  auto orthoButton = new QPushButton (_ ("&Orthographic"), this);
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
  summary = new QLabel (this);
  controls->addWidget (fitButton);
  controls->addWidget (orthoButton);
  controls->addWidget (sliceButton);
  controls->addWidget (sliceLowSpin);
  controls->addWidget (sliceDash);
  controls->addWidget (sliceHighSpin);
  controls->addWidget (boundsButton);
  controls->addWidget (summary, 1);
  layout->addLayout (controls);

  /* The second row acts on the region itself: a fresh scene, its resources,
   * and what is written into it. */
  auto regionControls = new QHBoxLayout ();
  reloadButton = new QPushButton (_ ("&Reload"), this);
  reloadButton->setObjectName ("reloadButton");
  fetchButton = new QPushButton (_ ("Get &Resources"), this);
  modifyButton = new QPushButton (_ ("&Modify Properties..."), this);
  replaceButton = new QPushButton (_ ("&Replace Block..."), this);
  addButton = new QPushButton (_ ("&Add Block..."), this);
  buildProgressBar = new QProgressBar (this);
  buildProgressBar->setObjectName ("buildProgressBar");
  buildProgressBar->setRange (0, 100);
  buildProgressBar->setValue (0);
  buildProgressBar->setTextVisible (true);
  buildProgressBar->setFixedWidth (180);
  buildProgressBar->setVisible (false);
  /* Nothing to change until a click has named a block. */
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
  regionControls->addWidget (reloadButton);
  regionControls->addWidget (fetchButton);
  regionControls->addWidget (modifyButton);
  regionControls->addWidget (replaceButton);
  regionControls->addWidget (addButton);
  regionControls->addWidget (buildProgressBar);
  regionControls->addStretch (1);
  layout->addLayout (regionControls);

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
           [this] { pickBlock (false); });
  connect (addButton, &QPushButton::clicked, this, [this] { pickBlock (true); });
  /* A write is refused while a job holds the region, so the buttons that open
   * an editor are disabled then instead of failing afterwards. */
  connect (regionClass.get (), &RegionClass::lockedChanged, this,
           [this] { updateEditButtons (); });
  updateEditButtons ();
  connect (view, &DhRenderWidget::blockPicked, this, &DhRenderUI::describe);
  /* The view finds the block; this window is the one that holds the region,
   * so the tooltip's text is built here. */
  connect (view, &DhRenderWidget::blockHovered, this,
           &DhRenderUI::describeHover);
  /* A failed GL frame drops the view back to the rasteriser, so the line
   * saying which one is in use has to follow. */
  connect (view, &DhRenderWidget::backendChanged, this,
           &DhRenderUI::updateSummary);

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

void
DhRenderUI::toggleFetch ()
{
  if (fetching)
    {
      downloader->stop ();
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
  downloader->downloadClientJar (
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
      updateEditButtons ();
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

  const quint32 id = regionClass->blockId (index);
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
  const bool usable = hasPicked && unlocked;
  if (updatingButtons)
    return;
  updatingButtons = true;
  if (modifyButton)
    modifyButton->setEnabled (usable);
  if (replaceButton)
    replaceButton->setEnabled (usable);
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
