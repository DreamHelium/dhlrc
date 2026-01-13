#include "region.h"
#include <KLocalizedString>
#include <QApplication>
#include <QDir>
#include <QLibrary>
#include <QTranslator>
#include <QWidget>
#include <qcoreapplication.h>

int
main (int argc, char *argv[])
{
  QApplication a (argc, argv);
  QApplication::setApplicationName ("dhlrc");
  QApplication::setApplicationDisplayName (("Litematica Reader"));

  int failed = false;
  auto ret = file_try_uncompress (
      "/home/dream_he/.minecraft/schematics/Station/"
      "Station2.3.1-generic-N.schem",
      // "/home/dream_he/1.tar.gz",
      [] (void *p, int v, const char *string, const char *text)
        {
          if (text)
            {
              QString msgStr ("%d, ");
              msgStr.append (string);
              msgStr.append ('\n');
              printf (msgStr.toUtf8 ().constData (), v, text);
            }
          else
            printf ("%d, %s\n", v, string);
        },
      nullptr, &failed);
  if (failed)
    {
      auto msg = vec_to_cstr (ret);
      printf ("%s\n", msg ? msg : "NULL");
      string_free (msg);
    }
  else
    vec_free (ret);

  /*
  QString module_path
      = QCoreApplication::applicationDirPath () + "/region_module/";
  QDir dir (module_path);
  auto list = dir.entryList ();
  for (auto &i : list)
    {
      typedef void (*ProgressFunc) (void *, int, const char *, const char *);
      typedef const char *(*LoadFunc) (const char *, ProgressFunc, void *);
      QLibrary loader (module_path + i);
      auto func = reinterpret_cast<LoadFunc> (
          loader.resolve ("region_create_from_file"));
      if (func)
        {
          auto str = func (
              // "/home/dream_he/.minecraft/schematics/Station/"
              // "Station2.3.1-generic-N.schem",
              "/home/dream_he/1.tar.gz",
              [] (void *p, int v, const char *string, const char *text)
                {
                  if (text)
                    {
                      QString msgStr("%d, ");
                      msgStr.append (string);
                      msgStr.append ('\n');
                      printf(msgStr.toUtf8().constData(), v, text);
                    }
                  else
                    printf ("%d, %s\n", v, string);
                },
              // nullptr,
              nullptr);
          if (str)
            printf ("%s\n", str);
          string_free (str);
        }
    }
    */

  // KLocalizedString::setApplicationDomain ("dhlrc");
  // char *trdir = get_translation_filedir (prname);
  // KLocalizedString::addDomainLocaleDir ("dhlrc", trdir);
  // g_free (trdir);
  //
  // auto pixmap = dh::loadSvgResourceFile ("/cn/dh/dhlrc/dhlrc.svg");
  // QApplication::setWindowIcon (QIcon (*pixmap));
  // delete pixmap;

  // void *region = region_new ();
  // auto set_name_msg = region_set_name (region, nullptr);
  // printf ("%s\n", set_name_msg);
  // string_free (set_name_msg);
  // printf ("Original timestamp: %ld\n", region_get_create_timestamp
  // (region)); auto msg = region_set_time (region, 0, 0); printf ("%s\n", msg
  // ? msg : "NULL"); printf ("New timestamp: %ld\n",
  // region_get_create_timestamp (region)); string_free (msg); region_free
  // (region);

  QWidget widget;
  widget.show ();
  // MainWindow w;
  // w.show ();

  return a.exec ();
}