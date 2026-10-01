#ifndef DHLRC_SAVEREGIONJOB_H
#define DHLRC_SAVEREGIONJOB_H

#include "manageregionui.h"
#include "region.h"

#include <KCompositeJob>
#include <KJob>
#include <KMessageWidget>
#include <QFuture>
#include <QPointer>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>

class PluginOptionsConfig;

class SaveAllRegionJob;

/* Exports a single region with one plugin, off the GUI thread.
 *
 * Shaped like `DhLoadJob`: one job per region, the composite starts them all
 * so they run in parallel on the thread pool, and each carries its own cancel
 * flag and its own row. The row belongs to the job (`messageWidget`), so it is
 * not created for a write that never happens, and it is not taken away by
 * anything but this job finishing.
 *
 * The output options are not requested here; the caller resolves them once and
 * hands the object in before `start ()`. */
class SaveRegionJob : public KJob
{
  Q_OBJECT
public:
  SaveRegionJob (std::shared_ptr<RegionClass> region, const QString &outputDir,
                 const QString &suffix, SingleTransFunc func,
                 void *configObject, const void *cancelFlag,
                 QObject *parent = nullptr);
  ~SaveRegionJob () override;

  void start () override;

  /* The row reporting this region. Owned by nobody else, and a `QPointer`
   * because the row deletes itself once hidden while the job may still be
   * finishing up. */
  QPointer<KMessageWidget> messageWidget = nullptr;

  /* Whether the options still have to be settled before this region is
   * written. Set by the composite when the plugin does not follow the
   * configuration, so the worker parks on its row until the user answers. */
  bool needsOptions = false;
  /* Reads the options the user chose, once the worker is woken. Called on the
   * GUI thread by `SaveAllRegionJob`; the worker only reads `configObject`
   * afterwards. */
  std::function<void *()> requestOptions;

  [[nodiscard]] const QString &
  displayName () const
  {
    return displayNameValue;
  }
  /* Why this region was not written; empty when it succeeded. */
  [[nodiscard]] const QString &
  failureReason () const
  {
    return failure;
  }
  /* True when this region was aborted before it was written. */
  [[nodiscard]] bool wasCancelled () const;
  /* True when `flag` is the flag this job watches, i.e. this is the job the
   * row belongs to. */
  [[nodiscard]] bool
  cancels (const void *flag) const
  {
    return cancelFlag == flag;
  }
  /* Takes the object the user configured for this file, just before the worker
   * is woken. Owned by the composite, which outlives this job. */
  void
  adoptConfigObject (void *object)
  {
    configObject = object;
  }
  /* Wakes a job parked on the row waiting for the user. */
  void forceResume ();

Q_SIGNALS:
  /* Raised when the worker has parked itself and needs the row to offer a way
   * to carry on; the composite adds the `Continue` action on it. */
  void selfSuspended (KJob *job);
  void selfResumed (KJob *job);

private:
  static void progressFunc (void *main_klass, int value, const char *text,
                            const char *arg);

  std::shared_ptr<RegionClass> region;
  QString outputDir;
  /* The module's file suffix, without the dot; appended to the display name so
   * the written file can be matched by extension when it is loaded again. */
  QString fileSuffix;
  QString displayNameValue;
  SingleTransFunc func = nullptr;
  /* The options to write with, already resolved by the caller. Borrowed for
   * the length of the write; `SaveAllRegionJob` owns it. */
  void *configObject = nullptr;
  const void *cancelFlag = nullptr;
  /* Parked on the row until the options are settled. */
  std::mutex mutex;
  std::condition_variable cv;
  /* The region is locked for the whole job, so it cannot be renamed, removed
   * or modified while it is being written. */
  std::unique_ptr<AutoLocker> lock;
  QString failure;
  std::unique_ptr<HelperStruct, void (*) (HelperStruct *)> helper_struct;
  QFuture<void> future;
};

/* Writes several regions into one file, off the GUI thread.
 *
 * One job for the whole file: the regions share a single output, so there is
 * nothing to split up and a per-region cancel would have no meaning. All the
 * regions are locked for the duration, since half-writing a file whose
 * contents are being renamed or removed would be worse than failing. */
class SaveMultiRegionJob : public KJob
{
  Q_OBJECT
public:
  SaveMultiRegionJob (const QList<std::shared_ptr<RegionClass>> &list,
                      const QString &filename, MultiTransFunc func,
                      void *configObject, const void *cancelFlag,
                      QObject *parent = nullptr);
  ~SaveMultiRegionJob () override;

  void start () override;

  /* See `SaveRegionJob::messageWidget`. */
  QPointer<KMessageWidget> messageWidget = nullptr;

  /* See `SaveRegionJob::needsOptions`. */
  bool needsOptions = false;
  std::function<void *()> requestOptions;

  /* True when `flag` is the flag this job watches. */
  [[nodiscard]] bool
  cancels (const void *flag) const
  {
    return cancelFlag == flag;
  }
  /* Takes the object the user configured for this file; owned by the batch. */
  void
  adoptConfigObject (void *object)
  {
    configObject = object;
  }
  /* Wakes a job parked on the row waiting for the user. */
  void forceResume ();

Q_SIGNALS:
  void selfSuspended (KJob *job);
  void selfResumed (KJob *job);

public:
  [[nodiscard]] bool wasCancelled () const;
  [[nodiscard]] const QString &
  failureReason () const
  {
    return failure;
  }

private:
  static void progressFunc (void *main_klass, int value, const char *text,
                            const char *arg);

  QList<std::shared_ptr<RegionClass>> regions;
  QString filename;
  MultiTransFunc func = nullptr;
  /* Borrowed from `SaveAllRegionJob` for the length of the write. */
  void *configObject = nullptr;
  const void *cancelFlag = nullptr;
  /* Parked on the row until the options are settled. */
  std::mutex mutex;
  std::condition_variable cv;
  /* One lock per region, so none can be modified while it is being written. */
  std::vector<std::unique_ptr<AutoLocker>> locks;
  QString failure;
  std::unique_ptr<HelperStruct, void (*) (HelperStruct *)> helper_struct;
  QFuture<void> future;
};

/* Drives one `SaveRegionJob` per region and owns the top-area widgets.
 *
 * Each region gets its own `KMessageWidget`; its built-in close button skips
 * that region, so no extra cancel control is needed. */
class SaveAllRegionJob : public KCompositeJob
{
  Q_OBJECT
public:
  explicit SaveAllRegionJob (QObject *parent = nullptr);
  ~SaveAllRegionJob () override;

  /* Writes one file per region, one job each, with `configObject`.
   *
   * Built like `DhAllLoadJob`: the batch row and a row for every region are
   * created here, before anything runs, and each region's job is handed its
   * own row. Nothing is deferred, so what the user sees always matches what is
   * about to be written.
   *
   * `configObject` has already been resolved by the caller (see
   * `resolveOutputConfig ()`) and is owned by this job from here on. */
  void addSave (const QList<std::shared_ptr<RegionClass>> &list,
                const QString &outputDir, const QString &suffix,
                SingleTransFunc func, const QString &type, QLibrary *library);

  /* Writes every region of `list` into the single file `filename`. Built the
   * same way as `addSave ()`; one job for the whole file, so a per-region
   * cancel would have nothing to cancel. */
  void addSaveIntoMulti (const QList<std::shared_ptr<RegionClass>> &list,
                         const QString &filename, const QString &suffix,
                         QLibrary *library, const QString &type);

  void start () override;

public:
  /* Resolves the output options for `type`, on the calling (GUI) thread.
   *
   * This is the save-side twin of `DhAllLoadJob::acquireInputConfig ()`, and
   * it follows the same rule: when the plugin's switch is on the saved values
   * are used and nothing is asked, otherwise the user is shown the options.
   *
   * Returns the object to write with, or nullptr when the user dismissed the
   * window — in which case the caller must abort the whole write rather than
   * quietly using the defaults. `cancelled`, when given, tells the two cases
   * apart: a plugin with no options also yields nullptr, but it did not ask.
   */
  static void *resolveOutputConfig (const QString &type, QLibrary *library,
                                    PluginOptionsConfig *pluginOptions,
                                    bool *cancelled = nullptr);

  /* The object the batch writes with, owned here until the last region is
   * done. Every subjob borrows it. */
  [[nodiscard]] void *
  configObjectForWrite () const
  {
    return configObject;
  }

private:
  /* Releases the output options once every region has been written. */
  void freeConfigObject ();

  /* Whether `type` has to be asked about before writing. */
  [[nodiscard]] bool needsOptions (const QString &type) const;
  /* Resolves the options on the GUI thread, for one file. */
  void *askForOptions (const QString &type, QLibrary *library);
  /* Takes ownership of an options object and hands it back, so a caller can
   * write `job->adoptConfigObject (ownConfigObject (o, lib))`. */
  void *ownConfigObject (void *object, QLibrary *library);
  /* Lets go of every parked worker. One answer settles the whole batch, since
   * the options object is shared. */
  void resumeAll ();
  /* Lets go of the one subjob watching `flag`, whichever kind it is. */
  void resumeFlag (const void *flag);

  /* Builds the batch's own row, the one that counts how many regions are done.
   */
  void setupOverallWidget ();

  /* Wires a row's close button to a cancel flag. `flag` may be nullptr, which
   * means "close everything" — that is what the batch's own row does. */
  void registerCancelRow (class KMessageWidget *widget, const void *flag);
  /* Detaches a row again, so hiding it on completion is not read as a cancel.
   */
  void unregisterCancelRow (class KMessageWidget *widget);
  /* Acts on a row being closed. */
  void cancelRow (QWidget *widget);

  /* One registered row: the widget, the flag its close button stops (nullptr
   * for the batch's row, which stops everything), and the connection to cut
   * when the row is done.
   *
   * A list rather than a map, because the connection has to be stored with the
   * row: `unregisterCancelRow ()` has to disconnect it for real, and a
   * connection whose context is the row cannot be found from here. */
  struct CancelRow
  {
    QPointer<KMessageWidget> widget;
    const void *flag = nullptr;
    QMetaObject::Connection connection;
  };
  std::vector<CancelRow> rows;

  /* The options the plugin follows the configuration with, when it does. The
   * first owned object, kept for `freeConfigObject ()`. */
  void *configObject = nullptr;
  QLibrary *configObjectLibrary = nullptr;
  /* Every options object the batch owns: one built from the saved settings
   * when the plugin follows them, or one per file that was configured by hand.
   * Held here because a job is deleted as soon as it finishes. */
  std::vector<std::pair<void *, QLibrary *>> ownedConfigObjects;

  /* The batch's own row. Every region also gets one, and this one says how
   * many of them are done, so a write of many files has a single line that
   * reaches 100% rather than several that each jump on their own. */
  KMessageWidget *overallWidget = nullptr;
  int jobNums = 0;
  int finishedJobs = 0;

  /* Owns the per-region cancel flags; they must outlive their jobs. The
   * deleter is `cancel_flag_destroy`, so they cannot leak. */
  std::vector<std::unique_ptr<const void, void (*) (const void *)>>
      cancelFlags;
};

#endif // DHLRC_SAVEREGIONJOB_H
