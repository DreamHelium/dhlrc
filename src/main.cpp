#include "region.h"
#include <KLocalizedString>
#include <QApplication>
#include <QDir>
#include <QLibrary>
#include <QTranslator>
#include <QWidget>
#include "mainwindow.h"
#include <qcoreapplication.h>

int
main (int argc, char *argv[])
{
  QApplication a (argc, argv);
  QApplication::setApplicationName ("dhlrc");
  QApplication::setApplicationDisplayName (("Litematica Reader"));

#ifdef Q_OS_WIN
  setlocale(LC_ALL, ".UTF-8");
#else
  setlocale (LC_ALL, "");
#endif

  auto dir = QApplication::applicationDirPath ();
  dir += QDir::toNativeSeparators ("/");
  dir += "locale";

  bindtextdomain ("dhlrc", dir.toUtf8 ().constData ());
  bind_textdomain_codeset ("dhlrc", "UTF-8");
  textdomain ("dhlrc");

  KLocalizedString::setApplicationDomain ("dhlrc");
  KLocalizedString::addDomainLocaleDir ("dhlrc", dir);

  QApplication::setWindowIcon (QIcon (":/cn/dh/dhlrc/dhlrc.svg"));

  MainWindow w;
  w.show ();

  return a.exec ();
}