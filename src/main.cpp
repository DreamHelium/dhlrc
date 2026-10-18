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
#include <QLibraryInfo>
#include <QLocale>
#include <QStandardPaths>
#include <QTranslator>
#include <QWidget>
#include <qcoreapplication.h>

namespace
{
/* The language directories to try, most specific first: KDE ships its
 * catalogues for `zh_CN` while Qt's locale names may carry the script
 * (`zh_Hans_CN`), so both forms are offered. */
QStringList
translationLanguages ()
{
  const QLocale locale = QLocale::system ();
  QStringList languages{ locale.name () };
  for (const QString &ui : locale.uiLanguages ())
    languages << QString (ui).replace (QLatin1Char ('-'), QLatin1Char ('_'));
  languages << locale.name ().section (QLatin1Char ('_'), 0, 0);
  languages.removeAll (QString ());
  languages.removeDuplicates ();
  return languages;
}

/* Root directories holding KDE-style catalogues, i.e. the ones whose children
 * are language directories: <root>/<language>/LC_MESSAGES/<domain>.mo. An
 * MSYS2 installation keeps them under bin/data because KF6 is built for the
 * Windows layout; a package carries the same tree beside the executable. */
QStringList
kdeLocaleRoots ()
{
  const QString appDir = QCoreApplication::applicationDirPath ();
  const QString prefix = QLibraryInfo::path (QLibraryInfo::PrefixPath);
  QStringList roots;
  roots << appDir + QStringLiteral ("/data/locale");
  roots << prefix + QStringLiteral ("/bin/data/locale");
  roots << prefix + QStringLiteral ("/share/locale");
  roots += QStandardPaths::locateAll (QStandardPaths::GenericDataLocation,
                                     QStringLiteral ("locale"),
                                     QStandardPaths::LocateDirectory);
  roots.removeDuplicates ();
  return roots;
}

/* Qt's own catalogue: the standard dialog buttons ("OK", "Restore Defaults"),
 * the command-line help and the like come from here, not from the program's
 * catalogue or from KF6. Qt never loads it by itself, so it is loaded here. */
void
installQtTranslations ()
{
  QStringList dirs;
  /* A package keeps it next to the executable; a development build finds it
   * in the Qt installation QLibraryInfo knows about. */
  dirs << QCoreApplication::applicationDirPath ()
              + QStringLiteral ("/translations");
  const QString installed
      = QLibraryInfo::path (QLibraryInfo::TranslationsPath);
  if (!installed.isEmpty ())
    dirs << installed;

  for (const QString &dir : dirs)
    {
      if (!QDir (dir).exists ())
        continue;
      bool loaded = false;
      /* `qtbase` holds the widgets' strings; `qt` is the older combined
       * catalogue some distributions still ship. */
      for (const auto &catalog :
           { QStringLiteral ("qtbase"), QStringLiteral ("qt") })
        {
          auto *translator = new QTranslator (qApp);
          if (translator->load (QLocale::system (), catalog,
                                QStringLiteral ("_"), dir))
            {
              QCoreApplication::installTranslator (translator);
              loaded = true;
            }
          else
            {
              delete translator;
            }
        }
      if (loaded)
        break;
    }
}

/* KF6's own catalogues: KDE ships them as gettext `.mo` files (which ki18n
 * reads) plus `*_qt.qm` counterparts for strings the frameworks translate
 * through Qt's `tr()`. Registering the directory with ki18n is what makes the
 * gettext side work in a development tree, where QStandardPaths does not see
 * the MSYS2 `bin/data` directory. */
void
installKdeTranslations ()
{
  const QStringList languages = translationLanguages ();
  for (const QString &root : kdeLocaleRoots ())
    {
      for (const QString &language : languages)
        {
          const QDir messages (root + QLatin1Char ('/') + language
                               + QStringLiteral ("/LC_MESSAGES"));
          if (!messages.exists ())
            continue;
          for (const QString &name :
               messages.entryList ({ QStringLiteral ("*.mo") },
                                   QDir::Files))
            KLocalizedString::addDomainLocaleDir (
                name.section (QLatin1Char ('.'), 0, 0).toUtf8 (), root);
          for (const QString &name :
               messages.entryList ({ QStringLiteral ("*_qt.qm") },
                                   QDir::Files))
            {
              auto *translator = new QTranslator (qApp);
              if (translator->load (messages.absoluteFilePath (name)))
                QCoreApplication::installTranslator (translator);
              else
                delete translator;
            }
          return;
        }
    }
}
} // namespace

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

  /* The program's own catalogue covers only its own strings. The strings it
   * gets from Qt (the standard dialog buttons first of all) and from KF6 (the
   * colour-scheme menu title, for instance) live in their own catalogues,
   * which nothing else loads on Windows. */
  installQtTranslations ();
  installKdeTranslations ();

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
  QCommandLineOption sliceOption (
      QStringLiteral ("slice"),
      i18n ("Preview: draw only the layers from Y1 to Y2, both included"),
      QStringLiteral ("Y1:Y2"));
  QCommandLineOption boundsOption (
      QStringLiteral ("bounds"),
      i18n ("Preview: draw the box around the region being drawn, clamped to "
            "the slice when one is set"));
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
  parser.addOption (sliceOption);
  parser.addOption (boundsOption);
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
      options.slice = parser.value (sliceOption);
      options.bounds = parser.isSet (boundsOption);
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
