#include "saveregionjob.h"

#include "configobjectui.h"
#include "mainwindow.h"
#include "manageregionui.h"
#include "settings.h"

#include <KMessageWidget>
#include <QDir>
#include <QTimer>
#include <libintl.h>
#include <qfileinfo.h>
#include <qtconcurrentrun.h>
#define _(str) gettext (str)
#undef asprintf

SaveRegionJob::SaveRegionJob (const QList<std::shared_ptr<RegionClass>> &list,
                              const QString &outputDir, SingleTransFunc func,
                              QLibrary *library, const void *cancel_flag,
                              QObject *parent)
    : KJob (parent), list (list), outputDir (outputDir), func (func),
      library (library), cancel_flag (cancel_flag),
      helper_struct (helper_struct_new (progressFunc, this, cancel_flag,
                                        DhConfig::elapsedMilliseconds (),
                                        DhConfig::memoryLimit ()),
                     helper_struct_free)
{
  /* Hold every region for the whole job so it cannot be renamed, removed or
   * modified while it is being written. */
  for (const auto &region : list)
    locks.emplace_back (std::make_unique<AutoLocker> (*region));
}

SaveRegionJob::~SaveRegionJob () = default;

void
SaveRegionJob::progressFunc (void *main_klass, int value, const char *text,
                             const char *arg)
{
  auto job = static_cast<SaveRegionJob *> (main_klass);
  job->setPercent (value);
  if (arg)
    Q_EMIT job->infoMessage (job, QString::asprintf (gettext (text), arg));
  else
    Q_EMIT job->infoMessage (job, gettext (text));
}

void
SaveRegionJob::setConfigObject (void *object)
{
  std::lock_guard lock (mutex);
  configObject = object;
}

void *
SaveRegionJob::takeConfigObject ()
{
  std::lock_guard lock (mutex);
  return configObject;
}

bool
SaveRegionJob::doResume ()
{
  {
    std::lock_guard lock (mutex);
    resumed = true;
  }
  cv.notify_one ();
  return true;
}

void
SaveRegionJob::forceResume ()
{
  /* Used when the whole export is cancelled: unblock the worker so it can
   * observe the cancel flag and finish. */
  {
    std::lock_guard lock (mutex);
    resumed = true;
  }
  cv.notify_one ();
}

void
SaveRegionJob::start ()
{
  auto realTask = [this]
    {
      const auto total = list.size ();
      int i = 0;
      for (const auto &region : list)
        {
          if (cancel_flag_is_cancelled (cancel_flag))
            break;

          current = region->displayName ();
          if (total > 0)
            setPercent (i * 100 / total);
          Q_EMIT infoMessage (this, _ ("Please click `Continue` to choose "
                                       "the output options."));

          /* Suspend until the owner has collected the output options. The
           * predicate makes sure a `doResume ()` that lands before we reach
           * `wait ()` is not lost. */
          Q_EMIT configureRequested (this);
          std::unique_lock lock (mutex);
          cv.wait (lock, [this] { return resumed; });
          resumed = false;
          lock.unlock ();

          if (cancel_flag_is_cancelled (cancel_flag))
            break;

          // NOLINTNEXTLINE(bugprone-unused-return-value)
          const auto *msg
              = func (region->get_region (),
                      (outputDir + QDir::separator () + current).toUtf8 (),
                      takeConfigObject (), helper_struct.get ());
          if (msg)
            {
              failedList.append (current);
              failedReason.append (msg);
              string_free (msg);
            }
          i++;
          if (total > 0)
            setPercent (i * 100 / total);
        }
      /* Emit from the job's own (GUI) thread: with auto-delete enabled the job
       * must not be destroyed from inside the worker before this lambda
       * returns. */
      QMetaObject::invokeMethod (
          this, [this] { Q_EMIT emitResult (); }, Qt::QueuedConnection);
    };

  future = QtConcurrent::run (std::move (realTask));
}

SaveAllRegionJob::SaveAllRegionJob (QObject *parent)
    : KCompositeJob (parent), cancel_flag (cancel_flag_new ())
{
  connect (this, &SaveAllRegionJob::cancelRequested, this,
           [&]
             {
               cancel_flag_cancel (this->cancel_flag);
               for (const auto &job : this->subjobs ())
                 qobject_cast<SaveRegionJob *> (job)->forceResume ();
             });
}

SaveAllRegionJob::~SaveAllRegionJob () { cancel_flag_destroy (cancel_flag); }

SaveRegionJob *
SaveAllRegionJob::addSave (const QList<std::shared_ptr<RegionClass>> &list,
                           const QString &outputDir, SingleTransFunc func,
                           QLibrary *library)
{
  auto *job = new SaveRegionJob (list, outputDir, func, library, cancel_flag);
  KCompositeJob::addSubjob (job);
  job->setAutoDelete (true);

  auto *widget = new KMessageWidget ();
  /* Give the widget its initial text *before* it is added, so the top area
   * reserves the right height (an empty KMessageWidget has none). The worker
   * has not set `currentRegion ()` yet, so start from a generic label. */
  widget->setText (_ ("Preparing to save..."));
  /* Closing the widget asks the whole export to stop. The flag avoids
   * re-cancelling when the widget is hidden on normal completion. */
  auto finished = std::make_shared<bool> (false);
  connect (widget, &KMessageWidget::hideAnimationFinished, this,
           [this, widget, finished]
             {
               widget->deleteLater ();
               if (!*finished)
                 Q_EMIT this->cancelRequested ();
             });
  MainWindow::addWidgetToTopArea (widget);

  connect (job, &SaveRegionJob::percentChanged, widget,
           [widget, job]
             {
               widget->setText (QString (_ ("Saving %1 (%2%)"))
                                    .arg (job->currentRegion ())
                                    .arg (job->percent ()));
             });
  connect (job, &SaveRegionJob::infoMessage, widget,
           [widget, job] (KJob *, const QString &text)
             {
               widget->setText (QString (_ ("Saving %1 (%2%): %3"))
                                    .arg (job->currentRegion ())
                                    .arg (job->percent ())
                                    .arg (text));
             });
  connect (job, &SaveRegionJob::configureRequested, this,
           [this, widget] (SaveRegionJob *realJob)
             {
               QAction *action = new QAction (_ ("Continue"), widget);
               connect (action, &QAction::triggered, this,
                        [this, realJob, widget]
                          {
                            widget->clearActions ();
                            realJob->setConfigObject (
                                ConfigObjectUI::getObject (
                                    realJob->pluginLibrary (), CONFIG_OUTPUT));
                            realJob->doResume ();
                          });
               widget->addAction (action);
             });
  connect (
      job, &SaveRegionJob::result, this,
      [this, job, widget, finished] (KJob *finishedJob)
        {
          *finished = true;
          if (finishedJob->error () != 0)
            {
              auto *failedWidget = new KMessageWidget ();
              failedWidget->setMessageType (KMessageWidget::Error);
              failedWidget->setTextFormat (Qt::MarkdownText);
              failedWidget->setText (finishedJob->errorText ());
              MainWindow::addWidgetToTopArea (failedWidget);
              connect (failedWidget, &KMessageWidget::hideAnimationFinished,
                       failedWidget, &KMessageWidget::deleteLater);
              QTimer::singleShot (5000, failedWidget,
                                  &KMessageWidget::animatedHide);
            }
          else if (!job->failedRegions ().isEmpty ())
            {
              QString text
                  = _ ("The following regions could not be saved:\n\n");
              const auto regions = job->failedRegions ();
              const auto reasons = job->failedReasons ();
              for (qsizetype i = 0; i < regions.size (); i++)
                {
                  text += "**" + regions.at (i) + "**\n\n" + reasons.at (i)
                          + "\n\n";
                }
              auto *failedWidget = new KMessageWidget ();
              failedWidget->setMessageType (KMessageWidget::Error);
              failedWidget->setTextFormat (Qt::MarkdownText);
              failedWidget->setText (text);
              MainWindow::addWidgetToTopArea (failedWidget);
              connect (failedWidget, &KMessageWidget::hideAnimationFinished,
                       failedWidget, &KMessageWidget::deleteLater);
              QTimer::singleShot (5000, failedWidget,
                                  &KMessageWidget::animatedHide);
            }
          widget->animatedHide ();
          removeSubjob (finishedJob);
          if (!hasSubjobs ())
            deleteLater ();
        });

  return job;
}

void
SaveAllRegionJob::start ()
{
  for (const auto &job : subjobs ())
    job->start ();
}
