#include "dhpreview.h"

#include "dhcore.h"
#include "dhgpuwidget.h"
#include "dhloadjob.h"
#include "manageregionui.h"
#include "region.h"
#include "regionclass.h"

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
#include <QStandardPaths>
#include <QStringList>
#include <QThread>
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

/* The cache directory the configuration names, or the platform default the
 * core itself would resolve to. */
QString
configuredCacheDir ()
{
  const QString configured
      = DhCore::instance () ? DhCore::instance ()->config ().cacheDirectory
                            : QString ();
  if (!configured.isEmpty ())
    return configured;
  return QStandardPaths::writableLocation (QStandardPaths::CacheLocation);
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
  paths->cacheDir = options.resources.isEmpty () ? configuredCacheDir ()
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
 * GPU path drew. */
int
renderGpu (DhlrcScene *scene, const DhlrcCamera &camera, int width, int height,
           const QString &png)
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
  {
    /* Everything the scene needs is copied out of the region while it is
     * locked, exactly as the 3D window builds one. */
    AutoLocker locker (*regionClass);
    dataVersion = regionClass->dataVersion ();

    ResourcePaths paths;
    if (!resolveResources (core, dataVersion, options, &paths, &error))
      return fail (error);
    ensureClientJar (core, dataVersion, options, paths);
    if (!applyResources (core, paths, &error))
      return fail (error);

    scene = dhlrc_render_scene_new (core, regionClass->get_region ());
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
    report (_ ("nothing could be drawn: the block models and textures of this "
               "region's version were not found"));

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

  report (QString (_ ("drawing with %1")).arg (
      useGpu ? QString (_ ("OpenGL")) : QString (_ ("the CPU"))));
  const int result = useGpu ? renderGpu (scene, camera, width, height,
                                         options.png)
                            : renderCpu (scene, camera, width, height,
                                         options.png);
  dhlrc_render_scene_free (scene);
  return result;
}
