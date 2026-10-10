#include "dhcore.h"
#include "mainwindow.h"
#include "utility.h"
#include "version.h"
#include <KIconTheme>
#include <KLocalizedString>
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QTranslator>
#include <QWidget>
#include <cstdio>
#include <qcoreapplication.h>

int
main (int argc, char *argv[])
{
  KIconTheme::initTheme ();
  QApplication a (argc, argv);

#ifdef Q_OS_WIN
  setlocale (LC_ALL, ".UTF-8");
#else
  setlocale (LC_ALL, "");
#endif

  auto dir = dh::getTranslationDir ();

  bindtextdomain ("dhlrc", dir.toUtf8 ().constData ());
  bind_textdomain_codeset ("dhlrc", "UTF-8");
  textdomain ("dhlrc");
  KLocalizedString::setApplicationDomain ("dhlrc");
  KLocalizedString::addDomainLocaleDir ("dhlrc", dir);
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
  parser.process (a);

  QApplication::setStyle ("breeze");

  /* Opens the configuration and starts watching it; the notifications it
   * produces are shown once the window exists. */
  DhCore::init (&a);

  MainWindow w;
  w.show ();

  return a.exec ();
}
