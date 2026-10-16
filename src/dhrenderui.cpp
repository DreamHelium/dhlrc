#include "dhrenderui.h"
#include "propertymodifyui.h"

#include "blockreaderui.h"
#include "dhcore.h"
#include "dhrenderwidget.h"
#include "region.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <libintl.h>
#include <utility>

#define _(str) gettext (str)

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

  auto controls = new QHBoxLayout ();
  auto fitButton = new QPushButton (_ ("&Fit"), this);
  auto orthoButton = new QPushButton (_ ("&Orthographic"), this);
  orthoButton->setCheckable (true);
  auto reloadButton = new QPushButton (_ ("&Reload"), this);
  fetchButton = new QPushButton (_ ("Get &Resources"), this);
  modifyButton = new QPushButton (_ ("&Modify Properties..."), this);
  /* Nothing to modify until a click has named a block. */
  modifyButton->setEnabled (false);
  summary = new QLabel (this);
  controls->addWidget (fitButton);
  controls->addWidget (orthoButton);
  controls->addWidget (reloadButton);
  controls->addWidget (fetchButton);
  controls->addWidget (modifyButton);
  controls->addWidget (summary, 1);
  layout->addLayout (controls);

  /* The lock is held while the scene is built: everything the view needs is
   * copied out of the region, so it never touches it again. */
  {
    AutoLocker locker (*regionClass);
    configureResources ();
    view = new DhRenderWidget (DhCore::instance ()->handle (),
                               regionClass->get_region (), this);
  }
  layout->addWidget (view, 1);

  picked = new QLabel (this);
  picked->setWordWrap (true);
  layout->addWidget (picked);

  connect (fitButton, &QPushButton::clicked, view, &DhRenderWidget::resetView);
  connect (orthoButton, &QPushButton::toggled, view,
           &DhRenderWidget::setOrthographic);
  connect (reloadButton, &QPushButton::clicked, this, &DhRenderUI::rebuild);
  connect (fetchButton, &QPushButton::clicked, this, &DhRenderUI::toggleFetch);
  connect (modifyButton, &QPushButton::clicked, this, &DhRenderUI::modifyPicked);
  connect (view, &DhRenderWidget::blockPicked, this, &DhRenderUI::describe);
  /* The view finds the block; this window is the one that holds the region,
   * so the tooltip's text is built here. */
  connect (view, &DhRenderWidget::blockHovered, this,
           &DhRenderUI::describeHover);
  /* A failed GL frame drops the view back to the rasteriser, so the line
   * saying which one is in use has to follow. */
  connect (view, &DhRenderWidget::backendChanged, this,
           &DhRenderUI::updateSummary);

  updateSummary ();

  /* Nothing drawn and the version's jar is missing: fetch it by itself, but
   * only after the window is up, and only when the setting allows it. */
  const bool allowed
      = DhCore::instance ()->settingValue ("DownloadClientJar") != QLatin1String ("false");
  if (view->quadCount () == 0 && allowed && jarState () == 0)
    QTimer::singleShot (0, this, &DhRenderUI::startFetch);
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
  {
    AutoLocker locker (*regionClass);
    configureResources ();
    view->rebuild (DhCore::instance ()->handle (), regionClass->get_region ());
  }
  view->resetView ();
  updateSummary ();
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
  summary->setText (QString (_ ("%1 x %2 x %3, %4 quads, %5 blocks not drawn, "
                                "%6"))
                        .arg (x)
                        .arg (y)
                        .arg (z)
                        .arg (view->quadCount ())
                        .arg (view->skippedCount ())
                        .arg (backend));

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
  if (modifyButton)
    modifyButton->setEnabled (true);

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
