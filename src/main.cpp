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
  // char *trdir = get_translation_filedir (prname);
  KLocalizedString::addDomainLocaleDir ("dhlrc", dir);
  // g_free (trdir);
  //
  // auto pixmap = dh::loadSvgResourceFile ("/cn/dh/dhlrc/dhlrc.svg");
  // QApplication::setWindowIcon (QIcon (*pixmap));
  // delete pixmap;

  // QWidget widget;
  // widget.show ();
  MainWindow w;
  w.show ();

  return a.exec ();
}