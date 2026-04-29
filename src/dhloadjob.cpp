#include "dhloadjob.h"
#include <libintl.h>
#define _(str) gettext (str)
#include "generalchoosedialog.h"
#include "mainwindow.h"
#include "manageregionui.h"
#include "region.h"
#include "settings.h"
#include "utility.h"
#undef asprintf
#include <QTimer>
#include <future>
#include <qfileinfo.h>

#define err_msg_handle_func(msg, func, ...)                                   \
  if (msg)                                                                    \
    return false;                                                             \
  msg = func (__VA_ARGS__);                                                   \
  if (msg)                                                                    \
    return false;

void
DhLoadJob::start ()
{
  auto realfunc = [&]
    {
      using LoadObjectFunc
          = const char *(*)(void *, ProgressFunc, void *, const void *,
                            void **, quint64, quint64);
      using ObjFreeFunc = void (*) (void *);
      auto setFunc
          = [] (void *main_klass, int value, const char *text, const char *arg)
        {
          auto real_klass = static_cast<DhLoadJob *> (main_klass);
          real_klass->setPercent (value);
          if (!arg)
            Q_EMIT real_klass->infoMessage (real_klass, gettext (text));
          else
            {
              auto msg = QString::asprintf (gettext (text), arg);
              Q_EMIT real_klass->infoMessage (real_klass, gettext (text));
            }
        };
      auto pushRegionFunc
          = [&] (const QString &dir, const QString &name, void *region)
        {
          auto realName = QFileInfo (dir).completeBaseName ();

          if (!name.isEmpty ())
            {
              realName += " - ";
              realName += name;
            }

          ManageRegionUI::appendRegion (region, realName);
        };
      auto multiFuncProcessFunc =
          [&] (QLibrary *lib, LoadObjectFunc loadObjectFn, ObjFreeFunc objFree,
               const char *&msg, void *data, const QString &baseTypeName)
        {
          void *realObject = nullptr;
          for (const auto &i : objects)
            {
              if (i.first == baseTypeName)
                realObject = i.second.get ();
            }
          /* First we load object */
          if (loadObjectFn && !realObject)
            {
              void *o_object = nullptr;
              std::unique_ptr<void, void (*) (void *)> object{ nullptr,
                                                               nullptr };
              err_msg_handle_func (msg, loadObjectFn, data, setFunc, this,
                                   cancel_flag, &o_object,
                                   quint64 (DhConfig::elapsedMilliseconds ()),
                                   quint64 (DhConfig::memoryLimit ()));
              object = { o_object, objFree };
              realObject = o_object;
              objects.emplace_back (baseTypeName, std::move (object));
            }
          typedef int32_t (*numFunc) (void *);
          auto numFn = reinterpret_cast<numFunc> (lib->resolve ("region_num"));
          int32_t nums = 0;
          /* We need the region_num function to check whether it's nullptr */
          if ((nums = numFn (realObject)))
            {
              auto indexFn
                  = reinterpret_cast<const char *(*)(void *, qint32)> (
                      lib->resolve ("region_name_index"));
              regionList.clear ();
              for (int k = 0; k < nums; k++)
                {
                  auto name = indexFn (realObject, k);
                  regionList.append (name);
                  string_free (name);
                }

              if (DhConfig::selectAllRegionsInLoading ())
                {
                  for (auto i = 0; i < regionList.size (); i++)
                    regionIndexes << i;
                }
              else
                {
                  Q_EMIT infoMessage (this,
                                      _ ("Please click `Continue` to choose "
                                         "region(s)."));
                  /* Emit the stop signal to stop, and use loop to
                   * stop the process. */
                  Q_EMIT selfSuspended (this);
                  std::unique_lock lock (mutex);
                  cv.wait (lock);
                  /* Continue */
                  Q_EMIT selfResumed (this);
                }
              typedef const char *(*LoadFunc) (void *, ProgressFunc, void **,
                                               void *, const void *, int32_t,
                                               quint64, quint64);
              auto func = reinterpret_cast<LoadFunc> (
                  lib->resolve ("region_create_from_file_as_index"));
              for (auto index : regionIndexes)
                {
                  void *singleRegion = nullptr;
                  if (func)
                    err_msg_handle_func (
                        msg, func, realObject, setFunc, &singleRegion, this,
                        cancel_flag, index,
                        quint64 (DhConfig::elapsedMilliseconds ()),
                        quint64 (DhConfig::memoryLimit ()));

                  auto name = region_get_region_name (singleRegion);
                  pushRegionFunc (filename, name, singleRegion);
                  string_free (name);
                }
              regionIndexes.clear ();

              return true;
            }
          return false;
        };
      auto singleFuncProcessFunc =
          [&] (QLibrary *lib, LoadObjectFunc loadObjectFn, ObjFreeFunc objFree,
               const char *&msg, void *data, const QString &baseTypeName)
        {
          void *realObject = nullptr;
          for (const auto &i : objects)
            {
              if (i.first == baseTypeName)
                realObject = i.second.get ();
            }
          void *region = nullptr;
          typedef const char *(*LoadFunc) (void *, ProgressFunc, void **,
                                           void *, const void *, quint64,
                                           quint64);
          auto func = reinterpret_cast<LoadFunc> (
              lib->resolve ("region_create_from_file"));

          if (loadObjectFn && !realObject)
            {
              void *o_object = nullptr;
              std::unique_ptr<void, void (*) (void *)> object{ nullptr,
                                                               nullptr };
              err_msg_handle_func (msg, loadObjectFn, data, setFunc, this,
                                   cancel_flag, &o_object,
                                   quint64 (DhConfig::elapsedMilliseconds ()),
                                   quint64 (DhConfig::memoryLimit ()));
              object = { o_object, objFree };
              realObject = o_object;
              objects.emplace_back (baseTypeName, std::move (object));
            }
          if (func)
            err_msg_handle_func (msg, func, realObject, setFunc, &region, this,
                                 cancel_flag,
                                 quint64 (DhConfig::elapsedMilliseconds ()),
                                 quint64 (DhConfig::memoryLimit ()));

          pushRegionFunc (filename, {}, region);
          return true;
        };
      auto pushMsgFunc = [&] (const char *msg)
        {
          QString realMsg = "**%1**:\n\n%2";
          realMsg = realMsg.arg (filename).arg (msg);
          setErrorText (realMsg);
          string_free (msg);
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

      /* Start */
      if (cancel_flag_is_cancelled (cancel_flag))
        {
          setErrorText (_ ("Cancelled."));
          Q_EMIT emitResult ();
          return;
        }
      int failed = 0;
      void *o_data = file_try_uncompress (
          filename.toUtf8 (), setFunc, this, &failed, cancel_flag,
          quint64 (DhConfig::elapsedMilliseconds ()),
          quint64 (DhConfig::memoryLimit ()));
      if (failed)
        {
          auto err_msg = vec_to_cstr (o_data);
          pushMsgFunc (err_msg);
          Q_EMIT emitResult ();
          return;
        }
      std::unique_ptr<void, void (*) (void *)> data{ o_data, vec_free };
      auto moduleNum = ManageRegionUI::moduleNum ();
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

          auto lib = ManageRegionUI::getModule (j);
          auto multiFn = reinterpret_cast<qint32 (*) ()> (
              lib->resolve ("region_is_multi"));
          auto objFree
              = reinterpret_cast<ObjFreeFunc> (lib->resolve ("object_free"));
          auto loadObjectFn = reinterpret_cast<LoadObjectFunc> (
              lib->resolve ("region_get_object"));
          auto transFn = reinterpret_cast<const char *(*)(const char *)> (
              lib->resolve ("init_translation"));
          auto nameFn = reinterpret_cast<const char *(*)()> (
              lib->resolve ("region_type"));
          auto baseTypeFn = reinterpret_cast<const char *(*)()> (
              lib->resolve ("region_base_type"));
          auto suffixFn = reinterpret_cast<const char *(*)()> (
              lib->resolve ("region_file_suffix"));

          QString name;
          QString baseTypeName;
          if (nameFn)
            {
              auto typeName = nameFn ();
              if (typeName)
                name = typeName;
              string_free (typeName);
            }
          if (baseTypeFn)
            {
              auto baseType = baseTypeFn ();
              if (baseType)
                baseTypeName = baseType;
              string_free (baseType);
            }
          if (suffixFn)
            {
              auto suffix = suffixFn ();
              if (suffix)
                typeName = suffix;
              string_free (suffix);
            }

          QString realSuffix = QFileInfo (filename).suffix ();

          /* Loaded by file extension name */

          if (DhConfig::loadingFileByExtension () && !realSuffix.isEmpty ())
            if (realSuffix != typeName)
              continue;

          if (name.isEmpty ())
            name = _ ("unknown");

          if (transFn)
            msg = transFn (dh::getTranslationDir ().toUtf8 ());

          if (multiFn)
            {
              if (multiFn ())
                {
                  if (multiFuncProcessFunc (lib, loadObjectFn, objFree, msg,
                                            data.get (), baseTypeName))
                    break;
                  tryErrorFunc (name, msg, realStr, j != moduleNum - 1);
                }
              else
                {
                  if (singleFuncProcessFunc (lib, loadObjectFn, objFree, msg,
                                             data.get (), baseTypeName))
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
            pushMsgFunc (msg);
          else
            {
              QString realMsg = "**%1**:\n\n%2";
              realMsg = realMsg.arg (filename).arg (realStr);
              setErrorText (realMsg);
            }
        }
      Q_EMIT emitResult ();
    };
  std::thread trd (realfunc);
  trd.detach ();
}

bool
DhLoadJob::doResume ()
{
  auto indexes = GeneralChooseDialog::getIndexes (
      _ ("Select a Region"), _ ("Please select a region."), regionList);
  if (!indexes.isEmpty ())
    regionIndexes = indexes;
  cv.notify_one ();
  return true;
}

void
DhLoadJob::forceResume ()
{
  cv.notify_one ();
}

QString
DhLoadJob::getFilename ()
{
  return filename;
}

QString
DhLoadJob::getTypeName ()
{
  return typeName;
}

DhAllLoadJob::DhAllLoadJob (QStringList list, QMainWindow *mainWindow,
                            ManageRegionUI *mrui, QObject *parent)
    : KCompositeJob (parent), cancel_flag (cancel_flag_new ()),
      mainWindow (mainWindow), mrui (mrui)
{
  auto realWindow = qobject_cast<MainWindow *> (this->mainWindow);
  connect (this, &DhAllLoadJob::cancel, this,
           [&] { cancel_flag_cancel (this->cancel_flag); });
  jobNums = list.length ();
  messageWidget = new KMessageWidget ();
  messageWidget->installEventFilter (this);
  QString text = _ ("Finish processing %1 of %2 (%3%).");
  text = text.arg (this->finishedJobs).arg (this->jobNums).arg (percent ());
  messageWidget->setText (text);
  MainWindow::addWidgetToToolBar (messageWidget);
  connect (this, &DhAllLoadJob::percentChanged, this,
           [&]
             {
               QString textb = _ ("Finish processing %1 of %2 (%3%).");
               textb = textb.arg (this->finishedJobs)
                           .arg (this->jobNums)
                           .arg (percent ());
               messageWidget->setText (textb);
             });
  int i = 0;
  for (auto filename : list)
    {
      auto job = new DhLoadJob (filename, cancel_flag);
      job->setAutoDelete (true);
      KCompositeJob::addSubjob (job);
      connect (job, &DhLoadJob::result, this,
               [&] (KJob *finishedJob)
                 {
                   this->finishedJobs += 1;
                   setPercent (this->finishedJobs * 100 / this->jobNums);
                   auto failedText = finishedJob->errorText ();
                   if (!failedText.isEmpty ())
                     {
                       auto failedWidget = new KMessageWidget ();
                       connect (failedWidget,
                                &KMessageWidget::hideAnimationFinished,
                                failedWidget, &KMessageWidget::deleteLater);
                       MainWindow::addWidgetToToolBar (failedWidget);
                       failedWidget->setMessageType (KMessageWidget::Error);
                       failedWidget->setText (failedText);
                       failedWidget->setTextFormat (Qt::MarkdownText);
                       QTimer::singleShot (5000, failedWidget,
                                           &KMessageWidget::deleteLater);
                     }
                   removeSubjob (finishedJob);
                   this->mrui->refresh_triggered ();
                   if (!hasSubjobs ())
                     {
                       this->messageWidget->deleteLater ();
                       deleteLater ();
                     }
                 });
      job->messageWidget = new KMessageWidget ();
      job->messageWidget->setCloseButtonVisible (false);
      connect (job->messageWidget, &KMessageWidget::hideAnimationFinished,
               job->messageWidget, &KMessageWidget::deleteLater);
      MainWindow::addWidgetToToolBar (job->messageWidget);
      connect (job, &DhLoadJob::infoMessage, this,
               [&, i] (KJob *realjob, const QString &str)
                 {
                   auto castedJob = qobject_cast<DhLoadJob *> (realjob);
                   QString realStr = "%1: %2 (%3%)";
                   QString realFilename = castedJob->getFilename ();
                   QString realPrefix = realFilename;
                   if (!castedJob->getTypeName ().isEmpty ())
                     realPrefix
                         = realPrefix + " (" + castedJob->getTypeName () + ")";
                   realStr = realStr.arg (realPrefix)
                                 .arg (str)
                                 .arg (realjob->percent ());
                   castedJob->messageWidget->setText (realStr);
                 });
      connect (job, &DhLoadJob::result, job->messageWidget,
               &KMessageWidget::animatedHide);
      connect (job, &DhLoadJob::selfSuspended, this,
               [&] (KJob *realJob)
                 {
                   auto castedJob = qobject_cast<DhLoadJob *> (realJob);
                   QAction *action = new QAction (_ ("Continue"));
                   connect (action, &QAction::triggered, castedJob,
                            &DhLoadJob::doResume);
                   castedJob->messageWidget->addAction (action);
                 });
      connect (job, &DhLoadJob::selfResumed, this,
               [&] (KJob *realJob)
                 {
                   auto castedJob = qobject_cast<DhLoadJob *> (realJob);
                   castedJob->messageWidget->clearActions ();
                 });
      i++;
    }
}

void
DhAllLoadJob::start ()
{
  for (const auto &job : subjobs ())
    job->start ();
}

bool
DhAllLoadJob::eventFilter (QObject *watched, QEvent *event)
{
  if (watched == messageWidget && event->type () == QEvent::Hide)
    {
      cancel_flag_cancel (cancel_flag);
      for (auto &job : subjobs ())
        qobject_cast<DhLoadJob *> (job)->forceResume ();
    }
  return KCompositeJob::eventFilter (watched, event);
}
