#include "dhpreview.h"

#include "blockreaderui.h"
#include "dhcore.h"
#include "dhgpuwidget.h"
#include "dhloadjob.h"
#include "manageregionui.h"
#include "region.h"
#include "regionclass.h"
#include "utility.h"

#include <libintl.h>

#define _(str) gettext (str)
/* The gettext shim's `asprintf` macro has to go before the Qt headers, as in
 * the loader. */
#undef asprintf

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QRegularExpression>
#include <QStringList>
#include <QThread>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <vector>

namespace
{
/* The panel the picture is painted on, the colour the 3D view uses. */
const QColor BACKGROUND (0x20, 0x28, 0x30);

/* How many frames the frame time is measured over. */
const int CPU_FRAMES = 20;
const int GPU_FRAMES = 50;

void
report (const QString &text)
{
  std::fprintf (stderr, "dhlrc_qt: %s\n", text.toUtf8 ().constData ());
}

int
fail (const QString &text)
{
  report (text);
  return 1;
}

/* Prints a download's own status lines, the way the 3D window's progress label
 * would show them. */
void
downloadInfo (void *, const char *message)
{
  if (message)
    report (QString::fromUtf8 (message));
}

/* `WxH`, as `--size` spells it. */
bool
parseSize (const QString &text, int *width, int *height, QString *error)
{
  const QStringList parts = text.split (QRegularExpression (QStringLiteral ("[xX]")));
  bool ok = parts.size () == 2;
  int parsedWidth = 0;
  int parsedHeight = 0;
  if (ok)
    {
      parsedWidth = parts[0].toInt (&ok);
      if (ok)
        parsedHeight = parts[1].toInt (&ok);
    }
  if (!ok || parsedWidth <= 0 || parsedHeight <= 0)
    {
      *error = QString (_ ("%1 is not a size; write it as WxH")).arg (text);
      return false;
    }
  *width = parsedWidth;
  *height = parsedHeight;
  return true;
}

/* `X,Y`, as `--hover` spells one picture pixel. */
bool
parseHover (const QString &text, int *x, int *y, QString *error)
{
  const QStringList parts
      = text.split (QRegularExpression (QStringLiteral ("\\s*,\\s*")));
  bool ok = parts.size () == 2;
  int parsedX = 0;
  int parsedY = 0;
  if (ok)
    {
      parsedX = parts[0].toInt (&ok);
      if (ok)
        parsedY = parts[1].toInt (&ok);
    }
  if (!ok || parsedX < 0 || parsedY < 0)
    {
      *error = QString (_ ("%1 is not a pixel; write it as X,Y")).arg (text);
      return false;
    }
  *x = parsedX;
  *y = parsedY;
  return true;
}

/* `Y1:Y2`, as `--slice` spells a range of layers. Both ends are included. */
bool
parseSlice (const QString &text, int *low, int *high, QString *error)
{
  const QStringList parts
      = text.split (QRegularExpression (QStringLiteral ("\\s*:\\s*")));
  bool ok = parts.size () == 2;
  int parsedLow = 0;
  int parsedHigh = 0;
  if (ok)
    {
      parsedLow = parts[0].toInt (&ok);
      if (ok)
        parsedHigh = parts[1].toInt (&ok);
    }
  if (!ok)
    {
      *error = QString (_ ("%1 is not a Y range; write it as Y1:Y2")).arg (text);
      return false;
    }
  *low = parsedLow;
  *high = parsedHigh;
  return true;
}

/* Loads the first region of `file` through the app's own pipeline:
 * `ManageRegionUI` supplies the plugins and `DhLoadJob` decodes the file and
 * hands the region back. The job is asynchronous, so a one-off event loop
 * turns its finished signal into the synchronous answer a command line
 * needs. */
bool
loadFirstRegion (const QString &file, std::shared_ptr<RegionClass> *out,
                 QString *error)
{
  if (!DhCore::instance () || !DhCore::instance ()->handle ())
    {
      *error = _ ("the core could not be opened");
      return false;
    }

  /* The plugin list is built by the region window; building it is what loads
   * the modules. It is never shown. */
  ManageRegionUI::instance ();

  /* For a multi-region file the loader asks which regions to take unless every
   * one is wanted. A command line has no one to answer that, so it is told to
   * take them all and the first region is the one drawn. The value is changed
   * in memory only. */
  DhCore::instance ()->setSettingValue (
      QStringLiteral ("SelectAllRegionsInLoading"), QStringLiteral ("true"));
  DhCore::instance ()->refresh ();

  const qsizetype before = ManageRegionUI::regionNum ();
  const void *cancel = cancel_flag_new ();
  QString filename = file;
  /* The plugin's own reading options: none, which is what the loader passes a
   * plugin that has nothing configured. */
  std::map<QString, void *> inputConfigs;
  DhLoadJob job (filename, cancel, inputConfigs);
  job.setAutoDelete (false);

  QEventLoop loop;
  QObject::connect (&job, &KJob::result, &loop, &QEventLoop::quit);
  job.start ();
  loop.exec ();

  const QString failure = job.errorText ();
  cancel_flag_destroy (cancel);
  if (!failure.isEmpty ())
    {
      *error = failure;
      return false;
    }
  if (ManageRegionUI::regionNum () <= before)
    {
      *error = _ ("the file held no region that could be read");
      return false;
    }
  *out = ManageRegionUI::getRegion (before);
  return *out != nullptr;
}

/* The version a data version names, read back from where its client jar
 * belongs: the core resolves the version, this only splits the file name. */
QString
versionForDataVersion (DhlrcCore *core, uint32_t dataVersion)
{
  char *path
      = dhlrc_core_client_jar_path (core, static_cast<int32_t> (dataVersion));
  if (!path)
    return QString ();
  const QString id
      = QFileInfo (QString::fromUtf8 (path)).completeBaseName ();
  dhlrc_string_free (path);
  return id;
}

/* Where the picture's resources are: the cache directory, the version whose
 * client jar is used, and the jar itself (explicit, or the cached one). */
struct ResourcePaths
{
  QString cacheDir;
  QString version;
  QString jar;
};

bool
resolveResources (DhlrcCore *core, uint32_t dataVersion,
                  const DhPreviewOptions &options, ResourcePaths *paths,
                  QString *error)
{
  paths->cacheDir = options.resources.isEmpty () ? dh::cacheDirectory ()
                                                 : options.resources;
  paths->version = options.version.isEmpty ()
                       ? versionForDataVersion (core, dataVersion)
                       : options.version;
  if (paths->version.isEmpty ())
    {
      *error = _ ("the region's data version names no known version; pass "
                  "--version");
      return false;
    }

  if (options.jar.isEmpty ())
    {
      paths->jar = QDir (paths->cacheDir)
                       .filePath (QStringLiteral ("client_jar/%1.jar")
                                      .arg (paths->version));
      return true;
    }

  /* A `ZipSource` that will not open is skipped silently, so the path is
   * checked here where the message can say which one it was. */
  if (!QFileInfo (options.jar).isFile ())
    {
      *error = QString (_ ("%1 is not a file")).arg (options.jar);
      return false;
    }
  QFile file (options.jar);
  const bool zip
      = file.open (QIODevice::ReadOnly) && file.read (2) == QByteArray ("PK");
  if (!zip)
    {
      *error = QString (_ ("%1 is not a zip archive")).arg (options.jar);
      return false;
    }
  paths->jar = options.jar;
  return true;
}

/* A version's block models and textures are only in its client jar. Without
 * `--jar` one that is missing may be fetched, exactly as the 3D window does
 * when the setting allows it; with `--jar`, or with a cache directory named
 * outright, nothing is fetched. */
void
ensureClientJar (DhlrcCore *core, uint32_t dataVersion,
                 const DhPreviewOptions &options, const ResourcePaths &paths)
{
  if (!options.jar.isEmpty () || QFileInfo (paths.jar).isFile ())
    return;

  const QString where
      = QString (_ ("the client jar of %1 is not in %2")).arg (paths.version,
                                                               paths.cacheDir);
  if (!options.resources.isEmpty ())
    {
      report (QString (_ ("%1; pass --jar or point --resources at one"))
                  .arg (where));
      return;
    }
  const bool allowed
      = DhCore::instance ()->settingValue (QStringLiteral ("DownloadClientJar"))
        != QLatin1String ("false");
  if (!allowed)
    {
      report (QString (_ ("%1; pass --jar, or let it be downloaded (the "
                          "DownloadClientJar setting)"))
                  .arg (where));
      return;
    }

  report (QString (_ ("downloading the client jar of %1 (about 30 MB)"))
              .arg (paths.version));
  char *failure = nullptr;
  const int32_t result = dhlrc_core_download_client_jar (
      core, static_cast<int32_t> (dataVersion), &downloadInfo, nullptr, nullptr,
      nullptr, &failure);
  if (result != 0)
    {
      report (QString (_ ("the client jar could not be downloaded: %1"))
                  .arg (failure ? QString::fromUtf8 (failure)
                                : QString (_ ("unknown error"))));
      if (failure)
        dhlrc_string_free (failure);
      return;
    }
  if (failure)
    dhlrc_string_free (failure);
  report (QString (_ ("downloaded %1")).arg (paths.jar));
}

bool
applyResources (DhlrcCore *core, const ResourcePaths &paths, QString *error)
{
  const QByteArray cacheDir = paths.cacheDir.toUtf8 ();
  const QByteArray version = paths.version.toUtf8 ();
  /* The jar has to outlive the call, so it lives here rather than in a
   * temporary expression. */
  const QByteArray jar
      = paths.jar.isEmpty () ? QByteArray () : paths.jar.toUtf8 ();
  std::vector<const char *> packs;
  if (!jar.isEmpty ())
    packs.push_back (jar.constData ());
  if (dhlrc_core_resources_configure (
          core, cacheDir.constData (), version.constData (),
          packs.empty () ? nullptr : packs.data (), packs.size ())
      != 0)
    {
      *error = _ ("the resource set could not be configured");
      return false;
    }
  return true;
}

bool
parseDegrees (const QString &text, const QString &what, double *out,
              QString *error)
{
  if (text.isEmpty ())
    return true;
  bool ok = false;
  const double value = text.toDouble (&ok);
  if (!ok)
    {
      *error = QString (_ ("%1 is not a number for %2")).arg (text, what);
      return false;
    }
  *out = value;
  return true;
}

/* The picture the software rasteriser makes, over the same background the GL
 * clear uses so the two are comparable. */
int
renderCpu (DhlrcScene *scene, const DhlrcCamera &camera, int width, int height,
           const QString &png)
{
  const uint8_t *pixels = dhlrc_render_image (scene, &camera, width, height);
  if (!pixels)
    return fail (_ ("the CPU renderer returned no picture"));

  const QImage frame (pixels, width, height, width * 4,
                      QImage::Format_RGBA8888);
  QImage picture (width, height, QImage::Format_RGB32);
  picture.fill (BACKGROUND);
  QPainter painter (&picture);
  painter.drawImage (0, 0, frame);
  painter.end ();
  if (!picture.save (png, "PNG"))
    return fail (QString (_ ("%1 could not be written")).arg (png));

  QElapsedTimer timer;
  timer.start ();
  for (int index = 0; index < CPU_FRAMES; ++index)
    dhlrc_render_image (scene, &camera, width, height);
  const qint64 elapsed = timer.elapsed ();
  report (QString (_ ("cpu: %1 frames in %2 ms (%3 ms/frame)"))
              .arg (CPU_FRAMES)
              .arg (elapsed)
              .arg (static_cast<double> (elapsed) / CPU_FRAMES, 0, 'f', 1));
  return 0;
}

/* The same picture from the GL widget: it is shown on a virtual display and its
 * framebuffer is read back, which is the only way a headless run can see what a
 * GPU path drew.
 *
 * `highlight` is the box `--hover` set and `bounds` the box `--bounds` set, or
 * NULL; with one, the frame is taken, the box is taken out and put back through
 * `highlightChanged ()` alone, and the frame has to change and then come back
 * exactly. That is the path a real hover or bounds toggle takes — the overlay's
 * own small buffer is re-read, never the scene's. */
int
renderGpu (DhlrcScene *scene, const DhlrcCamera &camera, int width, int height,
           const QString &png, const float *highlight, const float *bounds)
{
  if (!DhGpuWidget::available ())
    return fail (_ ("this machine cannot give a desktop OpenGL 3.3 context"));

  DhGpuWidget widget;
  /* `--size` is in pixels, and a `QOpenGLWidget` draws into a framebuffer its
   * logical size times the screen's scale, so the widget is made the size that
   * draws the pixels that were asked for. The screen is known before the widget
   * is shown, and a resize afterwards is not always honoured by the bare X
   * server a headless check runs on. */
  const qreal ratio = widget.screen () ? widget.screen ()->devicePixelRatio () : 1.0;
  const qreal safe = ratio < 0.01 ? 1.0 : ratio;
  widget.resize (qMax (1, qRound (width / safe)),
                 qMax (1, qRound (height / safe)));
  widget.setScene (scene);
  widget.setCamera (camera);
  widget.show ();

  /* The widget has to be mapped before it can hold a context, so the event loop
   * is pumped and a frame grabbed until the draw has actually happened. */
  QImage picture;
  QElapsedTimer first;
  first.start ();
  bool drawn = false;
  for (int attempt = 0; attempt < 500; ++attempt)
    {
      QCoreApplication::processEvents (QEventLoop::AllEvents);
      picture = widget.grabFramebuffer ();
      if (widget.drew ())
        {
          drawn = true;
          break;
        }
      QThread::msleep (5);
    }
  if (!drawn)
    return fail (QString (_ ("the OpenGL frame could not be drawn: %1"))
                     .arg (widget.problem ()));
  if (picture.isNull ())
    return fail (_ ("the OpenGL frame came back empty"));
  if (picture.size () != QSize (width, height))
    report (QString (_ ("the OpenGL framebuffer is %1x%2, not %3x%4 (the "
                        "screen scale is %5)"))
                .arg (picture.width ())
                .arg (picture.height ())
                .arg (width)
                .arg (height)
                .arg (ratio));
  if (!picture.save (png, "PNG"))
    return fail (QString (_ ("%1 could not be written")).arg (png));
  report (QString (_ ("gpu: first frame in %1 ms")).arg (first.elapsed ()));

  QElapsedTimer timer;
  timer.start ();
  for (int index = 0; index < GPU_FRAMES; ++index)
    widget.grabFramebuffer ();
  const qint64 elapsed = timer.elapsed ();
  report (QString (_ ("gpu: %1 frames in %2 ms (%3 ms/frame)"))
              .arg (GPU_FRAMES)
              .arg (elapsed)
              .arg (static_cast<double> (elapsed) / GPU_FRAMES, 0, 'f', 1));

  /* Both kinds of box are the scene's overlay, so the frame is taken once and
   * each box is then taken out and put back through `highlightChanged ()`
   * alone; the scene's own buffers must not be rebuilt for either.
   *
   * `grabFramebuffer ()` renders by itself, so no event loop is pumped here: on
   * this bare X server more event processing can resize the window under the
   * check and make the two frames incomparable. */
  const QImage baseline = widget.grabFramebuffer ();

  if (highlight != nullptr)
    {
      dhlrc_render_scene_set_highlight (scene, nullptr);
      widget.highlightChanged ();
      const QImage without = widget.grabFramebuffer ();
      if (without == baseline)
        return fail (_ ("hover: clearing the highlight did not change the "
                        "OpenGL picture"));

      dhlrc_render_scene_set_highlight (scene, const_cast<float *> (highlight));
      widget.highlightChanged ();
      const QImage again = widget.grabFramebuffer ();
      if (again != baseline)
        {
          QImage d1 = baseline.convertToFormat (QImage::Format_RGB32);
          QImage d2 = again.convertToFormat (QImage::Format_RGB32);
          quint64 changed = 0;
          for (int y = 0; y < d1.height (); ++y)
            {
              for (int x = 0; x < d1.width (); ++x)
                {
                  if (d1.pixel (x, y) != d2.pixel (x, y))
                    ++changed;
                }
            }
          return fail (
              QString (_ ("hover: setting the highlight again did not put the "
                          "OpenGL picture back (%1 pixels differ)"))
                  .arg (changed));
        }
      report (_ ("hover: the OpenGL outline follows highlightChanged () alone"));
    }

  if (bounds != nullptr)
    {
      dhlrc_render_scene_set_bounds (scene, nullptr);
      widget.highlightChanged ();
      const QImage without = widget.grabFramebuffer ();
      if (without == baseline)
        return fail (_ ("bounds: clearing the region box did not change the "
                        "OpenGL picture"));

      dhlrc_render_scene_set_bounds (scene, const_cast<float *> (bounds));
      widget.highlightChanged ();
      const QImage again = widget.grabFramebuffer ();
      if (again != baseline)
        {
          QImage d1 = baseline.convertToFormat (QImage::Format_RGB32);
          QImage d2 = again.convertToFormat (QImage::Format_RGB32);
          quint64 changed = 0;
          for (int y = 0; y < d1.height (); ++y)
            {
              for (int x = 0; x < d1.width (); ++x)
                {
                  if (d1.pixel (x, y) != d2.pixel (x, y))
                    ++changed;
                }
            }
          return fail (
              QString (_ ("bounds: setting the region box again did not put the "
                          "OpenGL picture back (%1 pixels differ)"))
                  .arg (changed));
        }
      report (_ ("bounds: the OpenGL region box follows highlightChanged () "
                 "alone"));
    }
  return 0;
}
} // namespace

int
dhRunPreview (const DhPreviewOptions &options)
{
  if (options.file.isEmpty ())
    return fail (_ ("--preview needs the region file to draw"));
  if (options.png.isEmpty ())
    return fail (_ ("--preview needs --png to say where the picture goes"));
  if (!QFileInfo (options.file).isFile ())
    return fail (QString (_ ("%1 is not a file")).arg (options.file));

  int width = 0;
  int height = 0;
  QString error;
  if (!parseSize (options.size, &width, &height, &error))
    return fail (error);

  /* `--slice` limits the picture to one range of Y; both ends are included. */
  int sliceLow = 0;
  int sliceHigh = 0;
  const bool sliced = !options.slice.isEmpty ();
  if (sliced && !parseSlice (options.slice, &sliceLow, &sliceHigh, &error))
    return fail (error);

  /* `cpu` and `gpu` are the two named backends; without one the GPU is used
   * when this machine can give it, and the CPU picture otherwise. */
  bool useGpu = false;
  if (options.backend.isEmpty ())
    useGpu = DhGpuWidget::available ();
  else if (options.backend == QLatin1String ("gpu"))
    useGpu = true;
  else if (options.backend == QLatin1String ("cpu"))
    useGpu = false;
  else
    return fail (QString (_ ("%1 is not a backend; use cpu or gpu"))
                     .arg (options.backend));

  std::shared_ptr<RegionClass> regionClass;
  if (!loadFirstRegion (options.file, &regionClass, &error))
    return fail (error);

  DhlrcCore *core = DhCore::instance ()->handle ();
  DhlrcScene *scene = nullptr;
  uint32_t dataVersion = 0;
  /* The region's size in blocks, which the bounds box wraps and the slice is
   * clamped to. */
  int32_t regionSize[3] = { 0, 0, 0 };
  {
    /* Everything the scene needs is copied out of the region while it is
     * locked, exactly as the 3D window builds one. */
    AutoLocker locker (*regionClass);
    void *region = regionClass->get_region ();
    dataVersion = regionClass->dataVersion ();
    regionSize[0] = region_get_x (region);
    regionSize[1] = region_get_y (region);
    regionSize[2] = region_get_z (region);

    ResourcePaths paths;
    if (!resolveResources (core, dataVersion, options, &paths, &error))
      return fail (error);
    ensureClientJar (core, dataVersion, options, paths);
    if (!applyResources (core, paths, &error))
      return fail (error);

    /* A slice is a scene of its own: the core leaves the other layers out and
     * frames the view around the ones that stay. */
    if (sliced)
      scene = dhlrc_render_scene_new_slice (core, region, sliceLow, sliceHigh);
    else
      scene = dhlrc_render_scene_new (core, region);
  }
  if (!scene)
    return fail (_ ("the scene could not be built"));

  const quint64 quads
      = static_cast<quint64> (dhlrc_render_scene_quads (scene));
  report (QString (_ ("%1: %2 quads, %3 palette entries not drawn"))
              .arg (QFileInfo (options.file).fileName ())
              .arg (quads)
              .arg (static_cast<quint64> (dhlrc_render_scene_skipped (scene))));
  if (quads == 0)
    {
      /* Nothing at all is different from nothing in this range: a slice above
       * the last layer just draws no block. */
      if (sliced)
        report (_ ("the slice draws no block"));
      else
        report (_ ("nothing could be drawn: the block models and textures of "
                   "this region's version were not found"));
    }

  /* The camera the 3D view opens with, framed against the picture's own
   * size; both backends are given this very struct. */
  DhlrcCamera camera{};
  camera.yaw = 45.0;
  camera.pitch = 30.0;
  camera.distance = 16.0;
  camera.fov = 45.0;
  camera.orthographic = 0;
  if (!parseDegrees (options.yaw, QStringLiteral ("--yaw"), &camera.yaw, &error)
      || !parseDegrees (options.pitch, QStringLiteral ("--pitch"),
                        &camera.pitch, &error))
    {
      dhlrc_render_scene_free (scene);
      return fail (error);
    }
  dhlrc_render_fit (scene, &camera, static_cast<uint32_t> (width),
                    static_cast<uint32_t> (height));

  /* `--hover` points at one picture pixel before the draw, so the outline is
   * part of the picture; the text the tooltip would show goes to stderr, which
   * is the only way a headless run can read it. */
  const float *hoverBox = nullptr;
  float box[6] = { 0 };
  if (!options.hover.isEmpty ())
    {
      int hoverX = 0;
      int hoverY = 0;
      if (!parseHover (options.hover, &hoverX, &hoverY, &error))
        {
          dhlrc_render_scene_free (scene);
          return fail (error);
        }

      int32_t block[3] = { 0, 0, 0 };
      QElapsedTimer pick;
      pick.start ();
      const bool hit
          = dhlrc_render_pick (scene, &camera, static_cast<uint32_t> (width),
                               static_cast<uint32_t> (height),
                               static_cast<double> (hoverX),
                               static_cast<double> (hoverY), block)
            != 0;
      report (QString (_ ("hover: the pick took %1 ms"))
                  .arg (static_cast<double> (pick.nsecsElapsed ()) / 1.0e6, 0,
                        'f', 3));
      if (!hit)
        {
          report (QString (_ ("hover: pixel %1,%2 is the background; nothing "
                              "is highlighted"))
                      .arg (hoverX)
                      .arg (hoverY));
        }
      else
        {
          /* The box the block is drawn in — a slab gives half a cell — and the
           * whole cell as the answer for a block that drew nothing. */
          box[0] = static_cast<float> (block[0]);
          box[1] = static_cast<float> (block[1]);
          box[2] = static_cast<float> (block[2]);
          box[3] = static_cast<float> (block[0] + 1);
          box[4] = static_cast<float> (block[1] + 1);
          box[5] = static_cast<float> (block[2] + 1);
          dhlrc_render_scene_block_box (scene, block[0], block[1], block[2], box);
          hoverBox = box;
          dhlrc_render_scene_set_highlight (scene, box);
          report (
              QString (_ ("hover: pixel %1,%2 picks block (%3, %4, %5); the "
                          "outline is %6 quads"))
                  .arg (hoverX)
                  .arg (hoverY)
                  .arg (block[0])
                  .arg (block[1])
                  .arg (block[2])
                  .arg (static_cast<quint64> (
                      dhlrc_render_scene_overlay_count (scene))));

          QString info;
          {
            AutoLocker locker (*regionClass);
            const qint32 index
                = regionClass->index (block[0], block[1], block[2]);
            if (index >= 0)
              info = BlockReaderUI::getBlockInfo (
                  regionClass->get_region (), static_cast<quint32> (index),
                  false);
          }
          report (_ ("hover: the tooltip would say:"));
          if (info.isEmpty ())
            report (QStringLiteral ("hover| (no info text for this block)"));
          else
            {
              /* One line each, prefixed, so the block of text is still easy to
               * pick out of a log. */
              const QStringList lines = info.split (QLatin1Char ('\n'));
              for (const QString &line : lines)
                {
                  if (!line.isEmpty ())
                    report (QStringLiteral ("hover| ") + line);
                }
            }
        }
    }

  /* `--bounds` shows the box around the region being drawn: the whole region,
   * or its Y clamped to the slice. The box is the scene's overlay, drawn in its
   * own colour, so it is set after the hover box — that leaves the overlay
   * count in the hover's report meaning what it always did. */
  float boundsBox[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
  const float *bounds = nullptr;
  if (options.bounds)
    {
      int low = 0;
      int high = regionSize[1];
      bool shown = regionSize[0] > 0 && regionSize[1] > 0 && regionSize[2] > 0;
      if (sliced)
        {
          /* A layer occupies the cell between its Y and the next one, and a
           * low above the high draws nothing, so there is nothing to box. */
          if (sliceLow > sliceHigh)
            shown = false;
          else
            {
              low = std::max (0, sliceLow);
              high = std::min (regionSize[1], sliceHigh + 1);
              shown = low < high;
            }
        }
      if (!shown)
        {
          report (_ ("bounds: this slice draws no layer, so there is no box to "
                     "show"));
        }
      else
        {
          boundsBox[0] = 0.0f;
          boundsBox[1] = static_cast<float> (low);
          boundsBox[2] = 0.0f;
          boundsBox[3] = static_cast<float> (regionSize[0]);
          boundsBox[4] = static_cast<float> (high);
          boundsBox[5] = static_cast<float> (regionSize[2]);
          bounds = boundsBox;
          dhlrc_render_scene_set_bounds (scene, boundsBox);
          report (QString (_ ("bounds: the box is (%1, %2, %3)..(%4, %5, %6), "
                              "%7 overlay quads in all"))
                      .arg (boundsBox[0])
                      .arg (boundsBox[1])
                      .arg (boundsBox[2])
                      .arg (boundsBox[3])
                      .arg (boundsBox[4])
                      .arg (boundsBox[5])
                      .arg (static_cast<quint64> (
                          dhlrc_render_scene_overlay_count (scene))));
        }
    }

  report (QString (_ ("drawing with %1")).arg (
      useGpu ? QString (_ ("OpenGL")) : QString (_ ("the CPU"))));

  const int result = useGpu ? renderGpu (scene, camera, width, height,
                                         options.png, hoverBox, bounds)
                            : renderCpu (scene, camera, width, height,
                                         options.png);
  dhlrc_render_scene_free (scene);
  return result;
}
