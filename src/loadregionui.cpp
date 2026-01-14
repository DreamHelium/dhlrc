#include "loadregionui.h"

#include <QApplication>
#include <QDir>
#include <QLibrary>
#include <QMessageBox>
#include <QTimer>
#include <future>
// #include <lrchooseui.h>
#include <QUuid>
#include <manage.h>
#include <region.h>
#define _(str) gettext (str)

enum
{
  DHLRC_LOAD_IS_LITEMATIC,
  DHLRC_LOAD_SUCCESS,
  DHLRC_LOAD_CANCELED,
  DHLRC_LOAD_FAILED
};

LoadRegionUI::LoadRegionUI (QStringList list, dh::ManageRegion *mr,
                            QWidget *parent)
    : LoadObjectUI (parent), mr (mr), list (list)
{
  setLabel (_ ("Loading file(s) to Region(s)."));
  connect (this, &LoadRegionUI::winClose, this,
           [&]
             {
               // if (!finished)
               //   g_cancellable_cancel (cancellable);
               if (!failedList.isEmpty ())
                 {
                   QString errorMsg
                       = _ ("The following files are not added:\n");
                   for (int i = 0; i < failedList.size (); i++)
                     {
                       const QString &dir = failedList.at (i);
                       const QString &reason = failedReason.at (i);
                       errorMsg += dir + ": " + reason + "\n";
                     }
                   QMessageBox::critical (this, _ ("Error!"), errorMsg);
                 }
             });
  connect (this, &LoadRegionUI::continued, this,
           [&]
             {
               // if (!finished)
               //   {
               //     auto lcui = new LrChooseUI (instance, description);
               //     lcui->exec ();
               //     delete lcui;
               //     g_main_loop_quit (main_loop);
               //     stopped = false;
               //   }
             });
  process ();
}

LoadRegionUI::~LoadRegionUI ()
{
  // g_object_unref (cancellable);
  // g_main_loop_unref (main_loop);
}

void
LoadRegionUI::process ()
{
  auto real_task = [&]
    {
      int i = 0;
      // GCancellable *new_cancellable = g_object_ref (cancellable);
      for (const auto &dir : list)
        {
          auto setFunc = [] (void *main_klass, int value, const char *text,
                             const char *arg)
            {
              Q_EMIT static_cast<LoadRegionUI *> (main_klass)
                  ->refreshSubProgress (value);
              if (!arg)
                Q_EMIT static_cast<LoadRegionUI *> (main_klass)
                    ->refreshSubLabel (text);
              else
                {
                  auto msg = QString::asprintf (text, arg);
                  Q_EMIT static_cast<LoadRegionUI *> (main_klass)
                      ->refreshSubLabel (msg);
                }
            };

          // if (g_cancellable_is_cancelled (new_cancellable))
          //   break;
          Q_EMIT refreshFullProgress (i * 100 / list.size ());
          QString realLabel = "[%1/%2] %3";
          realLabel = realLabel.arg (i + 1).arg (list.size ()).arg (dir);
          Q_EMIT refreshFullLabel (realLabel);
          i++;
          /* Processing stuff */
          Q_EMIT refreshSubLabel (_ ("Parsing NBT"));
          int failed = 0;
          void *data
              = file_try_uncompress (dir.toUtf8 (), setFunc, this, &failed);

          // GError *err = nullptr;
          // auto nbt = DhNbtInstance (dir.toUtf8 (), setFunc, this,
          //                           new_cancellable, 0, 100, &err);
          if (failed)
            {
              // g_message ("%d", dhlrc_get_ignore_leftover ());
              // if (err->code == NBT_GLIB_PARSE_ERROR_LEFTOVER_DATA
              //     && dhlrc_get_ignore_leftover ())
              //   {
              //     /* Ignore Leftover data */
              //     g_error_free (err);
              //     goto continue_situation;
              //   }
              failedList.append (dir);
              auto err_msg = vec_to_cstr (data);
              failedReason.append (err_msg);
              string_free (err_msg);
              QString str = _ ("Error encountered with domain %1 "
                               "and code %2: %3");
              // str = str.arg (g_quark_to_string (err->domain))
              //           .arg (err->code)
              //           .arg (err->message);
              // if (!g_cancellable_is_cancelled (new_cancellable))
              Q_EMIT refreshSubLabel (str);
              // else
              //   g_message ("%s", str.toUtf8 ().constData ());
              // g_error_free (err);
            }
          // else
          // continue_situation:
          // if (lite_region_num_instance (&nbt))
          //   {
          //     Q_EMIT refreshFullLabel (
          //         _ ("Please click `Continue` to choose region(s)."));
          //     instance = &nbt;
          //     /* Use the filename's description to fill the description */
          //     char *real_name = g_path_get_basename (dir.toUtf8 ());
          //     char *real_des = dh_get_filename_without_extension
          //     (real_name); g_free (real_name);
          //     /* Fill description */
          //     description = real_des;
          //     /* Emit the stop signal to stop, and use loop to stop the
          //      * process. */
          //     Q_EMIT stopProgress ();
          //     g_main_loop_run (main_loop);
          //     /* Continue */
          //     g_free (description);
          //     description = nullptr;
          //     instance = nullptr;
          //   }
          else
            {
              auto moduleDir = QApplication::applicationDirPath ();
              moduleDir += QDir::toNativeSeparators ("/");
              moduleDir += "region_module";

              auto moduleList = QDir (moduleDir).entryList (QDir::Files);
              for (auto module : moduleList)
                {
                  typedef const char *(*LoadFunc) (void *, ProgressFunc,
                                                   void **, void *);
                  QString realModuleDir
                      = moduleDir + QDir::toNativeSeparators ("/") + module;
                  QLibrary lib (realModuleDir);
                  lib.load ();
                  auto func = reinterpret_cast<LoadFunc> (
                      lib.resolve ("region_create_from_file"));
                  void *region = nullptr;
                  auto msg = func (data, setFunc, &region, this);
                  if (msg)
                    {
                      failedList.append (dir);
                      failedReason.append (msg);
                      string_free (msg);
                    }
                  else
                    {
                      Region regionStruct = {
                        region,
                        QFileInfo (dir).baseName (),
                        QUuid::createUuid ().toString (),
                        QDateTime::currentDateTime (),
                      };
                      mr->appendRegion (regionStruct);
                    }
                }
            }
          // {
          //   char *real_name = g_path_get_basename (dir.toUtf8 ());
          //   char *real_des = dh_get_filename_without_extension (real_name);
          //   g_free (real_name);
          //   dh_info_new_short (DH_TYPE_REGION, region, real_des);
          //   g_free (real_des);
          // }
          // else /* ? */
          // {

          // }
          // }
          // if (!g_cancellable_is_cancelled (new_cancellable))
          // {
          Q_EMIT refreshFullProgress (100);

          // }
          // g_object_unref (new_cancellable);
        }
      finish ();
    };

  std::thread thread (real_task);
  thread.detach ();
}