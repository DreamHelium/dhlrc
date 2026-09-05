#include "dhloadjob.h"
#include <QFuture>
#include <libintl.h>
#include <memory>
#include <qobject.h>
#define _(str) gettext (str)
#include "generalchoosedialog.h"
#include "mainwindow.h"
#include "manageregionui.h"
#include "region.h"
#include "settings.h"
#undef asprintf
#include <QTimer>
#include <qfileinfo.h>
#include <qtconcurrentrun.h>

void
DhLoadJob::start ()
{
  future
      = QtConcurrent::run (
            [&]
              {
                /* Loading File */
                int failed = false;
                auto tempVec = file_try_uncompress (
                    filename.toUtf8 (), helper_struct.get (), &failed);
                if (failed)
                  {
                    auto msg = vec_to_cstr (tempVec);
                    QString failMsg = msg;
                    string_free (msg);
                    throw DhLoadError (failMsg, _ ("Loading File"));
                  }
                return std::unique_ptr<VecU8, void (*) (VecU8 *)> (tempVec,
                                                                   vec_free);
              })
            .then (
                [&] (std::unique_ptr<void, void (*) (void *)> ptr)
                  {
                    /* Loading Object */
                    auto objectList = ManageRegionUI::getLoadObjectList ();
                    DhMultiLoadError errors;
                    for (const auto &load : objectList)
                      {
                        void *object = nullptr;
                        auto msg = load.loadObjectFunc (ptr.get (), &object,
                                                        helper_struct.get ());
                        if (msg)
                          {
                            QString typeWithPrefix = _ ("Loading Object %1");
                            typeWithPrefix
                                = typeWithPrefix.arg (load.baseType);
                            errors.appendError (msg, typeWithPrefix);
                            string_free (msg);
                            continue;
                          }
                        return std::make_pair (
                            load.baseType,
                            std::unique_ptr<void, void (*) (void *)>{
                                object, load.objFreeFunc });
                      }
                    throw errors;
                  })
            .then (
                [&] (std::pair<QString,
                               std::unique_ptr<void, void (*) (void *)>>
                         tempObject)
                  {
                    /* Loading Region */
                    auto baseList = ManageRegionUI::getModules ();
                    /* Get available type list
                     * [type, fileSuffix]
                     */
                    QList<std::pair<QString, QString>> typeList;
                    for (const auto &i : baseList)
                      {
                        auto base = i;
                        if (base->baseType == tempObject.first)
                          typeList.append ({ base->type, base->fileSuffix });
                      }
                    /* try */
                    /* Temporary strategy:
                     * 1. If loading file by extension without retry, we just
                     * load.
                     * 2. If retry, we use the type list before and try one by
                     * one.
                     */
                    if (DhConfig::loadingFileByExtension ())
                      {
                        auto extension = QFileInfo (filename).suffix ();
                        /* Store the real item */
                        std::pair<QString, QString> realItem;
                        for (const auto &[type, fileSuffix] : typeList)
                          {
                            if (fileSuffix == extension)
                              {
                                realItem = { type, fileSuffix };
                                break;
                              }
                          }
                        if (!DhConfig::failThenRetry ())
                          {
                            /* We need to load just once, so first we clear. */
                            typeList.clear ();
                            if (!realItem.first.isEmpty ())
                              typeList.append (realItem);
                          }
                        else
                          {
                            /* If no file extension, it will get error if
                             * realItem is empty. So we need to determine.
                             */
                            if (typeList[0] != realItem
                                && !realItem.first.isEmpty ())
                              typeList.swapItemsAt (
                                  0, typeList.indexOf (realItem));
                          }
                      }
                    DhMultiLoadError err;
                    for (const auto &pair : typeList)
                      {
                        auto type = pair.first;
                        ModuleBase *base = nullptr;
                        for (const auto &i : baseList)
                          {
                            if (i->type == type)
                              {
                                base = i;
                                break;
                              }
                          }
                        if (base)
                          {
                            if (base->multiSupport)
                              {
                                if (loadMultiRegion (
                                        base, tempObject.second.get (), err))
                                  break;
                              }
                            else
                              {
                                auto singleBase
                                    = dynamic_cast<SingleModuleBase *> (base);
                                void *region = nullptr;
                                auto msg = singleBase->loadFunc (
                                    tempObject.second.get (), setFunc, &region,
                                    this, cancel_flag,
                                    quint64 (DhConfig::elapsedMilliseconds ()),
                                    quint64 (DhConfig::memoryLimit ()));
                                if (msg)
                                  {
                                    QString prefix
                                        = _ ("Loading region type %1");
                                    prefix = prefix.arg (singleBase->type);
                                    err.appendError (msg, prefix);
                                    string_free (msg);
                                    continue;
                                  }
                                auto fileBaseName
                                    = QFileInfo (filename).completeBaseName ();
                                ManageRegionUI::appendRegion (region,
                                                              fileBaseName);
                                if (pair != typeList[0])
                                  {
                                    QString prefix
                                        = _ ("Loading region type %1");
                                    prefix = prefix.arg (pair.first);
                                    QString message = _ (
                                        "Loading progress is successful, but "
                                        "the file type should be %1.");
                                    message = message.arg (typeList[0].second);
                                    err.appendError (message, prefix);
                                  }
                                break;
                              }
                          }
                      }
                    throw err;
                  })
            .onFailed (
                [&] (const DhLoadError &err)
                  {
                    QString msg = _ ("Failed when %1: %2");
                    msg = msg.arg (err.state).arg (err.error);
                    QString realMsg = "**%1**:\n\n%2";
                    realMsg = realMsg.arg (filename).arg (msg);
                    setErrorText (realMsg);
                    Q_EMIT emitResult ();
                  })
            .onFailed (
                [&] (const DhMultiLoadError &err)
                  {
                    QString realFailMsg;
                    for (auto i = 0; i < err.errors.length (); i++)
                      {
                        QString msg = _ ("Failed when %1: %2");
                        msg = msg.arg (err.errors[i].state)
                                  .arg (err.errors[i].error);
                        if (i != err.errors.length () - 1)
                          msg += "\n\n";
                        realFailMsg += msg;
                      }
                    QString realMsg;
                    if (!realFailMsg.isEmpty ())
                      {
                        realMsg = "**%1**:\n\n%2";
                        realMsg = realMsg.arg (filename).arg (realFailMsg);
                      }
                    setErrorText (realMsg);
                    Q_EMIT emitResult ();
                  })
            .onFailed (
                [&] ()
                  {
                    qDebug () << "?";
                    Q_EMIT emitResult ();
                  })
            .onCanceled ([&] { Q_EMIT emitResult (); });
  /* It seems that we don't need this. */
  connect (this, &DhLoadJob::selfCancel, this,
           [&] { /*this->future.cancelChain ();*/ });
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

void
DhLoadJob::setFunc (void *main_klass, int value, const char *text,
                    const char *arg)
{
  auto real_klass = static_cast<DhLoadJob *> (main_klass);
  real_klass->setPercent (value);
  if (!arg)
    Q_EMIT real_klass->infoMessage (real_klass, gettext (text));
  else
    {
      auto msg = QString::asprintf (gettext (text), arg);
      Q_EMIT real_klass->infoMessage (real_klass, msg);
    }
}

bool
DhLoadJob::loadMultiRegion (ModuleBase *base, void *object,
                            DhMultiLoadError &err)
{
  auto multiBase = dynamic_cast<MultiModuleBase *> (base);
  if (!multiBase)
    {
      err.appendError (_ ("Not a valid multi-region module"),
                       _ ("Loading Region"));
      return false;
    }
  auto num = multiBase->numFunc (object);
  for (int j = 0; j < num; j++)
    {
      auto name = multiBase->nameFunc (object, j);
      regionList.append (name);
      string_free (name);
    }
  if (!DhConfig::selectAllRegionsInLoading ())
    {

      Q_EMIT infoMessage (this,
                          _ ("Please click `Continue` to choose region(s)."));
      /* Emit the stop signal to stop, and use loop to
       * stop the process. */
      Q_EMIT selfSuspended (this);
      std::unique_lock lock (mutex);
      cv.wait (lock);
      /* Continue */
      Q_EMIT selfResumed (this);
    }
  else
    {
      for (int j = 0; j < num; j++)
        regionIndexes << j;
    }
  for (const auto &index : regionIndexes)
    {
      void *singleRegion = nullptr;
      auto msg = multiBase->loadFunc (object, &singleRegion, index,
                                      helper_struct.get ());
      if (msg)
        {
          QString prefix = _ ("Loading region type %1");
          prefix = prefix.arg (multiBase->type);
          err.appendError (msg, prefix);
          string_free (msg);
          return false;
        }
      auto name = region_get_region_name (singleRegion);
      auto fileBaseName = QFileInfo (filename).completeBaseName ();
      ManageRegionUI::appendRegion (singleRegion, fileBaseName + " - " + name);
      string_free (name);
    }
  if (regionList.isEmpty ())
    return false;
  Q_EMIT loadRegionSuccess ();
  return true;
}

DhAllLoadJob::DhAllLoadJob (QStringList list, QObject *parent)
    : KCompositeJob (parent), cancel_flag (cancel_flag_new ())
{
  connect (this, &DhAllLoadJob::cancel, this,
           [&]
             {
               cancel_flag_cancel (this->cancel_flag);
               for (const auto &job : this->subjobs ())
                 Q_EMIT qobject_cast<DhLoadJob *> (job)->selfCancel ();
             });
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
                                           &KMessageWidget::animatedHide);
                     }
                   removeSubjob (finishedJob);
                   Q_EMIT ManageRegionUI::instance ()->regionChanged ();
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
      Q_EMIT cancel ();
      for (auto &job : subjobs ())
        qobject_cast<DhLoadJob *> (job)->forceResume ();
    }
  return KCompositeJob::eventFilter (watched, event);
}
