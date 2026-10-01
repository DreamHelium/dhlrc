#include "saveregionjob.h"

#include "configobjectitems.h"
#include "configobjectui.h"
#include "mainwindow.h"
#include "manageregionui.h"
#include "settings.h"

#include <KMessageWidget>
#include <QDebug>
#include <QDir>
#include <QTimer>
#include <libintl.h>
#include <qfileinfo.h>
#include <qtconcurrentrun.h>
#define _(str) gettext (str)
#undef asprintf

SaveRegionJob::SaveRegionJob (std::shared_ptr<RegionClass> region,
                              const QString &outputDir, const QString &suffix,
                              SingleTransFunc func, void *configObject,
                              const void *cancelFlag, QObject *parent)
    : KJob (parent), region (std::move (region)), outputDir (outputDir),
      fileSuffix (suffix), displayNameValue (this->region->displayName ()),
      func (func), configObject (configObject), cancelFlag (cancelFlag),
      /* Lock for the whole job: nothing may rename, remove or modify a region
       * while it is being written. */
      lock (std::make_unique<AutoLocker> (*this->region)),
      helper_struct (helper_struct_new (progressFunc, this, cancelFlag,
                                        DhConfig::elapsedMilliseconds (),
                                        DhConfig::memoryLimit ()),
                     helper_struct_free)
{
  /* The row is built here, at construction, exactly as `DhLoadJob` does: it
   * then exists from the moment the job does, independently of when `start ()`
   * is called. Building it later is what made it possible for a region's row
   * to never appear at all. */
  messageWidget = new KMessageWidget ();
  messageWidget->setText (QString (_ ("Saving %1...")).arg (displayNameValue));
  MainWindow::addWidgetToTopArea (messageWidget);

  /* Updating the row is the job's own business, rather than a lambda in the
   * composite: one place to change, and no chance of the two disagreeing. */
  connect (this, &SaveRegionJob::percentChanged, this,
           [this]
             {
               if (messageWidget)
                 messageWidget->setText (QString (_ ("Saving %1 (%2%)"))
                                             .arg (displayNameValue)
                                             .arg (percent ()));
             });
  connect (this, &SaveRegionJob::infoMessage, this,
           [this] (KJob *, const QString &text)
             {
               if (messageWidget)
                 messageWidget->setText (QString (_ ("Saving %1 (%2%): %3"))
                                             .arg (displayNameValue)
                                             .arg (percent ())
                                             .arg (text));
             });
}

SaveRegionJob::~SaveRegionJob () = default;

bool
SaveRegionJob::wasCancelled () const
{
  return cancel_flag_is_cancelled (cancelFlag) != 0;
}

void
SaveRegionJob::forceResume ()
{
  cv.notify_one ();
}

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
SaveRegionJob::start ()
{
  auto realTask = [this]
    {
      /* The flag can already be set when the user pressed Cancel while the job
       * was queued. */
      if (!wasCancelled ())
        {
          /* Parked until the user has answered, when the plugin does not
           * follow the configuration. The row is already on screen — it was
           * built when this job was — so "Click Continue" refers to something
           * the user can see. */
          if (needsOptions)
            {
              Q_EMIT selfSuspended (this);
              std::unique_lock lock (mutex);
              cv.wait (lock);
              Q_EMIT selfResumed (this);

              /* Woken by closing the row rather than by answering: the write
               * is called off. `requestOptions ()` is the only path that
               * fills `configObject`. */
              if (cancel_flag_is_cancelled (cancelFlag) != 0)
                {
                  QMetaObject::invokeMethod (
                      this, [this] { Q_EMIT emitResult (); },
                      Qt::QueuedConnection);
                  return;
                }
            }

          /* The plugin writes exactly the name it is given and adds nothing,
           * so the suffix comes from the module here. Without it the file has
           * no extension for the loader to match on next time. */
          auto name = displayNameValue;
          if (!fileSuffix.isEmpty ()
              && !name.endsWith (QStringLiteral (".") + fileSuffix,
                                 Qt::CaseInsensitive))
            name += QStringLiteral (".") + fileSuffix;

          // NOLINTNEXTLINE(bugprone-unused-return-value)
          const auto *msg
              = func (region->get_region (),
                      (outputDir + QDir::separator () + name).toUtf8 (),
                      configObject, helper_struct.get ());
          if (msg)
            {
              failure = QString::fromUtf8 (msg);
              string_free (msg);
            }
          else
            {
              setPercent (100);
            }
        }

      /* The lock is released here, before the result is announced, and not by
       * the destructor.
       *
       * `AutoLocker` unlocks through `RegionClass::unlock ()`, which emits
       * `lockedChanged`; `ManageRegionUI` answers that by rebuilding the item
       * frames. With `removeSubjob ()` deleting this job from inside its own
       * `result` emission, that rebuild would run in the middle of the signal
       * dispatch and could pull the emitting widget out from under it. Letting
       * go of the lock first keeps the rebuild in this queued slot, which is a
       * safe point. */
      lock.reset ();

      /* Emit from the job's own (GUI) thread: with auto-delete enabled the job
       * must not be destroyed from inside the worker before this lambda
       * returns. */
      QMetaObject::invokeMethod (
          this, [this] { Q_EMIT emitResult (); }, Qt::QueuedConnection);
    };

  future = QtConcurrent::run (std::move (realTask));
}

SaveAllRegionJob::SaveAllRegionJob (QObject *parent) : KCompositeJob (parent)
{
}

SaveAllRegionJob::~SaveAllRegionJob () = default;

void *
SaveAllRegionJob::resolveOutputConfig (const QString &type, QLibrary *library,
                                       PluginOptionsConfig *pluginOptions,
                                       bool *cancelled)
{
  if (cancelled)
    *cancelled = false;

  if (!library)
    return nullptr;

  /* The switch decides where the options come from, and only one of the two
   * branches runs:
   *
   * - on: this plugin follows the global configuration, so the saved values
   *   are used and nothing is asked;
   * - off: the settings are ignored and the user is asked, here, on the GUI
   *   thread, before any job exists. */
  if (pluginOptions
      && pluginOptions->useConfigured (type, ConfigObjectItems::Kind::Output))
    {
      auto *object = ConfigObjectItems::createObject (
          ConfigObjectItems::Kind::Output, library);
      pluginOptions->apply (type, ConfigObjectItems::Kind::Output, object);
      return object;
    }

  return ConfigObjectUI::getObject (library, CONFIG_OUTPUT, true, cancelled);
}

void
SaveAllRegionJob::setupOverallWidget ()
{
  if (overallWidget)
    return;

  /* The batch's own line, above the per-region ones, so a write of many files
   * has a single figure that reaches 100% instead of several rows that each
   * finish on their own. Created before the first region is added, so the top
   * area reserves its height. */
  overallWidget = new KMessageWidget ();
  /* Keyed by `nullptr`: closing this row calls off every region, unlike the
   * rows below it that each stop one. */
  registerCancelRow (overallWidget, nullptr);
  overallWidget->setText (QString (_ ("Saved %1 of %2 (%3%)."))
                              .arg (finishedJobs)
                              .arg (jobNums)
                              .arg (percent ()));
  MainWindow::addWidgetToTopArea (overallWidget);
}

void
SaveAllRegionJob::registerCancelRow (KMessageWidget *widget, const void *flag)
{
  /* Closing a row is how the user calls a write off. The close button hides
   * the row through `animatedHide ()`, and `hideAnimationFinished` is the
   * signal that reports exactly that — emitted once the row is hidden, and
   * emitted immediately when animations are disabled.
   *
   * The signal rather than a `Hide` event: a row that has just been added is
   * hidden and shown again while Qt settles the layout, and a plain `Hide`
   * there would look like the user closing a row that was never on screen.
   *
   * The connection is owned by the composite, not by the row. A row outlives
   * the composite when the batch is torn down on the last region finishing,
   * and a row-owned connection would then call back into a destroyed composite
   * — which is where this crashed. */
  QPointer<KMessageWidget> guard = widget;
  rows.push_back (
      { widget, flag,
        connect (widget, &KMessageWidget::hideAnimationFinished, this,
                 [this, guard]
                   {
                     if (guard)
                       cancelRow (guard);
                   }) });
}

void
SaveAllRegionJob::unregisterCancelRow (KMessageWidget *widget)
{
  /* The row is forgotten and its connection cut, so hiding it on completion
   * (which emits the same signal) does not come back as a cancel. */
  for (auto it = rows.begin (); it != rows.end (); ++it)
    {
      if (it->widget != widget)
        continue;
      QObject::disconnect (it->connection);
      rows.erase (it);
      break;
    }
}

void
SaveAllRegionJob::resumeAll ()
{
  for (const auto &subjob : subjobs ())
    {
      if (auto *single = qobject_cast<SaveRegionJob *> (subjob))
        single->forceResume ();
      else if (auto *multi = qobject_cast<SaveMultiRegionJob *> (subjob))
        multi->forceResume ();
    }
}

void
SaveAllRegionJob::resumeFlag (const void *flag)
{
  /* Both kinds of subjob park on the same signal, so both have to be looked
   * at: casting only to `SaveRegionJob` left a parked multi-file write waiting
   * forever, because closing its row could never reach it. */
  for (const auto &subjob : subjobs ())
    {
      if (auto *single = qobject_cast<SaveRegionJob *> (subjob);
          single && single->cancels (flag))
        single->forceResume ();
      else if (auto *multi = qobject_cast<SaveMultiRegionJob *> (subjob);
               multi && multi->cancels (flag))
        multi->forceResume ();
    }
}

void
SaveAllRegionJob::cancelRow (QWidget *widget)
{
  const void *flag = nullptr;
  bool found = false;
  for (const auto &row : rows)
    {
      if (row.widget == widget)
        {
          flag = row.flag;
          found = true;
          break;
        }
    }
  if (!found)
    return;

  if (flag)
    {
      /* One region's row: stop that region and let go of that worker only.
       * Waking the others would take them past their park without the options
       * having been answered for them — `requestOptions ()` is only ever run
       * for the row that was clicked — and they would write with nothing. */
      cancel_flag_cancel (flag);
      resumeFlag (flag);
      return;
    }

  /* The batch's own row stops everything, so every parked worker is let go to
   * see the flag and unwind. */
  for (const auto &f : cancelFlags)
    cancel_flag_cancel (f.get ());
  resumeAll ();
}

void
SaveAllRegionJob::addSave (const QList<std::shared_ptr<RegionClass>> &list,
                           const QString &outputDir, const QString &suffix,
                           SingleTransFunc func, const QString &type,
                           QLibrary *library)
{
  if (list.isEmpty ())
    return;

  configObjectLibrary = library;
  jobNums = list.size ();
  setupOverallWidget ();

  /* Whether the user has to settle the options for each file. Asked per file,
   * since each file is its own save: one answer per region, not one for the
   * batch. */
  const bool ask = needsOptions (type);

  /* Following the configuration: one object is built from the saved values and
   * shared, since nothing about it could differ between the files. */
  if (!ask)
    {
      auto *object = ConfigObjectItems::createObject (
          ConfigObjectItems::Kind::Output, library);
      PluginOptionsConfig::instance ()->apply (
          type, ConfigObjectItems::Kind::Output, object);
      ownConfigObject (object, library);
    }

  /* One row and one job per region, built here in one pass exactly as
   * `DhAllLoadJob` builds them: nothing waits for a later callback, so every
   * row exists from the moment the write is set up and cannot be missed. Each
   * job is handed its own row, which it fills in as it goes. */
  for (const auto &region : list)
    {
      const void *flag = cancel_flag_new ();
      cancelFlags.emplace_back (flag, cancel_flag_destroy);

      auto *job = new SaveRegionJob (region, outputDir, suffix, func,
                                     configObject, flag);
      job->setAutoDelete (true);
      KCompositeJob::addSubjob (job);

      /* Each job asks for its own object when there is something to ask. */
      job->needsOptions = ask;
      job->requestOptions
          = [this, type, library] { return askForOptions (type, library); };

      /* Closing this row skips that region. */
      registerCancelRow (job->messageWidget, flag);
      /* The row is detached before it is hidden on completion, so a region
       * finishing is not read as a cancel of the others. The hide is a pass
       * late so a fast write does not take its row away before it is painted,
       * and it holds a `QPointer`: the row may be gone by the time the timer
       * fires (the last region finishing tears the whole batch down), and a
       * raw pointer there would be a use-after-free. */
      connect (job, &SaveRegionJob::result, this,
               [this, job]
                 {
                   unregisterCancelRow (job->messageWidget);
                   if (job->messageWidget)
                     {
                       QPointer<KMessageWidget> row = job->messageWidget;
                       QTimer::singleShot (0, row,
                                           [row]
                                             {
                                               if (row)
                                                 row->animatedHide ();
                                             });
                     }
                 });

      connect (
          job, &SaveRegionJob::result, this,
          [this, job] (KJob *finishedJob)
            {
              /* A cancelled region is simply skipped: nothing to report. */
              if (!job->wasCancelled () && !job->failureReason ().isEmpty ())
                {
                  auto *failedWidget = new KMessageWidget ();
                  failedWidget->setMessageType (KMessageWidget::Error);
                  failedWidget->setTextFormat (Qt::MarkdownText);
                  failedWidget->setText (
                      QString (_ ("Region %1 could not be saved:\n\n%2"))
                          .arg (job->displayName (), job->failureReason ()));
                  MainWindow::addWidgetToTopArea (failedWidget);
                  connect (failedWidget,
                           &KMessageWidget::hideAnimationFinished,
                           failedWidget, &KMessageWidget::deleteLater);
                  QTimer::singleShot (5000, failedWidget,
                                      &KMessageWidget::animatedHide);
                }

              removeSubjob (finishedJob);
              finishedJobs += 1;
              if (overallWidget)
                {
                  if (jobNums > 0)
                    setPercent (finishedJobs * 100 / jobNums);
                  overallWidget->setText (QString (_ ("Saved %1 of %2 (%3%)."))
                                              .arg (finishedJobs)
                                              .arg (jobNums)
                                              .arg (percent ()));
                }
              if (!hasSubjobs ())
                {
                  freeConfigObject ();
                  if (overallWidget)
                    overallWidget->animatedHide ();
                  deleteLater ();
                }
            });

      /* The worker has parked, waiting to be told to go ahead. A `Continue`
       * action on the row is that permission, exactly as in `DhAllLoadJob`. */
      connect (job, &SaveRegionJob::selfSuspended, this,
               [this, job, type, library] (KJob *)
                 {
                   if (!job->messageWidget)
                     return;
                   job->messageWidget->setText (
                       QString (_ ("Ready to save %1. Click Continue to set "
                                   "the options."))
                           .arg (job->displayName ()));
                   auto *action = new QAction (_ ("Continue"), job);
                   connect (action, &QAction::triggered, job,
                            [this, job, action, type, library]
                              {
                                action->deleteLater ();
                                /* The window is opened here, on the GUI
                                 * thread. `askForOptions ()` hands back the
                                 * object the batch now owns, and the job is
                                 * given it before it is let go: it reads its
                                 * own object the moment it wakes. */
                                auto *object = askForOptions (type, library);
                                ownConfigObject (object, library);
                                job->adoptConfigObject (object);
                                /* Only this region is released. Each file is
                                 * its own save with its own settings, so
                                 * releasing the rest would write them with
                                 * answers meant for someone else. */
                                job->forceResume ();
                              });
                   job->messageWidget->addAction (action);
                 });
      /* The action goes once the answer is in, so the row cannot be resumed
       * twice. */
      connect (job, &SaveRegionJob::selfResumed, this,
               [job] (KJob *)
                 {
                   if (job->messageWidget)
                     job->messageWidget->clearActions ();
                 });
    }
}

void
SaveAllRegionJob::addSaveIntoMulti (
    const QList<std::shared_ptr<RegionClass>> &list, const QString &filename,
    const QString &suffix, QLibrary *library, const QString &type)
{
  if (list.isEmpty ())
    return;

  auto *module = ManageRegionUI::getModule (type);
  auto *multi = module ? module->multi () : nullptr;
  if (!multi || !multi->multiSaveFunc)
    {
      /* Silence here once cost a long debugging session: the caller believed
       * it had asked for a single-file write and the regions quietly came out
       * as one file each instead. */
      qWarning () << "SaveAllRegionJob: " << type
                  << "offers no multi-region writer, falling back to one file "
                     "per region";
      return;
    }

  configObjectLibrary = library;
  /* One file means the batch's figure is simply whether that file is done. */
  jobNums = 1;
  setupOverallWidget ();

  const bool ask = needsOptions (type);
  if (!ask)
    {
      /* Following the configuration: the object comes from the saved values.
       */
      ownConfigObject (ConfigObjectItems::createObject (
                           ConfigObjectItems::Kind::Output, library),
                       library);
      PluginOptionsConfig::instance ()->apply (
          type, ConfigObjectItems::Kind::Output, configObject);
    }

  const void *flag = cancel_flag_new ();
  cancelFlags.emplace_back (flag, cancel_flag_destroy);

  /* The suffix is appended once for the whole file, since the plugin writes
   * the name it is given and adds nothing. */
  auto realName = filename;
  if (!suffix.isEmpty ()
      && !realName.endsWith (QStringLiteral (".") + suffix,
                             Qt::CaseInsensitive))
    realName += QStringLiteral (".") + suffix;

  auto *job = new SaveMultiRegionJob (list, realName, multi->multiSaveFunc,
                                      configObject, flag);
  job->setAutoDelete (true);
  KCompositeJob::addSubjob (job);
  job->setProperty ("count", list.size ());
  job->needsOptions = ask;

  /* The row is built by the job, as for the per-region form. */
  registerCancelRow (job->messageWidget, flag);

  /* The worker parks until `Continue` is clicked, exactly as the per-region
   * form does; the row already exists, so there is something to click. */
  connect (job, &SaveMultiRegionJob::selfSuspended, this,
           [this, job, type, library] (KJob *)
             {
               if (!job->messageWidget)
                 return;
               job->messageWidget->setText (
                   QString (_ ("Ready to save %1 region(s). Click Continue to "
                               "set the options."))
                       .arg (job->property ("count").toInt ()));
               auto *action = new QAction (_ ("Continue"), job);
               connect (action, &QAction::triggered, job,
                        [this, job, action, type, library]
                          {
                            action->deleteLater ();
                            /* One file, one set of options, so there is
                             * nothing to share and nothing to ask twice. */
                            job->adoptConfigObject (ownConfigObject (
                                askForOptions (type, library), library));
                            job->forceResume ();
                          });
               job->messageWidget->addAction (action);
             });
  connect (job, &SaveMultiRegionJob::selfResumed, this,
           [job] (KJob *)
             {
               if (job->messageWidget)
                 job->messageWidget->clearActions ();
             });
  connect (job, &SaveMultiRegionJob::result, this,
           [this, job]
             {
               unregisterCancelRow (job->messageWidget);
               if (auto *row = job->messageWidget.data ())
                 QTimer::singleShot (0, row, [row] { row->animatedHide (); });
             });

  /* The batch's figure follows the file's own progress: there is only one, so
   * it moves with it. */
  connect (job, &SaveMultiRegionJob::percentChanged, this,
           [this, job]
             {
               if (!overallWidget)
                 return;
               setPercent (job->percent ());
               overallWidget->setText (
                   QString (_ ("Saved %1 of %2 (%3%)."))
                       .arg (job->percent () >= 100 ? 1 : 0)
                       .arg (1)
                       .arg (job->percent ()));
             });

  connect (job, &SaveMultiRegionJob::result, this,
           [this, job, realName] (KJob *)
             {
               if (!job->wasCancelled () && !job->failureReason ().isEmpty ())
                 {
                   auto *failedWidget = new KMessageWidget ();
                   failedWidget->setMessageType (KMessageWidget::Error);
                   failedWidget->setTextFormat (Qt::MarkdownText);
                   failedWidget->setText (
                       QString (_ ("The file %1 could not be written:\n\n%2"))
                           .arg (realName, job->failureReason ()));
                   MainWindow::addWidgetToTopArea (failedWidget);
                   connect (failedWidget,
                            &KMessageWidget::hideAnimationFinished,
                            failedWidget, &KMessageWidget::deleteLater);
                   QTimer::singleShot (5000, failedWidget,
                                       &KMessageWidget::animatedHide);
                 }

               removeSubjob (job);
               if (!hasSubjobs ())
                 {
                   freeConfigObject ();
                   if (overallWidget)
                     overallWidget->animatedHide ();
                   deleteLater ();
                 }
             });
}

SaveMultiRegionJob::SaveMultiRegionJob (
    const QList<std::shared_ptr<RegionClass>> &list, const QString &filename,
    MultiTransFunc func, void *configObject, const void *cancelFlag,
    QObject *parent)
    : KJob (parent), regions (list), filename (filename), func (func),
      configObject (configObject), cancelFlag (cancelFlag),
      helper_struct (helper_struct_new (progressFunc, this, cancelFlag,
                                        DhConfig::elapsedMilliseconds (),
                                        DhConfig::memoryLimit ()),
                     helper_struct_free)
{
  /* Every region is locked for the whole write: it is one file, so it cannot
   * be half-written while a region in it is renamed or removed. */
  locks.reserve (regions.size ());
  for (const auto &region : regions)
    locks.emplace_back (std::make_unique<AutoLocker> (*region));

  /* The row belongs to the job, built at construction like `DhLoadJob`'s. */
  messageWidget = new KMessageWidget ();
  messageWidget->setText (
      QString (_ ("Saving %1 region(s)...")).arg (regions.size ()));
  MainWindow::addWidgetToTopArea (messageWidget);

  const auto count = regions.size ();
  connect (this, &SaveMultiRegionJob::percentChanged, this,
           [this, count]
             {
               if (messageWidget)
                 messageWidget->setText (
                     QString (_ ("Saving %1 region(s) (%2%)"))
                         .arg (count)
                         .arg (percent ()));
             });
  connect (this, &SaveMultiRegionJob::infoMessage, this,
           [this, count] (KJob *, const QString &text)
             {
               if (messageWidget)
                 messageWidget->setText (
                     QString (_ ("Saving %1 region(s) (%2%): %3"))
                         .arg (count)
                         .arg (percent ())
                         .arg (text));
             });
}

SaveMultiRegionJob::~SaveMultiRegionJob () = default;

bool
SaveMultiRegionJob::wasCancelled () const
{
  return cancel_flag_is_cancelled (cancelFlag) != 0;
}

void
SaveMultiRegionJob::progressFunc (void *main_klass, int value,
                                  const char *text, const char *arg)
{
  auto job = static_cast<SaveMultiRegionJob *> (main_klass);
  job->setPercent (value);
  if (arg)
    Q_EMIT job->infoMessage (job, QString::asprintf (gettext (text), arg));
  else
    Q_EMIT job->infoMessage (job, gettext (text));
}

void
SaveMultiRegionJob::forceResume ()
{
  cv.notify_one ();
}

void
SaveMultiRegionJob::start ()
{
  auto realTask = [this]
    {
      if (!wasCancelled ())
        {
          /* Parked until the user has answered; see `SaveRegionJob::start ()`.
           */
          if (needsOptions)
            {
              Q_EMIT selfSuspended (this);
              std::unique_lock lock (mutex);
              cv.wait (lock);
              Q_EMIT selfResumed (this);
              if (cancel_flag_is_cancelled (cancelFlag) != 0)
                {
                  QMetaObject::invokeMethod (
                      this, [this] { Q_EMIT emitResult (); },
                      Qt::QueuedConnection);
                  return;
                }
            }

          /* The plugin is handed the regions as one array of pointers, which
           * is the shape `region_save_into_multi` expects. */
          std::vector<void *> raw;
          raw.reserve (regions.size ());
          for (const auto &region : regions)
            raw.push_back (region->get_region ());

          auto *msg = func (raw.data (), raw.size (),
                            filename.toUtf8 ().constData (), configObject,
                            helper_struct.get ());
          if (msg)
            {
              failure = QString::fromUtf8 (msg);
              string_free (msg);
            }
          else
            {
              setPercent (100);
            }
        }

      /* The locks go before the result is announced, for the same reason as in
       * `SaveRegionJob`: unlocking emits `lockedChanged`, which rebuilds the
       * item frames, and that must not happen inside this job's own `result`
       * dispatch. */
      locks.clear ();

      /* Emitted from the job's own thread: with auto-delete enabled the job
       * must not be destroyed from inside the worker before this returns. */
      QMetaObject::invokeMethod (
          this, [this] { Q_EMIT emitResult (); }, Qt::QueuedConnection);
    };

  future = QtConcurrent::run (std::move (realTask));
}

void
SaveAllRegionJob::freeConfigObject ()
{
  /* Every object the batch built: one per file that was configured, plus the
   * one built from the saved settings when the plugin follows them. */
  for (const auto &[object, library] : ownedConfigObjects)
    ConfigObjectItems::freeObject (ConfigObjectItems::Kind::Output, library,
                                   object);
  ownedConfigObjects.clear ();
  configObject = nullptr;
  configObjectLibrary = nullptr;
}

bool
SaveAllRegionJob::needsOptions (const QString &type) const
{
  auto *options = PluginOptionsConfig::instance ();
  if (!options)
    return false;
  /* Asked only when the plugin is not following the configuration and there is
   * something to configure; a plugin with no options has nothing to answer. */
  return !options->useConfigured (type, ConfigObjectItems::Kind::Output)
         && options->hasOptions (type, ConfigObjectItems::Kind::Output);
}

void *
SaveAllRegionJob::askForOptions (const QString &type, QLibrary *library)
{
  /* Runs on the GUI thread, from the Continue click: the options window is
   * modal and must not be shown from the thread pool.
   *
   * A fresh object every time, because each file is its own save and so has
   * its own settings. Handing out one shared object meant the first answer was
   * quietly applied to every file, which is not what a per-file save offers.
   */
  bool cancelled = false;
  auto *object = resolveOutputConfig (
      type, library, PluginOptionsConfig::instance (), &cancelled);
  if (cancelled)
    {
      ConfigObjectItems::freeObject (ConfigObjectItems::Kind::Output, library,
                                     object);
      return nullptr;
    }
  return object;
}

void *
SaveAllRegionJob::ownConfigObject (void *object, QLibrary *library)
{
  /* The objects are owned here, not by the jobs that use them: a job is
   * deleted by `removeSubjob ()` the moment it finishes, and the object has to
   * outlive the worker using it. */
  if (!object)
    return nullptr;
  if (!configObject)
    {
      /* Kept as well, so the batch can be released in one place even when no
       * window was ever shown. */
      configObject = object;
      configObjectLibrary = library;
    }
  ownedConfigObjects.push_back ({ object, library });
  return object;
}

void
SaveAllRegionJob::start ()
{
  /* The rows and the subjobs were built by `addSave ()` / `addSaveIntoMulti
   * ()`, so this only has to set them going. */
  for (const auto &job : subjobs ())
    job->start ();
}
