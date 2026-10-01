#include "dhloadjob.h"
#include "configobjectitems.h"
#include "configobjectui.h"
#include "pluginoptionsconfig.h"
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

/* The plugin types that can read a file, most likely first.
 *
 * Every candidate shares the object codec that was used to decode the file, so
 * the order only says which of them to try first. It is decided by the suffix:
 *
 * - a suffix that names a plugin puts that plugin first;
 * - a missing or unknown suffix puts nothing first, and the order is then just
 *   the module order, which is arbitrary but stable.
 *
 * Callers must not treat the first entry as authoritative: with no suffix
 * there is no "expected" type, so `matched` reports whether the suffix
 * actually identified one. That is what the warning about "the file type
 * should be X" must key off, rather than whichever entry happens to be first.
 */
struct PluginCandidates
{
  QList<QString> types;
  bool matched = false;
};

static PluginCandidates
candidatesFor (const QString &filename, const QString &baseType)
{
  /* Every plugin that understands the codec this file decoded to. */
  QList<QString> all;
  for (auto *module : ManageRegionUI::getModules ())
    {
      if (module->baseType () == baseType)
        all << module->type ();
    }

  PluginCandidates result;

  /* Only look at the suffix when the user asked for that; ignoring it is the
   * whole point of turning the option off. */
  if (!DhConfig::loadingFileByExtension ())
    {
      result.types = all;
      return result;
    }

  auto extension = QFileInfo (filename).suffix ();
  QString matchedType;
  for (auto *module : ManageRegionUI::getModules ())
    {
      if (module->baseType () != baseType || extension.isEmpty ())
        continue;
      if (module->fileSuffix () == extension)
        {
          matchedType = module->type ();
          break;
        }
    }

  if (matchedType.isEmpty ())
    {
      /* No suffix, or one no plugin claims. There is no expected type, so the
       * order is the module order and every candidate is worth trying. */
      result.types = all;
      return result;
    }

  /* The suffix named a plugin, so it goes first. Whether the others are tried
   * after it failing is what the retry option decides. */
  result.matched = true;
  result.types << matchedType;
  if (DhConfig::failThenRetry ())
    {
      for (const auto &type : all)
        {
          if (type != matchedType)
            result.types << type;
        }
    }
  return result;
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
                    /* This file was cancelled: stop before decoding it. */
                    if (cancel_flag_is_cancelled (cancel_flag))
                      throw DhLoadError (_ ("Cancelled."), _ ("Loading File"));

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
                    auto candidates
                        = candidatesFor (filename, tempObject.first);
                    DhMultiLoadError err;
                    for (const auto &type : candidates.types)
                      {
                        auto *base = ManageRegionUI::getModule (type);
                        if (!base)
                          continue;
                        /* The plugin's own options, if the batch resolved any
                         * for it. Looking them up by type is what makes a
                         * fallback safe: each entry was built by that plugin's
                         * own `*_config_new ()`, so no plugin ever receives
                         * another one's struct. */
                        auto configEntry = inputConfigs.find (type);
                        auto *config = configEntry == inputConfigs.end ()
                                           ? nullptr
                                           : configEntry->second;
                        bool loaded = false;
                        if (auto *multi = base->multi ())
                          loaded = loadMultiRegion (
                              multi, tempObject.second.get (), err, config);
                        else if (auto *single = base->single ())
                          {
                            void *region = nullptr;
                            auto msg = single->loadFunc (
                                tempObject.second.get (), &region,
                                helper_struct.get (), config);
                            if (msg)
                              {
                                err.appendError (msg,
                                                 loadingTypePrefix (type));
                                string_free (msg);
                                continue;
                              }
                            /* GUI label: the file name without the
                             * extension. The region's own name comes from the
                             * file itself. */
                            auto displayName
                                = QFileInfo (filename).completeBaseName ();
                            ManageRegionUI::appendRegion (region, displayName);
                            loaded = true;
                          }
                        if (!loaded)
                          continue;

                        /* The suffix named a plugin and a different one had to
                         * be used: worth saying, since the file is then not
                         * quite what its name says. With no suffix there was
                         * never an expectation, so nothing is reported. */
                        if (candidates.matched
                            && type != candidates.types.first ())
                          {
                            QString message
                                = _ ("Loading progress is successful, but the "
                                     "file type should be %1.");
                            message = message.arg (candidates.types.first ());
                            err.appendError (message,
                                             loadingTypePrefix (type));
                          }
                        typeName = type;
                        break;
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

DhLoadJob::~DhLoadJob () = default;

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
                            DhMultiLoadError &err, void *inputConfig)
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
                                      helper_struct.get (), inputConfig);
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
    : KCompositeJob (parent)
{
  connect (this, &DhAllLoadJob::cancel, this,
           [&]
             {
               /* Cancel every file, including the ones already queued. */
               for (const auto &flag : cancelFlags)
                 cancel_flag_cancel (flag.get ());
               for (const auto &job : this->subjobs ())
                 qobject_cast<DhLoadJob *> (job)->forceResume ();
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
      /* Each file gets its own cancel flag, so one can be aborted without
       * touching the others. It is owned by `cancelFlags` below. */
      const void *fileFlag = cancel_flag_new ();
      cancelFlags.emplace_back (fileFlag, cancel_flag_destroy);

      /* The plugin that will read the file is not known until it has been
       * decoded, so the options cannot wait for it. Instead every candidate
       * plugin gets its own object, resolved on the GUI thread (the dialog
       * cannot be shown from the worker that does the reading):
       *
       * - a plugin that follows the configuration has one built from the saved
       *   values;
       * - one that does not is asked about;
       * - one that offers no options has none, and is not asked about.
       *
       * A missing suffix is therefore no longer special: there is simply no
       * preferred candidate, and each plugin the file might belong to already
       * has the settings the user chose for it.
       *
       * Objects are keyed by type and shared, so a batch that mixes formats —
       * or holds ten files of one format — resolves each plugin's options
       * once. */
      registerInputConfigs ();

      auto job = new DhLoadJob (filename, fileFlag, inputConfigs);
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
                       freeInputConfig ();
                       this->messageWidget->deleteLater ();
                       deleteLater ();
                     }
                 });
      job->messageWidget = new KMessageWidget ();
      connect (job->messageWidget, &KMessageWidget::hideAnimationFinished,
               job->messageWidget, &KMessageWidget::deleteLater);
      MainWindow::addWidgetToTopArea (job->messageWidget);
      /* KMessageWidget already ships a close button that calls
       * `animatedHide ()`. Closing this row means "abort this file", which
       * also has to unblock a job suspended waiting for an answer. The same
       * signal fires when we hide the row on completion, so only treat it as a
       * cancel while the job is still running. */
      auto running = std::make_shared<bool> (true);
      connect (job->messageWidget, &KMessageWidget::hideAnimationFinished, job,
               [fileFlag, job, running]
                 {
                   if (!*running)
                     return;
                   cancel_flag_cancel (fileFlag);
                   Q_EMIT job->forceResumeRequested ();
                 });
      connect (job, &DhLoadJob::result, job, [running] { *running = false; });
      connect (job, &DhLoadJob::forceResumeRequested, job,
               [job] { job->forceResume (); });
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

void
DhAllLoadJob::registerInputConfigs ()
{
  /* Every plugin, not every candidate for one file: the batch may hold several
   * formats and the codec is unknown until each file is decoded, so this is
   * the only point where the set is known. Resolving a plugin that never ends
   * up being used costs nothing beyond a settings lookup. */
  for (auto *module : ManageRegionUI::getModules ())
    acquireInputConfig (module->type ());
}

void *
DhAllLoadJob::acquireInputConfig (const QString &type)
{
  /* One object per plugin type, reused for every file of that type in this
   * batch. That is what keeps a mixed batch working (each type gets its own)
   * while still asking only once per plugin. */
  auto known = inputConfigs.find (type);
  if (known != inputConfigs.end ())
    return known->second;

  void *object = nullptr;
  auto *module = ManageRegionUI::getModule (type);
  auto *config = PluginOptionsConfig::instance ();
  if (module && config
      && config->hasOptions (type, ConfigObjectItems::Kind::Input))
    {
      if (config->useConfigured (type))
        {
          /* Follows the global configuration: the saved values are used and
           * nothing is asked. */
          object = ConfigObjectItems::createObject (
              ConfigObjectItems::Kind::Input, module->library ());
          config->apply (type, ConfigObjectItems::Kind::Input, object);
        }
      else
        {
          /* Asked here rather than in the job: this runs on the GUI thread,
           * while the reading happens on a worker, and a modal dialog from
           * there would be undefined behaviour. */
          object
              = ConfigObjectUI::getObject (module->library (), CONFIG_INPUT);
        }
    }

  inputConfigs[type] = object;
  if (!object)
    {
      /* Nothing to release later, so it does not need an entry. */
      inputConfigs.erase (type);
    }
  return object;
}

void
DhAllLoadJob::freeInputConfig ()
{
  /* Every object this batch built, once the last job is done and no plugin can
   * still be holding one. */
  for (const auto &[type, object] : inputConfigs)
    {
      if (!object)
        continue;
      auto *module = ManageRegionUI::getModule (type);
      ConfigObjectItems::freeObject (ConfigObjectItems::Kind::Input,
                                     module ? module->library () : nullptr,
                                     object);
    }
  inputConfigs.clear ();
}

DhAllLoadJob::~DhAllLoadJob () = default;

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
