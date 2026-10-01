#ifndef DHLRC_SAVEREGIONJOB_H
#define DHLRC_SAVEREGIONJOB_H

#include "manageregionui.h"
#include "region.h"

#include <KCompositeJob>
#include <KJob>
#include <QFuture>
#include <memory>

class KMessageWidget;
class PluginOptionsConfig;

/* Exports a single region with one plugin, off the GUI thread.
 *
 * One job per region (mirroring `DhLoadJob`): the composite starts them all,
 * so they run in parallel on the thread pool, and each one carries its own
 * cancel flag. That makes it possible to abort a single region while the
 * others keep going.
 *
 * The output options are not requested here; `SaveAllRegionJob` asks once and
 * hands the resulting object to every job before starting them. */
class SaveRegionJob : public KJob
{
  Q_OBJECT
public:
  SaveRegionJob (std::shared_ptr<RegionClass> region, const QString &outputDir,
                 SingleTransFunc func, void *configObject,
                 const void *cancelFlag, QObject *parent = nullptr);
  ~SaveRegionJob () override;

  void start () override;

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

private:
  static void progressFunc (void *main_klass, int value, const char *text,
                            const char *arg);

  std::shared_ptr<RegionClass> region;
  QString outputDir;
  QString displayNameValue;
  SingleTransFunc func = nullptr;
  void *configObject = nullptr;
  const void *cancelFlag = nullptr;
  /* The region is locked for the whole job, so it cannot be renamed, removed
   * or modified while it is being written. */
  std::unique_ptr<AutoLocker> lock;
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

  /* Asks for the output options once, then schedules one job per region. Each
   * region gets its own progress widget; closing that widget skips the region.
   *
   * `type` names the plugin and is used to look the options up in the
   * settings; when they are not taken from there, the user is asked as before.
   */
  void addSave (const QList<std::shared_ptr<RegionClass>> &list,
                const QString &outputDir, SingleTransFunc func,
                QLibrary *library, const QString &type,
                PluginOptionsConfig *pluginOptions = nullptr);

  void start () override;

private:
  /* Releases the output options once every region has been written. */
  void freeConfigObject ();

  /* The options every job of this batch shares, plus the library they were
   * allocated by (needed to free them). */
  void *configObject = nullptr;
  QLibrary *configObjectLibrary = nullptr;

  /* Owns the per-region cancel flags; they must outlive their jobs. The
   * deleter is `cancel_flag_destroy`, so they cannot leak. */
  std::vector<std::unique_ptr<const void, void (*) (const void *)>>
      cancelFlags;
};

#endif // DHLRC_SAVEREGIONJOB_H
