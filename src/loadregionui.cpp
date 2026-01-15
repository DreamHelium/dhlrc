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
    : LoadObjectUI (parent), mr (mr), list (list),
      cancel_flag (cancel_flag_new ())
{
  setLabel (_ ("Loading file(s) to Region(s)."));
  connect (this, &LoadRegionUI::winClose, this,
           [&]
             {
               cancel_flag_cancel (cancel_flag);

               if (!failedList.isEmpty () || !finished)
                 {
                   QString errorMsg
                       = _ ("The following files are not added:\n");
                   for (int i = 0; i < failedList.size (); i++)
                     {
                       const QString &dir = failedList.at (i);
                       const QString &reason = failedReason.at (i);
                       errorMsg += dir + ": " + reason + "\n";
                     }
                   if (!finished)
                     errorMsg
                         += _ ("The loading of current file is cancelled.");
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
  connect (this, &LoadRegionUI::finishLoadOne, mr,
           &dh::ManageRegion::refresh_triggered);
  process ();
}

LoadRegionUI::~LoadRegionUI ()
{
  // g_object_unref (cancellable);
  // g_main_loop_unref (main_loop);
  cancel_flag_destroy (cancel_flag);
}

void
LoadRegionUI::process ()
{
  auto real_task = [&]
    {
      int i = 0;
      auto new_cancel_flag = cancel_flag;
      cancel_flag_clone (cancel_flag);
      for (const auto &dir : list)
        {
          auto setFunc = [] (void *main_klass, int value, const char *text,
                             const char *arg)
            {
              Q_EMIT static_cast<LoadRegionUI *> (main_klass)
                  ->refreshSubProgress (value);
              if (!arg)
                Q_EMIT static_cast<LoadRegionUI *> (main_klass)
                    ->refreshSubLabel (gettext (text));
              else
                {
                  auto msg = QString::asprintf (gettext (text), arg);
                  Q_EMIT static_cast<LoadRegionUI *> (main_klass)
                      ->refreshSubLabel (msg);
                }
            };

          if (cancel_flag_is_cancelled (new_cancel_flag))
            break;
          Q_EMIT refreshFullProgress (i * 100 / list.size ());
          QString realLabel = "[%1/%2] %3";
          realLabel = realLabel.arg (i + 1).arg (list.size ()).arg (dir);
          Q_EMIT refreshFullLabel (realLabel);
          i++;
          /* Processing stuff */
          Q_EMIT refreshSubLabel (_ ("Parsing NBT"));
          int failed = 0;
          void *data = file_try_uncompress (dir.toUtf8 (), setFunc, this,
                                            &failed, new_cancel_flag);

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

              if (!cancel_flag_is_cancelled (new_cancel_flag))
                {
                  failedList.append (dir);
                  auto err_msg = vec_to_cstr (data);
                  failedReason.append (err_msg);
                  Q_EMIT refreshSubLabel (err_msg);
                  string_free (err_msg);
                }
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
              auto moduleNum = mr->moduleNum ();
              for (int j = 0; j < moduleNum; j++)
                {
                  typedef const char *(*LoadFunc) (void *, ProgressFunc,
                                                   void **, void *);
                  auto lib = mr->getModule (j);
                  auto func = reinterpret_cast<LoadFunc> (
                      lib->resolve ("region_create_from_file"));
                  void *region = nullptr;
                  auto msg = func (data, setFunc, &region, this);
                  if (msg)
                    {
                      failedList.append (dir);
                      failedReason.append (msg);
                      Q_EMIT refreshSubLabel (msg);
                      string_free (msg);
                    }
                  else
                    {
                      auto realName = QFileInfo (dir).fileName ();
                      auto lastDot = realName.lastIndexOf ('.');
                      if (lastDot != -1)
                        {
                          realName = realName.left (lastDot);
                        }
                      Region regionStruct = {
                        region,
                        realName,
                        QUuid::createUuid ().toString (),
                        QDateTime::currentDateTime (),
                      };
                      mr->appendRegion (regionStruct);
                      break;
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
          if (!cancel_flag_is_cancelled (cancel_flag))
            {
              Q_EMIT refreshFullProgress (100);
              Q_EMIT finishLoadOne ();
            }
        }
      cancel_flag_destroy (new_cancel_flag);
      finish ();
    };

  std::thread thread (real_task);
  thread.detach ();
}