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
#include "utility.h"
#undef asprintf
#include <QTimer>
#include <qfileinfo.h>
#include <qtconcurrentrun.h>

/* Shared message helpers, so the same wording is not spelled out repeatedly.
 */
static QString
loadingTypePrefix (const QString &type)
{
  return QString (_ ("Loading region type %1")).arg (type);
}

static QString
failedMessage (const QString &state, const QString &error)
{
  return QString (_ ("Failed when %1: %2")).arg (state).arg (error);
}

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
                    for (const auto &base : baseList)
                      {
                        if (base->baseType () == tempObject.first)
                          typeList.append (
                              { base->type (), base->fileSuffix () });
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
                        auto *base = ManageRegionUI::getModule (pair.first);
                        if (!base)
                          continue;
                        if (auto *multi = base->multi ())
                          {
                            if (loadMultiRegion (
                                    multi, tempObject.second.get (), err))
                              {
                                typeName = base->type ();
                                break;
                              }
                          }
                        else if (auto *single = base->single ())
                          {
                            void *region = nullptr;
                            auto msg = single->loadFunc (
                                tempObject.second.get (), &region,
                                helper_struct.get ());
                            if (msg)
                              {
                                err.appendError (
                                    msg, loadingTypePrefix (base->type ()));
                                string_free (msg);
                                continue;
                              }
                            /* GUI label: the file name without the
                             * extension. The region's own name comes from the
                             * file itself. */
                            auto displayName
                                = QFileInfo (filename).completeBaseName ();
                            ManageRegionUI::appendRegion (region, displayName);
                            if (pair != typeList[0])
                              {
                                QString message
                                    = _ ("Loading progress is successful, but "
                                         "the file type should be %1.");
                                message = message.arg (pair.first);
                                err.appendError (
                                    message,
                                    loadingTypePrefix (typeList[0].second));
                              }
                            typeName = base->type ();
                            break;
                          }
                      }
                    throw err;
                  })
            .onFailed (
                [&] (const DhLoadError &err)
                  {
                    QString realMsg = "**%1**:\n\n%2";
                    realMsg = realMsg.arg (filename).arg (
                        failedMessage (err.state, err.error));
                    setErrorText (realMsg);
                    Q_EMIT emitResult ();
                  })
            .onFailed (
                [&] (const DhMultiLoadError &err)
                  {
                    QStringList messages;
                    for (const auto &loadError : err.errors)
                      {
                        messages << failedMessage (loadError.state,
                                                   loadError.error);
                      }
                    QString realMsg;
                    if (!messages.isEmpty ())
                      {
                        realMsg = "**%1**:\n\n%2";
                        realMsg = realMsg.arg (filename).arg (
                            messages.join ("\n\n"));
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
}

bool
DhLoadJob::doResume ()
{
  auto indexes = GeneralChooseDialog::getIndexes (
      _ ("Select Region(s)"), _ ("Please select region(s)."), regionList);
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
DhLoadJob::loadMultiRegion (MultiModuleBase *multiBase, void *object,
                            DhMultiLoadError &err)
{
  /* A previous attempt in the retry loop may have filled these; start clean so
   * the region list is not duplicated. */
  regionList.clear ();
  regionIndexes.clear ();

  auto num = multiBase->numFunc (object);
  for (int j = 0; j < num; j++)
    {
      auto name = multiBase->nameFunc (object, j);
      if (name)
        regionList.append (name);
      string_free (name);
    }
  if (!DhConfig::selectAllRegionsInLoading ())
    {

      Q_EMIT infoMessage (this, _ ("Please click `Continue` to choose "
                                   "the region(s) to load."));
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
          prefix = prefix.arg (multiBase->type ());
          err.appendError (msg, prefix);
          string_free (msg);
          return false;
        }
      const char *rawName = region_get_region_name (singleRegion);
      QString name = rawName ? QString::fromUtf8 (rawName) : QString ();
      /* File name without the extension, so the pattern shows "house" rather
       * than "house.litematic". */
      auto fileBaseName = QFileInfo (filename).completeBaseName ();
      /* GUI label for the region list, built from the configured pattern. If
       * it expands to nothing, fall back to a plain "file - region". */
      auto displayName = dh::expandRegionNamePattern (
          DhConfig::multiRegionNamePattern (), fileBaseName, name);
      if (displayName.trimmed ().isEmpty ())
        displayName = QStringLiteral ("%1 - %2").arg (fileBaseName).arg (name);
      string_free (rawName);
      ManageRegionUI::appendRegion (singleRegion, displayName);
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
               cancel_flag_cancel (cancel_flag);
               for (const auto &job : this->subjobs ())
                 Q_EMIT qobject_cast<DhLoadJob *> (job)->selfCancel ();
             });
  jobNums = list.length ();
  messageWidget = new KMessageWidget ();
  messageWidget->installEventFilter (this);
  QString text = _ ("Finish processing %1 of %2 (%3%).");
  text = text.arg (this->finishedJobs).arg (this->jobNums).arg (percent ());
  messageWidget->setText (text);
  MainWindow::addWidgetToTopArea (messageWidget);
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
                   if (this->jobNums > 0)
                     setPercent (this->finishedJobs * 100 / this->jobNums);
                   auto failedText = finishedJob->errorText ();
                   if (!failedText.isEmpty ())
                     {
                       auto failedWidget = new KMessageWidget ();
                       connect (failedWidget,
                                &KMessageWidget::hideAnimationFinished,
                                failedWidget, &KMessageWidget::deleteLater);
                       MainWindow::addWidgetToTopArea (failedWidget);
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
      MainWindow::addWidgetToTopArea (job->messageWidget);
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

DhAllLoadJob::~DhAllLoadJob () { cancel_flag_destroy (cancel_flag); }

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
