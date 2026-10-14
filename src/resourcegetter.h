#ifndef RESOURCEGETTER_H
#define RESOURCEGETTER_H

#include <QMap>
#include <QObject>
#include <QString>
#include <atomic>
#include <thread>

using VersionMap = QMap<int, QString>;

/* Fetches files from Mojang's servers into the cache directory.
 *
 * The work itself lives in the core (one blocking C call,
 * `dhlrc_core_download_object`), so this only runs it on a worker thread and
 * turns the core's progress into Qt signals. The terminal frontend calls the
 * same core entry point directly, with no Qt involved. */
class DhDownloader : public QObject
{
  Q_OBJECT
public:
  explicit DhDownloader (QObject *object = nullptr);
  ~DhDownloader () override;

  /* Starts fetching `object` (`minecraft/lang/zh_cn.json`) for the version
   * whose data version is `version`. Emits `progress` and `info` while
   * running, then `finished` exactly once. */
  void downloadObject (int version, const QString &object);
  /* Asks a running download to stop; it ends soon after. */
  void stop ();
  /* Whether a stop has been asked for (the core's cancel callback reads this).
   */
  [[nodiscard]] bool isCancelled () const;

Q_SIGNALS:
  /* 0-100; the core reports bytes, this reports the percentage. */
  void progress (int value);
  void info (const QString &message);
  /* Empty when the file is in place, otherwise the reason it is not. */
  void finished (const QString &error);

private:
  void run (int version, const QString &object);
  std::thread worker;
  std::atomic<bool> cancelled = false;
};

const VersionMap *get_version_list ();
/** The localized name of a block or item (`minecraft:oak_log`), in the locale
 * the configuration carries. Empty when there is no name for it. */
QString get_translation_from_object (const QString &name);

#endif /* RESOURCEGETTER_H */
