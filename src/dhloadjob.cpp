#include "dhloadjob.h"
#include "configobjectitems.h"
#include "configobjectui.h"
#include "dhcore.h"
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
 * the order only says which of them to try first.
 *
 * The file extension decides: the module that claims it is the expected reader,
 * and `matched` says whether the extension actually named one. The other
 * modules follow when the retry option is on, and whenever no module claims the
 * extension — there is no expectation then, so every module that understands
 * the codec is worth a try. A file that loads through a different module than
 * its extension named is not an error: the job reports it once the load is
 * done, so the user is told without losing the file.
 */
struct PluginCandidates
{
  QList<QString> types;
  bool matched = false;
};

/* The module whose `fileSuffix ()` matches the file's extension and that
 * understands `baseType`, or an empty string. */
static QString
moduleForSuffix (const QString &filename, const QString &baseType)
{
  const auto extension = QFileInfo (filename).suffix ();
  if (extension.isEmpty ())
    return {};
  for (auto *module : ManageRegionUI::getModules ())
    {
      if (module->baseType () == baseType && module->fileSuffix () == extension)
        return module->type ();
    }
  return {};
}

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
  const auto matchedType = moduleForSuffix (filename, baseType);
  if (matchedType.isEmpty ())
    {
      /* No suffix, or one no plugin claims: there is no expected type, so the
       * order is the module order and every candidate is worth trying. */
      result.types = all;
      return result;
    }

  /* The suffix named a plugin, so it goes first. Whether the others are tried
   * after it failing is what the retry option decides. */
  result.matched = true;
  result.types << matchedType;
  if (DhCore::instance ()->config ().failThenRetry)
    {
      for (const auto &type : all)
        {
          if (type != matchedType)
            result.types << type;
        }
    }
  return result;
}

/* The name of a compression form, for the sentences below. */
static QString
compressionName (int value)
{
  switch (value)
    {
    case ObjectCompressionGZip:
      return _ ("gzip");
    case ObjectCompressionZLib:
      return _ ("zlib");
    default:
      return _ ("no compression");
    }
}

/* The name of an NBT encoding, matching the labels the object codec uses. */
static QString
encodingName (int value)
{
  switch (value)
    {
    case ObjectEncodingBigEndian:
      return QStringLiteral ("BigEndian");
    case ObjectEncodingLittleEndian:
      return QStringLiteral ("LittleEndian");
    case ObjectEncodingNetworkLittleEndian:
      return QStringLiteral ("NetworkLittleEndian");
    default:
      return QStringLiteral ("?");
    }
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
                /* The compression the file turned out to have is kept for the
                 * report at the end: whether it is what the format wanted is
                 * only known once the plugin that read it is. */
                auto tempVec = file_try_uncompress (
                    filename.toUtf8 (), helper_struct.get (), &failed,
                    &detectedCompression);
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

                    /* Decoding is permissive — every supported encoding is
                     * tried and the first that parses is kept — but which one
                     * parsed is asked for, so a file read as something other
                     * than its format's own encoding can be reported instead of
                     * passing unnoticed. */
                    ObjectLoadOptions loadOptions{ 0, ObjectEncodingAny,
                                                   &detectedEncoding };

                    /* Loading Object */
                    auto objectList = ManageRegionUI::getLoadObjectList ();
                    DhMultiLoadError errors;
                    for (const auto &load : objectList)
                      {
                        void *object = nullptr;
                        auto msg = load.loadObjectFunc (ptr.get (), &object,
                                                        helper_struct.get (),
                                                        &loadOptions);
                        if (msg)
                          {
                            QString typeWithPrefix = _ ("Loading Object %1");
                            typeWithPrefix
                                = typeWithPrefix.arg (load.baseType ());
                            errors.appendError (msg, typeWithPrefix);
                            string_free (msg);
                            continue;
                          }
                        return std::make_pair (
                            load.baseType (),
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
                    /* Whether any candidate managed to read the file. The
                     * messages of the ones that did not are only worth showing
                     * when none did. */
                    bool loaded = false;
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
                        loaded = false;
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

                        /* The load worked; anything odd about the file is
                         * reported when the job finishes rather than being
                         * dropped, or the user would never learn that the file
                         * is not quite what its name or its format says. */
                        collectWarnings (base, candidates.matched,
                                         candidates.types.isEmpty ()
                                             ? QString ()
                                             : candidates.types.first (),
                                         type);
                        typeName = type;
                        break;
                      }
                    /* Nothing could read the file: every attempt is reported.
                     * Otherwise the job is done and reports success — the
                     * earlier attempts' messages are left behind, since the
                     * file did load after all, and what is worth saying (a
                     * name or format that did not match) was collected as a
                     * warning above. */
                    if (!loaded)
                      {
                        throw err;
                      }
                    Q_EMIT emitResult ();
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
  /* A file this module finds no region in is not its format, so the retry can
   * move on; asking which region to load would offer an empty list. */
  if (num <= 0)
    {
      err.appendError (_ ("The file holds no region this module can read."),
                       loadingTypePrefix (multiBase->type ()));
      return false;
    }
  for (int j = 0; j < num; j++)
    {
      auto name = multiBase->nameFunc (object, j);
      if (name)
        regionList.append (name);
      string_free (name);
    }
  if (!DhCore::instance ()->config ().selectAllRegionsInLoading)
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

      /* Woken by closing the row rather than by choosing regions: the file was
       * called off, so nothing is read from it. `doResume ()` is the only path
       * that fills `regionIndexes`, so an empty list here means a cancel. */
      if (cancel_flag_is_cancelled (cancel_flag) != 0)
        {
          return false;
        }
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
          DhCore::instance ()->config ().multiRegionNamePattern, fileBaseName,
          name);
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

void
DhLoadJob::collectWarnings (ModuleBase *base, bool matched,
                            const QString &expectedType, const QString &type)
{
  /* The extension named one plugin and another one read the file. With no
   * suffix there was never an expectation, so nothing is said. */
  if (matched && !expectedType.isEmpty () && type != expectedType)
    {
      loadWarnings << QString (_ ("The file extension points at %1, but the "
                                  "file was read as %2."))
                          .arg (expectedType, type);
    }

  /* The format says how its files are compressed; this one is another way. */
  const int expectedCompression = base->compression ();
  if (expectedCompression != ObjectCompressionAny
      && detectedCompression != ObjectCompressionAny
      && expectedCompression != detectedCompression)
    {
      if (detectedCompression == ObjectCompressionNone)
        {
          loadWarnings << QString (_ ("The file is not compressed, but the %1 "
                                      "format expects %2."))
                              .arg (type, compressionName (expectedCompression));
        }
      else
        {
          loadWarnings << QString (_ ("The file is %1-compressed, but the %2 "
                                      "format expects %3."))
                              .arg (compressionName (detectedCompression), type,
                                    compressionName (expectedCompression));
        }
    }

  /* The format declares the NBT encoding it uses; this file decoded as another
   * one, so it was read as something it is not quite. */
  const int expectedEncoding = base->encoding ();
  if (expectedEncoding != ObjectEncodingAny
      && detectedEncoding != ObjectEncodingAny
      && expectedEncoding != detectedEncoding)
    {
      loadWarnings << QString (_ ("The file was decoded as %1, but the %2 "
                                  "format declares %3."))
                          .arg (encodingName (detectedEncoding), type,
                                encodingName (expectedEncoding));
    }
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

  /* Which plugins the user asked to be prompted about. Only the ones a file's
   * extension points at: a window for a plugin that cannot read any of these
   * files is noise, and before decoding the extension is the only hint there
   * is. A file whose extension names nothing is read with the saved options,
   * since which plugin's options would matter is exactly what is unknown. */
  for (const auto &file : list)
    {
      const auto extension = QFileInfo (file).suffix ();
      if (extension.isEmpty ())
        continue;
      for (auto *module : ManageRegionUI::getModules ())
        {
          if (module->fileSuffix () == extension)
            askTypes.insert (module->type ());
        }
    }
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
       * once.
       *
       * A dismissal aborts the whole batch: the user said no to the settings
       * the load would run with, so nothing should be read. */
      if (!registerInputConfigs ())
        {
          freeInputConfig ();
          this->messageWidget->deleteLater ();
          deleteLater ();
          return;
        }

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
                   else if (auto *loadJob
                            = qobject_cast<DhLoadJob *> (finishedJob);
                            loadJob && !loadJob->warnings ().isEmpty ())
                     {
                       /* The file loaded, but it is not quite what its name or
                        * its format says. Said once the reading is done, and
                        * as a hint rather than an error: the regions are there
                        * and usable either way. */
                       auto *warningWidget = new KMessageWidget ();
                       connect (warningWidget,
                                &KMessageWidget::hideAnimationFinished,
                                warningWidget, &KMessageWidget::deleteLater);
                       MainWindow::addWidgetToTopArea (warningWidget);
                       warningWidget->setMessageType (KMessageWidget::Warning);
                       QString warningText = "%1\n\n%2";
                       warningText
                           = warningText.arg (loadJob->getFilename (),
                                              loadJob->warnings ().join (
                                                  "\n\n"));
                       warningWidget->setText (warningText);
                       warningWidget->setTextFormat (Qt::MarkdownText);
                       QTimer::singleShot (8000, warningWidget,
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
                   /* The row may already be gone: a cancel deletes it while
                    * the job is still finishing up. */
                   if (castedJob->messageWidget)
                     castedJob->messageWidget->setText (realStr);
                 });
      connect (job, &DhLoadJob::result, job->messageWidget,
               &KMessageWidget::animatedHide);
      connect (job, &DhLoadJob::selfSuspended, this,
               [&] (KJob *realJob)
                 {
                   auto castedJob = qobject_cast<DhLoadJob *> (realJob);
                   if (!castedJob->messageWidget)
                     return;
                   QAction *action = new QAction (_ ("Continue"));
                   connect (action, &QAction::triggered, castedJob,
                            &DhLoadJob::doResume);
                   castedJob->messageWidget->addAction (action);
                 });
      connect (job, &DhLoadJob::selfResumed, this,
               [&] (KJob *realJob)
                 {
                   auto castedJob = qobject_cast<DhLoadJob *> (realJob);
                   /* Null when the row was closed to cancel this file: the
                    * worker is woken to unwind, not to carry on, and there is
                    * no row left to clear. */
                   if (castedJob->messageWidget)
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
DhAllLoadJob::registerInputConfigs ()
{
  /* Every plugin, not just the ones a file's extension points at: the batch
   * may hold several formats and the codec is unknown until each file is
   * decoded, so this is the only point where the set is known. A plugin is
   * only *asked* about when the user turned asking on for it (see
   * `askTypes`); the rest get their saved options, so a fallback reader still
   * runs with the settings the user chose for it. */
  for (auto *module : ManageRegionUI::getModules ())
    {
      bool dismissed = false;
      acquireInputConfig (module->type (), &dismissed);
      if (dismissed)
        return false;
    }
  return true;
}

void *
DhAllLoadJob::acquireInputConfig (const QString &type, bool *dismissed)
{
  if (dismissed)
    *dismissed = false;

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
      if (config->useConfigured (type, ConfigObjectItems::Kind::Input)
          || !askTypes.contains (type))
        {
          /* Follows the global configuration, or the user never asked to be
           * prompted about this plugin: the saved values (or the plugin's own
           * defaults) are used and nothing is asked. */
          object = ConfigObjectItems::createObject (
              ConfigObjectItems::Kind::Input, module->library ());
          config->apply (type, ConfigObjectItems::Kind::Input, object);
        }
      else
        {
          /* Asked here rather than in the job: this runs on the GUI thread,
           * while the reading happens on a worker, and a modal dialog from
           * there would be undefined behaviour. A dismissal means the user
           * does not want the load to happen with these settings, so it is
           * reported back and cuts the whole batch short. */
          bool wasDismissed = false;
          object = ConfigObjectUI::getObject (module->library (), CONFIG_INPUT,
                                              true, &wasDismissed);
          if (wasDismissed)
            {
              ConfigObjectItems::freeObject (ConfigObjectItems::Kind::Input,
                                             module->library (), object);
              if (dismissed)
                *dismissed = true;
              return nullptr;
            }
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
