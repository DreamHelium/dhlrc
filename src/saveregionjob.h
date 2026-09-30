#ifndef DHLRC_SAVEREGIONJOB_H
#define DHLRC_SAVEREGIONJOB_H

#include "manageregionui.h"
#include "region.h"

#include <KCompositeJob>
#include <KJob>
#include <QFuture>
#include <condition_variable>
#include <memory>

class KMessageWidget;

/* Exports a list of regions with one plugin, off the GUI thread.
 *
 * The job mirrors `DhLoadJob`: it owns a `HelperStruct` for progress reports,
 * is cancellable through a `cancel_flag`, and can suspend itself so the caller
 * may ask the user for output options (`doResume ()`). Progress and the
 * current region are published through `percent` / `infoMessage`, so a
 * composite parent can show them in the top message area. */
class SaveRegionJob : public KJob
{
  Q_OBJECT
public:
  SaveRegionJob (const QList<std::shared_ptr<RegionClass>> &list,
                 const QString &outputDir, SingleTransFunc func,
                 QLibrary *library, const void *cancel_flag,
                 QObject *parent = nullptr);
  ~SaveRegionJob () override;

  void start () override;
  bool doResume () override;
  void forceResume ();

  /* Names of the regions that could not be written, and why. */
  [[nodiscard]] QStringList
  failedRegions () const
  {
    return failedList;
  }
  [[nodiscard]] QStringList
  failedReasons () const
  {
    return failedReason;
  }
  [[nodiscard]] const QString &
  currentRegion () const
  {
    return current;
  }
  [[nodiscard]] QLibrary *
  pluginLibrary () const
  {
    return library;
  }

Q_SIGNALS:
  /* The user asked for the output options of `currentRegion ()`. */
  void configureRequested (SaveRegionJob *job);

private:
  static void progressFunc (void *main_klass, int value, const char *text,
                            const char *arg);
  /* Reads the pending output config under the lock. */
  void *takeConfigObject ();

  QList<std::shared_ptr<RegionClass>> list;
  QString outputDir;
  SingleTransFunc func = nullptr;
  QLibrary *library = nullptr;
  const void *cancel_flag = nullptr;
  void *configObject = nullptr;
  QString current;
  QStringList failedList;
  QStringList failedReason;
  std::mutex mutex;
  std::condition_variable cv;
  /* Set while the worker is suspended waiting for `doResume ()`. */
  bool resumed = false;
  /* Keeps every region locked while the job runs, so nothing can rename or
   * remove a region being written. */
  std::vector<std::unique_ptr<AutoLocker>> locks;
  std::unique_ptr<HelperStruct, void (*) (HelperStruct *)> helper_struct;
  QFuture<void> future;

public Q_SLOTS:
  /* Called by the owner once the options dialog has been accepted. */
  void setConfigObject (void *object);
};

/* Drives one `SaveRegionJob` per export type and owns the top-area widgets. */
class SaveAllRegionJob : public KCompositeJob
{
  Q_OBJECT
public:
  explicit SaveAllRegionJob (QObject *parent = nullptr);
  ~SaveAllRegionJob () override;

  /* Adds an export and returns the (owned) job, so the caller can keep it. */
  SaveRegionJob *addSave (const QList<std::shared_ptr<RegionClass>> &list,
                          const QString &outputDir, SingleTransFunc func,
                          QLibrary *library);

  void start () override;

Q_SIGNALS:
  /* The user closed the progress widget; the whole export is aborted. */
  void cancelRequested ();

private:
  const void *cancel_flag = nullptr;
};

#endif // DHLRC_SAVEREGIONJOB_H
