#include "dhcore.h"
#ifdef DH_RENDER_CHECK
#include "dhpreview.h"
#endif
#include "mainwindow.h"
#include "utility.h"
#include "version.h"
#include "windowsappid.h"
#include <KIconTheme>
#include <KLocalizedString>
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QTranslator>
#include <QWidget>
#include <qcoreapplication.h>

int
main (int argc, char *argv[])
{
  /* Windows names a toast after the process's AppUserModelID; set it — and the
   * name/icon Windows shows — before any window exists, so the notifications are
   * from dhlrc rather than from PowerShell. */
  applyWindowsAppUserModelID ();

  KIconTheme::initTheme ();
  QApplication a (argc, argv);

#ifdef Q_OS_WIN
  setlocale (LC_ALL, ".UTF-8");
#else
  setlocale (LC_ALL, "");
#endif

  auto dir = dh::getTranslationDir ();

  /* Both catalogues are bound here; which one is used is decided below, once
   * the configuration says whether the cute pack is on. The plain one is the
   * provisional choice, so anything said before that point is normal. */
  const QByteArray localeDir = dir.toUtf8 ();
  bindtextdomain ("dhlrc", localeDir.constData ());
  bind_textdomain_codeset ("dhlrc", "UTF-8");
  bindtextdomain ("dhlrc-cute", localeDir.constData ());
  bind_textdomain_codeset ("dhlrc-cute", "UTF-8");
  textdomain ("dhlrc");
  KLocalizedString::setApplicationDomain ("dhlrc");
  KLocalizedString::addDomainLocaleDir ("dhlrc", dir);
  KLocalizedString::addDomainLocaleDir ("dhlrc-cute", dir);

  /* The configuration decides which catalogue is in use. It is read when the
   * program starts, so turning the cute pack on takes a restart — hence the
   * setting says so. This has to happen before the window and the command-line
   * help are built, because they are the first things that speak. */
  DhCore::init (&a);
  if (DhCore::instance ()
      && DhCore::instance ()->settingValue (QStringLiteral ("CuteLanguagePack"))
             == QLatin1String ("true"))
    {
      textdomain ("dhlrc-cute");
      KLocalizedString::setApplicationDomain ("dhlrc-cute");
    }

  QApplication::setApplicationName ("dhlrc");
  QApplication::setApplicationDisplayName (
      i18n ("Minecraft Structure Modifier"));
  QApplication::setApplicationVersion (dh::versionString ());
  QApplication::setWindowIcon (QIcon (":/cn/dh/dhlrc/dhlrc.svg"));

  /* `addHelpOption ()` / `addVersionOption ()` only take effect through
   * `process ()`, which prints and exits by itself. `parse ()` alone merely
   * records the values and returns, so the options would fall through to the
   * window below instead of being handled. The text it prints comes from the
   * setters above, so the command line and the About window share one source.
   */
  QCommandLineParser parser;
  parser.setApplicationDescription (QApplication::applicationDisplayName ());
  parser.addHelpOption ();
  parser.addVersionOption ();

#ifdef DH_RENDER_CHECK
  /* The headless preview, a development tool rather than something a user
   * needs: it draws one picture of a file and exits, which is how both
   * backends of the 3D view are checked without a display (the same command
   * with `--backend cpu` and `--backend gpu` has to give the same picture).
   * It is only built with `-DDH_BUILD_RENDER_CHECK=ON`. */
  QCommandLineOption previewOption (
      QStringLiteral ("preview"),
      i18n ("Draw one picture of a region file and exit; needs --png"),
      QStringLiteral ("file"));
  QCommandLineOption pngOption (
      QStringLiteral ("png"),
      i18n ("Where the preview writes its PNG picture"),
      QStringLiteral ("out.png"));
  QCommandLineOption backendOption (
      QStringLiteral ("backend"),
      i18n ("Preview backend: cpu or gpu (the default is the GPU when OpenGL "
            "is available)"),
      QStringLiteral ("cpu|gpu"));
  QCommandLineOption sizeOption (
      QStringLiteral ("size"), i18n ("Preview picture size"),
      QStringLiteral ("WxH"), QStringLiteral ("900x640"));
  QCommandLineOption yawOption (QStringLiteral ("yaw"),
                                i18n ("Preview camera yaw in degrees"),
                                QStringLiteral ("D"));
  QCommandLineOption pitchOption (QStringLiteral ("pitch"),
                                  i18n ("Preview camera pitch in degrees"),
                                  QStringLiteral ("D"));
  QCommandLineOption hoverOption (
      QStringLiteral ("hover"),
      i18n ("Preview: highlight the block under this picture pixel (X,Y) and "
            "print the tooltip text"),
      QStringLiteral ("X,Y"));
  QCommandLineOption jarOption (
      QStringLiteral ("jar"),
      i18n (
          "Preview: read the block models and textures from this client jar"),
      QStringLiteral ("path"));
  QCommandLineOption resourcesOption (
      QStringLiteral ("resources"),
      i18n ("Preview: read resources from this cache directory"),
      QStringLiteral ("dir"));
  QCommandLineOption versionOption (
      QStringLiteral ("version"),
      i18n ("Preview: use the client jar of this game version"),
      QStringLiteral ("id"));
  parser.addOption (previewOption);
  parser.addOption (pngOption);
  parser.addOption (backendOption);
  parser.addOption (sizeOption);
  parser.addOption (yawOption);
  parser.addOption (pitchOption);
  parser.addOption (hoverOption);
  parser.addOption (jarOption);
  parser.addOption (resourcesOption);
  parser.addOption (versionOption);
#endif

  parser.process (a);

  QApplication::setStyle ("breeze");

#ifdef DH_RENDER_CHECK
  /* A preview never opens the main window: it loads the file, draws one
   * picture and exits. */
  if (parser.isSet (previewOption))
    {
      DhPreviewOptions options;
      options.file = parser.value (previewOption);
      options.png = parser.value (pngOption);
      options.backend = parser.value (backendOption);
      options.size = parser.value (sizeOption);
      options.yaw = parser.value (yawOption);
      options.pitch = parser.value (pitchOption);
      options.hover = parser.value (hoverOption);
      options.jar = parser.value (jarOption);
      options.resources = parser.value (resourcesOption);
      options.version = parser.value (versionOption);
      return dhRunPreview (options);
    }
#endif

  MainWindow w;
  w.show ();

  return a.exec ();
}
