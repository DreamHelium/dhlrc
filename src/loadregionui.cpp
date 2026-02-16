#include "loadregionui.h"
#include <QApplication>
#include <QDir>
#include <QLibrary>
#include <QMessageBox>
#include <QTimer>
#include <coroutine>
#include <future>
// #include <lrchooseui.h>
#include <QThread>
#include <QUuid>
#include <generalchoosedialog.h>
#include <manage.h>
#include <region.h>
#include <utility.h>
#define _(str) gettext (str)
#undef asprintf

#define err_msg_handle_func(msg, func, ...)                                   \
  if (msg)                                                                    \
    return false;                                                             \
  msg = func (__VA_ARGS__);                                                   \
  if (msg)                                                                    \
    return false;

LoadRegionUI::LoadRegionUI (QStringList list, dh::ManageRegion *mr,
                            QWidget *parent)
    : LoadObjectUI (parent), mr (mr), list (list),
      cancel_flag (cancel_flag_new ())
{
  setLabel (_ ("Loading file(s) to Region(s)."));
  connect (this, &LoadRegionUI::winClose, this,
           [&]
             {
               if (!finished)
                 {
                   cancel_flag_cancel (cancel_flag);
                   cv.notify_one ();
                   Q_EMIT refreshFullLabel (_ ("Please wait..."));
                 }

               if (stopped)
                 {
                   cv.notify_one ();
                   failedList.append (currentDir);
                   failedReason.append (_ ("Cancelled."));
                 }
               if (!failedList.isEmpty ())
                 {
                   QString errorMsg
                       = _ ("The following files are not added:\n\n");
                   for (int i = 0; i < failedList.size (); i++)
                     {
                       const QString &dir = failedList.at (i);
                       const QString &reason = failedReason.at (i);
                       errorMsg
                           += "**" + dir + "**" + ":\n\n" + reason + "\n\n";
                     }
                   auto messageBox = new QMessageBox (this);
                   messageBox->setIcon (QMessageBox::Critical);
                   messageBox->setWindowTitle (_ ("Error!"));
                   messageBox->setText (errorMsg);
                   messageBox->setTextFormat (Qt::MarkdownText);
                   messageBox->exec ();
                   delete messageBox;
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
                     regionIndexes = indexes;
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
      using LoadObjectFunc = const char *(*)(void *, ProgressFunc, void *,
                                             const void *, void **);
      using ObjFreeFunc = void (*) (void *);
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
      auto pushRegionFunc
          = [&] (const QString &dir, const QString &name, void *region)
        {
          auto realName = QFileInfo (dir).fileName ();
          auto lastDot = realName.lastIndexOf ('.');
          if (lastDot != -1)
            realName = realName.left (lastDot);
          if (!name.isEmpty ())
            {
              realName += " - ";
              realName += name;
            }

          mr->appendRegion (region, realName);
        };
      auto singleFuncProcessFunc =
          [&] (const QString &dir, QLibrary *lib, LoadObjectFunc loadObjectFn,
               ObjFreeFunc objFree, const char *&msg, void *data)
        {
          void *o_object = nullptr;
          std::unique_ptr<void, void (*) (void *)> object{ nullptr, nullptr };
          void *region = nullptr;
          typedef const char *(*LoadFunc) (void *, ProgressFunc, void **,
                                           void *, const void *);
          auto func = reinterpret_cast<LoadFunc> (
              lib->resolve ("region_create_from_file"));

          if (loadObjectFn)
            {
              err_msg_handle_func (msg, loadObjectFn, data, setFunc, this,
                                   cancel_flag, &o_object);
              object = { o_object, objFree };
            }
          if (func)
            err_msg_handle_func (msg, func, object.get (), setFunc, &region,
                                 this, cancel_flag);

          pushRegionFunc (dir, {}, region);
          return true;
        };

      auto multiFuncProcessFunc
          = [&] (const QString &dir, QLibrary *lib,
                 LoadObjectFunc loadObjectFn, ObjFreeFunc objFree,
                 const char *&msg, void *data, const QString &realLabel)
        {
          void *o_object = nullptr;
          /* First we load object */
          if (loadObjectFn)
            err_msg_handle_func (msg, loadObjectFn, data, setFunc, this,
                                 cancel_flag, &o_object);
          std::unique_ptr<void, void (*) (void *)> object{ o_object, objFree };
          typedef int32_t (*numFunc) (void *);
          auto numFn = reinterpret_cast<numFunc> (lib->resolve ("region_num"));
          int32_t nums = 0;
          /* We need the region_num function to check whether it's nullptr */
          if ((nums = numFn (object.get ())))
            {
              auto indexFn
                  = reinterpret_cast<const char *(*)(void *, qint32)> (
                      lib->resolve ("region_name_index"));
              regionList.clear ();
              for (int k = 0; k < nums; k++)
                {
                  auto name = indexFn (object.get (), k);
                  regionList.append (name);
                  string_free (name);
                }
              Q_EMIT refreshFullLabel (_ ("Please click `Continue` to choose "
                                          "region(s)."));
              /* Emit the stop signal to stop, and use loop to
               * stop the process. */
              Q_EMIT stopProgress ();
              std::unique_lock lock (mutex);
              cv.wait (lock);
              /* Continue */
              Q_EMIT refreshFullLabel (realLabel);
              typedef const char *(*LoadFunc) (void *, ProgressFunc, void **,
                                               void *, const void *, int32_t);
              auto func = reinterpret_cast<LoadFunc> (
                  lib->resolve ("region_create_from_file_as_index"));
              for (auto index : regionIndexes)
                {
                  void *singleRegion = nullptr;
                  if (func)
                    err_msg_handle_func (msg, func, object.get (), setFunc,
                                         &singleRegion, this, cancel_flag,
                                         index);

                  auto name = region_get_region_name (singleRegion);
                  pushRegionFunc (dir, name, singleRegion);
                  string_free (name);
                }
              regionIndexes.clear ();
              return true;
            }
          return false;
        };
      auto tryErrorFunc = [&] (const QString &name, const char *msg,
                               QString &realStr, bool addEnter)
        {
          QString realMsg = msg;
          if (!msg)
            realMsg = _ ("No region found in this type.");
          QString ret = _ ("%1 try's fail message: %2");
          ret = ret.arg (name).arg (realMsg);
          realStr += ret;
          string_free (msg);
          if (addEnter)
            realStr += "\n\n";
        };

      auto realProcessFunc = [&] (const QString &dir, int i)
        {
          currentDir = dir;
          if (cancel_flag_is_cancelled (cancel_flag))
            return;

          Q_EMIT refreshFullProgress (i * 100 / list.size ());
          QString realLabel = "[%1/%2] %3";
          realLabel = realLabel.arg (i + 1).arg (list.size ()).arg (dir);
          Q_EMIT refreshFullLabel (realLabel);

          /* Processing stuff */
          Q_EMIT refreshSubLabel (_ ("Parsing NBT"));
          int failed = 0;
          void *o_data = file_try_uncompress (dir.toUtf8 (), setFunc, this,
                                              &failed, cancel_flag);
          if (failed)
            {
              auto err_msg = vec_to_cstr (o_data);
              pushMsgFunc (dir, err_msg);
              return;
            }
          std::unique_ptr<void, void (*) (void *)> data{ o_data, vec_free };
          auto moduleNum = mr->moduleNum ();
          const char *msg = nullptr;
          int j = 0;
          failed = false;
          QString realStr;
          for (; j < moduleNum; j++)
            {
              if (cancel_flag_is_cancelled (cancel_flag))
                {
                  failed = true;
                  break;
                }

              auto lib = mr->getModule (j);
              auto multiFn = reinterpret_cast<qint32 (*) ()> (
                  lib->resolve ("region_is_multi"));
              auto objFree = reinterpret_cast<ObjFreeFunc> (
                  lib->resolve ("object_free"));
              auto loadObjectFn = reinterpret_cast<LoadObjectFunc> (
                  lib->resolve ("region_get_object"));
              auto transFn = reinterpret_cast<const char *(*)(const char *)> (
                  lib->resolve ("init_translation"));
              auto nameFn = reinterpret_cast<const char *(*)()> (
                  lib->resolve ("region_type"));

              QString name;
              if (nameFn)
                {
                  auto typeName = nameFn ();
                  if (typeName)
                    name = typeName;
                  string_free (typeName);
                }
              if (name.isEmpty ())
                name = _ ("unknown");

              if (transFn)
                msg = transFn (dh::getTranslationDir ().toUtf8 ());

              if (multiFn)
                {
                  if (multiFn ())
                    {
                      if (multiFuncProcessFunc (dir, lib, loadObjectFn,
                                                objFree, msg, data.get (),
                                                realLabel))
                        break;
                      tryErrorFunc (name, msg, realStr, j != moduleNum - 1);
                    }
                  else
                    {
                      if (singleFuncProcessFunc (dir, lib, loadObjectFn,
                                                 objFree, msg, data.get ()))
                        break;
                      tryErrorFunc (name, msg, realStr, j != moduleNum - 1);
                    }
                }
              if (j == moduleNum - 1)
                failed = true;
            }
          if (failed)
            {
              if (realStr.isEmpty ())
                pushMsgFunc (dir, msg);
              else
                {
                  failedList.append (dir);
                  failedReason.append (realStr);
                  Q_EMIT refreshSubLabel (realStr);
                }
            }

          if (!failed && !cancel_flag_is_cancelled (cancel_flag))
            Q_EMIT finishLoadOne ();
        };
      int i = 0;
      for (const auto &dir : list)
        {
          realProcessFunc (dir, i);
          i++;
        }
      Q_EMIT refreshFullProgress (100);
      cv.notify_one ();
      Q_EMIT finish ();
    };

  std::thread thread (real_task);
  thread.detach ();
}