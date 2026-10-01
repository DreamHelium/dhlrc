#include "saveregionjob.h"

#include "configobjectitems.h"
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

SaveRegionJob::SaveRegionJob (std::shared_ptr<RegionClass> region,
                              const QString &outputDir, SingleTransFunc func,
                              void *configObject, const void *cancelFlag,
                              QObject *parent)
    : KJob (parent), region (std::move (region)), outputDir (outputDir),
      displayNameValue (this->region->displayName ()), func (func),
      configObject (configObject), cancelFlag (cancelFlag),
      /* Lock for the whole job: nothing may rename, remove or modify a region
       * while it is being written. */
      lock (std::make_unique<AutoLocker> (*this->region)),
      helper_struct (helper_struct_new (progressFunc, this, cancelFlag,
                                        DhConfig::elapsedMilliseconds (),
                                        DhConfig::memoryLimit ()),
                     helper_struct_free)
{
}

SaveRegionJob::~SaveRegionJob () = default;

bool
SaveRegionJob::wasCancelled () const
{
  return cancel_flag_is_cancelled (cancelFlag) != 0;
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
          // NOLINTNEXTLINE(bugprone-unused-return-value)
          const auto *msg = func (
              region->get_region (),
              (outputDir + QDir::separator () + displayNameValue).toUtf8 (),
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

void
SaveAllRegionJob::addSave (const QList<std::shared_ptr<RegionClass>> &list,
                           const QString &outputDir, SingleTransFunc func,
                           QLibrary *library, const QString &type,
                           PluginOptionsConfig *pluginOptions)
{
  /* The output options belong to the plugin, not to a single region, so the
   * batch builds one object and hands the same pointer to every job.
   *
   * One rule picks the source:
   *
   * - the switch is on -> this plugin follows the global configuration, so the
   *   saved values are used and nothing is asked;
   * - the switch is off -> the settings are ignored and the user is asked.
   *
   * Only one of the two runs, so the dialog cannot be shown behind an object
   * that was built and then dropped. The batch owns whatever comes back and
   * releases it in `freeConfigObject ()`. */
  configObjectLibrary = library;
  if (pluginOptions && pluginOptions->useConfigured (type))
    {
      configObject = ConfigObjectItems::createObject (
          ConfigObjectItems::Kind::Output, library);
      pluginOptions->apply (type, ConfigObjectItems::Kind::Output,
                            configObject);
    }
  else
    {
      configObject = ConfigObjectUI::getObject (library, CONFIG_OUTPUT);
    }

  for (const auto &region : list)
    {
      const void *flag = cancel_flag_new ();
      cancelFlags.emplace_back (flag, cancel_flag_destroy);

      auto *job
          = new SaveRegionJob (region, outputDir, func, configObject, flag);
      KCompositeJob::addSubjob (job);
      job->setAutoDelete (true);

      auto *widget = new KMessageWidget ();
      /* Give the widget its initial text *before* it is added, so the top area
       * reserves the right height (an empty KMessageWidget has none). */
      widget->setText (
          QString (_ ("Saving %1...")).arg (region->displayName ()));

      /* KMessageWidget already ships a close button that calls
       * `animatedHide ()`. Closing this row means "skip this region", but the
       * same signal also fires when we hide the row on completion, so only
       * treat it as a cancel while the job is still running. */
      auto running = std::make_shared<bool> (true);
      connect (widget, &KMessageWidget::hideAnimationFinished, job,
               [flag, running]
                 {
                   if (*running)
                     cancel_flag_cancel (flag);
                 });

      connect (job, &SaveRegionJob::percentChanged, widget,
               [widget, job]
                 {
                   widget->setText (QString (_ ("Saving %1 (%2%)"))
                                        .arg (job->displayName ())
                                        .arg (job->percent ()));
                 });
      connect (job, &SaveRegionJob::infoMessage, widget,
               [widget, job] (KJob *, const QString &text)
                 {
                   widget->setText (QString (_ ("Saving %1 (%2%): %3"))
                                        .arg (job->displayName ())
                                        .arg (job->percent ())
                                        .arg (text));
                 });

      MainWindow::addWidgetToTopArea (widget);
      connect (job, &SaveRegionJob::result, widget,
               [running] { *running = false; });
      connect (job, &SaveRegionJob::result, widget,
               &KMessageWidget::animatedHide);

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
              if (!hasSubjobs ())
                {
                  freeConfigObject ();
                  deleteLater ();
                }
            });
    }
}

void
SaveAllRegionJob::freeConfigObject ()
{
  if (!configObject)
    return;
  ConfigObjectItems::freeObject (ConfigObjectItems::Kind::Output,
                                 configObjectLibrary, configObject);
  configObject = nullptr;
  configObjectLibrary = nullptr;
}

void
SaveAllRegionJob::start ()
{
  for (const auto &job : subjobs ())
    job->start ();
}
