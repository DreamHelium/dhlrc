#include "loadregionui.h"

#include <QApplication>
#include <QDir>
#include <QLibrary>
#include <QMessageBox>
#include <QTimer>
#include <future>
// #include <lrchooseui.h>
#include <QThread>
#include <QUuid>
#include <generalchoosedialog.h>
#include <manage.h>
#include <region.h>
#include <utility.h>
#define _(str) gettext (str)

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
               QThread::msleep (10);
               if (stopped)
                 {
                   cv.notify_one ();
                   failedList.append (currentDir);
                   failedReason.append (_ ("Cancelled."));
                 }
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
                   QMessageBox::critical (this, _ ("Error!"), errorMsg);
                 }
             });
  connect (this, &LoadRegionUI::continued, this,
           [&]
             {
               if (!finished)
                 {
                   auto indexes = GeneralChooseDialog::getIndexes (
                       _ ("Select a Region"), _ ("Please select a region."),
                       regionList);
                   if (!indexes.isEmpty ())
                     {
                       regionIndexes = indexes;
                     }
                   cv.notify_one ();
                   stopped = false;
                 }
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
      auto setFunc
          = [] (void *main_klass, int value, const char *text, const char *arg)
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
      auto pushMsgFunc = [&] (const QString &curDir, const char *msg)
        {
          failedList.append (curDir);
          failedReason.append (msg);
          Q_EMIT refreshSubLabel (msg);
          string_free (msg);
        };
      auto realProcessFunc = [&] (const QString &dir, int i)
        {
          if (cancel_flag_is_cancelled (cancel_flag))
            return;

          Q_EMIT refreshFullProgress (i * 100 / list.size ());
          QString realLabel = "[%1/%2] %3";
          realLabel = realLabel.arg (i + 1).arg (list.size ()).arg (dir);
          Q_EMIT refreshFullLabel (realLabel);

          /* Processing stuff */
          Q_EMIT refreshSubLabel (_ ("Parsing NBT"));
          int failed = 0;
          void *data = file_try_uncompress (dir.toUtf8 (), setFunc, this,
                                            &failed, cancel_flag);
          if (failed)
            {
              auto err_msg = vec_to_cstr (data);
              pushMsgFunc (dir, err_msg);
            }
          else
            {
              auto moduleNum = mr->moduleNum ();
              const char *msg = nullptr;
              for (int j = 0; j < moduleNum; j++)
                {
                  typedef int32_t *(*multiFunc) ();
                  object = nullptr;
                  typedef const char *(*LoadTranslation) (const char *);
                  auto lib = mr->getModule (j);
                  auto multiFn = reinterpret_cast<multiFunc> (
                      lib->resolve ("region_is_multi"));
                  auto objFree = reinterpret_cast<void (*) (void *)> (
                      lib->resolve ("object_free"));
                  typedef const char *(*LoadObjectFunc) (
                      void *, ProgressFunc, void *, const void *, void **);
                  auto loadObjectFn = reinterpret_cast<LoadObjectFunc> (
                      lib->resolve ("region_get_object"));
                  auto transFn = reinterpret_cast<LoadTranslation> (
                      lib->resolve ("init_translation"));

                  if (transFn)
                    msg = transFn (dh::getTranslationDir ().toUtf8 ());
                  if (multiFn)
                    {
                      if (multiFn ())
                        {
                          if (loadObjectFn && !msg)
                            {
                              msg = loadObjectFn (data, setFunc, this,
                                                  cancel_flag, &object);
                              if (msg)
                                {
                                  if (!cancel_flag_is_cancelled (cancel_flag))
                                    vec_free (data);
                                  pushMsgFunc (dir, msg);
                                }
                            }
                          typedef int32_t (*numFunc) (void *);
                          auto numFn = reinterpret_cast<numFunc> (
                              lib->resolve ("region_num"));
                          int32_t nums = 0;
                          if ((nums = numFn (object)))
                            {
                              library = lib;
                              currentDir = dir;
                              auto indexFn
                                  = reinterpret_cast<const char *(*)(void *,
                                                                     qint32)> (
                                      lib->resolve ("region_name_index"));
                              regionList.clear ();
                              for (int k = 0; k < nums; k++)
                                {
                                  auto name = indexFn (object, k);
                                  regionList.append (name);
                                  string_free (name);
                                }
                              Q_EMIT refreshFullLabel (
                                  _ ("Please click `Continue` to choose "
                                     "region(s)."));
                              /* Emit the stop signal to stop, and use loop to
                               * stop the process. */
                              Q_EMIT stopProgress ();
                              std::unique_lock lock (mutex);
                              cv.wait (lock);
                              /* Continue */
                              Q_EMIT refreshFullLabel (realLabel);
                              typedef const char *(*LoadFunc) (
                                  void *, ProgressFunc, void **, void *,
                                  const void *, int32_t);
                              auto func = reinterpret_cast<LoadFunc> (
                                  library->resolve (
                                      "region_create_from_file_as_index"));
                              for (auto index : regionIndexes)
                                {
                                  void *singleRegion = nullptr;
                                  if (func)
                                    msg = func (object, setFunc, &singleRegion,
                                                this, cancel_flag, index);
                                  if (msg)
                                    pushMsgFunc (dir, msg);
                                  else
                                    {
                                      auto realName
                                          = QFileInfo (dir).fileName ();
                                      auto lastDot
                                          = realName.lastIndexOf ('.');
                                      if (lastDot != -1)
                                        {
                                          realName = realName.left (lastDot);
                                        }
                                      auto name
                                          = region_get_name (singleRegion);
                                      realName += " - ";
                                      realName += name;
                                      string_free (name);
                                      Region regionStruct = {
                                        singleRegion,
                                        realName,
                                        QUuid::createUuid ().toString (),
                                        QDateTime::currentDateTime (),
                                        new QReadWriteLock (),
                                      };
                                      mr->appendRegion (regionStruct);
                                    }
                                }
                              objFree (object);
                              regionIndexes.clear ();
                              vec_free (data);
                              object = nullptr;
                              library = nullptr;
                              break;
                            }
                          else
                            objFree (object);
                        }
                      else
                        {
                          typedef const char *(*LoadFunc) (
                              void *, ProgressFunc, void **, void *,
                              const void *);
                          auto func = reinterpret_cast<LoadFunc> (
                              lib->resolve ("region_create_from_file"));

                          if (loadObjectFn && !msg)
                            {
                              msg = loadObjectFn (data, setFunc, this,
                                                  cancel_flag, &object);
                              vec_free (data);
                              if (msg
                                  && !cancel_flag_is_cancelled (cancel_flag))
                                vec_free (data);
                            }
                          if (func && !msg)
                            msg = func (object, setFunc, &region, this,
                                        cancel_flag);
                          if (msg)
                            if (j != moduleNum - 1)
                              string_free (msg);
                          object = nullptr;
                          if (msg)
                            pushMsgFunc (dir, msg);
                          if (region)
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
                                new QReadWriteLock (),
                              };
                              mr->appendRegion (regionStruct);
                              region = nullptr;
                              break;
                            }
                        }
                    }
                  else
                    pushMsgFunc (dir, msg);
                }

              if (!cancel_flag_is_cancelled (cancel_flag))
                {
                  Q_EMIT refreshFullProgress (100);
                  Q_EMIT finishLoadOne ();
                }
            }
          cancel_flag_destroy (cancel_flag);
        };
      int i = 0;
      cancel_flag_clone (cancel_flag);
      for (const auto &dir : list)
        {
          realProcessFunc (dir, i);
          i++;
        }

      finish ();
    };

  std::thread thread (real_task);
  thread.detach ();
}