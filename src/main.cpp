#include "region.h"
#include <KLocalizedString>
#include <QApplication>
#include <QTranslator>
#include <QWidget>
#include <qcoreapplication.h>

int
main (int argc, char *argv[])
{
  QApplication a (argc, argv);
  QApplication::setApplicationName ("dhlrc");
  QApplication::setApplicationDisplayName (("Litematica Reader"));

  // KLocalizedString::setApplicationDomain ("dhlrc");
  // char *trdir = get_translation_filedir (prname);
  // KLocalizedString::addDomainLocaleDir ("dhlrc", trdir);
  // g_free (trdir);
  //
  // auto pixmap = dh::loadSvgResourceFile ("/cn/dh/dhlrc/dhlrc.svg");
  // QApplication::setWindowIcon (QIcon (*pixmap));
  // delete pixmap;

  void *region = region_new ();
  printf ("Original timestamp: %ld\n", region_get_create_timestamp (region));
  auto msg = region_set_time (region, INT64_MAX, 0);
  printf ("%s\n", msg);
  printf ("New timestamp: %ld\n", region_get_create_timestamp (region));
  string_free (msg);
  region_free (region);

  QWidget widget;
  widget.show ();
  // MainWindow w;
  // w.show ();

  return a.exec ();
}