#include "mainwindow.h"
#include "utility.h"
#include <KLocalizedString>
#include <QApplication>
#include <QDir>
#include <QTranslator>
#include <QWidget>
#include <qcoreapplication.h>

int
main (int argc, char *argv[])
{
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
  QApplication::setWindowIcon (QIcon (":/cn/dh/dhlrc/dhlrc.svg"));

  MainWindow w;
  w.show ();

  return a.exec ();
}