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
#include <QFinalState>
#include <QStateMachine>
#include <QTimer>
#include <future>
#include <qfileinfo.h>
#include <qtconcurrentrun.h>

void
DhLoadJob::start ()
{
  auto loadFile = new QState ();
  auto loadObject = new QState ();
  auto loadRegion = new QState ();
  auto errorState = new QFinalState ();
  auto finalState = new QFinalState ();

  loadFile->addTransition (this, &DhLoadJob::error, errorState);
  loadFile->addTransition (this, &DhLoadJob::loadFileSuccess, loadObject);
  loadObject->addTransition (this, &DhLoadJob::error, errorState);
  loadObject->addTransition (this, &DhLoadJob::loadObjectSuccess, loadRegion);
  loadRegion->addTransition (this, &DhLoadJob::error, errorState);
  loadRegion->addTransition (this, &DhLoadJob::loadRegionSuccess, finalState);

  machine.addState (loadFile);
  machine.addState (loadObject);
  machine.addState (loadRegion);
  machine.addState (errorState);
  machine.addState (finalState);
  machine.setInitialState (loadFile);

  connect (&machine, &QStateMachine::started, this, &DhLoadJob::loadFile);
  connect (errorState, &QFinalState::entered, this, &DhLoadJob::doFail);
  connect (loadObject, &QState::entered, this, &DhLoadJob::loadObject);
  connect (loadRegion, &QState::entered, this, &DhLoadJob::loadRegion);
  connect (finalState, &QFinalState::entered, this, &DhLoadJob::emitResult);
  machine.start ();
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
DhLoadJob::loadMultiRegion (ModuleBase *base)
{
  auto multiBase = dynamic_cast<MultiModuleBase *> (base);
  if (!multiBase)
    {
      Q_EMIT error ();
      return false;
    }
  auto num = multiBase->numFunc (tempObject.second.get ());
  for (int j = 0; j < num; j++)
    {
      auto name = multiBase->nameFunc (tempObject.second.get (), j);
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
      auto msg = multiBase->loadFunc (
          tempObject.second.get (), setFunc, &singleRegion, this, cancel_flag,
          index, quint64 (DhConfig::elapsedMilliseconds ()),
          quint64 (DhConfig::memoryLimit ()));
      if (msg)
        {
          failMsgs.append ({ multiBase->type, msg });
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

void
DhLoadJob::loadFile ()
{
  QThreadPool::globalInstance ()->start (
      [&]
        {
          int failed = false;
          auto tempVec = file_try_uncompress (
              filename.toUtf8 (), setFunc, this, &failed, cancel_flag,
              quint64 (DhConfig::elapsedMilliseconds ()),
              quint64 (DhConfig::memoryLimit ()));
          if (failed)
            {
              auto msg = vec_to_cstr (tempVec);
              failMsgs.append ({ QString (_ ("Load file")), msg });
              string_free (msg);
              Q_EMIT error ();
              return;
            }
          vec = { tempVec, vec_free };
          Q_EMIT loadFileSuccess ();
        });
}

void
DhLoadJob::loadObject ()
{
  QThreadPool::globalInstance ()->start (
      [&]
        {
          auto objectList = ManageRegionUI::getLoadObjectList ();
          for (const auto &load : objectList)
            {
              void *object = nullptr;
              auto msg = load.loadObjectFunc (
                  vec.get (), setFunc, this, cancel_flag, &object,
                  quint64 (DhConfig::elapsedMilliseconds ()),
                  quint64 (DhConfig::memoryLimit ()));
              if (msg)
                {
                  failMsgs.append ({ load.baseType, msg });
                  string_free (msg);
                  continue;
                }
              tempObject = std::make_pair (
                  load.baseType, std::unique_ptr<void, void (*) (void *)>{
                                     object, load.objFreeFunc });
              Q_EMIT loadObjectSuccess ();
              return;
            }
          Q_EMIT error ();
        });
}

void
DhLoadJob::loadRegion ()
{
  QThreadPool::globalInstance ()->start (
      [&]
        {
          auto baseList = ManageRegionUI::getModules ();
          for (const auto &i : baseList)
            {
              auto base = i;
              if (base->baseType == tempObject.first)
                typeList.append ({ base->type, base->fileSuffix });
            }
          if (DhConfig::loadingFileByExtension ())
            {
              auto extension = QFileInfo (filename).suffix ();
              std::pair<QString, QString> realItem;
              for (const auto &item : typeList)
                {
                  if (item.second == extension)
                    realItem = item;
                }
              if (!DhConfig::failThenRetry ())
                {
                  if (!realItem.first.isEmpty ())
                    {
                      typeList.clear ();
                      typeList.append (realItem);
                    }
                }
              else
                {
                  if (typeList[0] != realItem)
                    typeList.swapItemsAt (0, typeList.indexOf (realItem));
                }
            }
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
                      if (loadMultiRegion (base))
                        return;
                    }
                  else
                    {
                      auto singleBase
                          = dynamic_cast<SingleModuleBase *> (base);
                      void *region = nullptr;
                      auto msg = singleBase->loadFunc (
                          tempObject.second.get (), setFunc, &region, this,
                          cancel_flag,
                          quint64 (DhConfig::elapsedMilliseconds ()),
                          quint64 (DhConfig::memoryLimit ()));
                      if (msg)
                        {
                          failMsgs.append ({ singleBase->type, msg });
                          string_free (msg);
                          continue;
                        }
                      auto fileBaseName
                          = QFileInfo (filename).completeBaseName ();
                      ManageRegionUI::appendRegion (region, fileBaseName);
                      Q_EMIT loadRegionSuccess ();
                      return;
                    }
                }
            }
          Q_EMIT error ();
        });
}

void
DhLoadJob::doFail ()
{
  if (!failMsgs.isEmpty ())
    {
      QString realFailMsg;
      for (auto i = 0; i < failMsgs.length (); i++)
        {
          QString msg = _ ("**%1** try's fail message: %2");
          msg = msg.arg (failMsgs[i].first).arg (failMsgs[i].second);
          if (i != failMsgs.length () - 1)
            msg += "\n\n";
          realFailMsg += msg;
        }
      QString realMsg = "**%1**:\n\n%2";
      realMsg = realMsg.arg (filename).arg (realFailMsg);
      setErrorText (realMsg);
    }
  Q_EMIT emitResult ();
}

DhAllLoadJob::DhAllLoadJob (QStringList list, QObject *parent)
    : KCompositeJob (parent), cancel_flag (cancel_flag_new ())
{
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
                   ManageRegionUI::notify ();
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
