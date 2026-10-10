#ifndef DHLRC_DHCORE_H
#define DHLRC_DHCORE_H

#include "dhlrc_core.h"

#include <QObject>
#include <QString>
#include <QVector>

/* A snapshot of the configuration, mirroring the old `DhConfig` accessors so a
 * caller can move off `DhConfig` one field at a time. */
struct DhConfigData
{
  qint64 memoryLimit = 0;
  int limitUnit = 0;
  qint64 elapsedMilliseconds = 0;
  bool selectAllRegionsInLoading = false;
  bool loadingFileByExtension = false;
  bool failThenRetry = false;
  bool strictNbtEncoding = false;
  bool failDownloadUseCache = false;
  int defaultShowOption = 0;
  bool overrideSetting = false;
  QString baseName;
  QString regionName;
  QString multiRegionNamePattern;
  QString namePatternSampleFile;
  QString namePatternSampleRegion;
  QString description;
  QString author;
  QString overrideVersion;
  QString cacheDirectory;
};

/* One notification the core produced while opening (for example, that it
 * created the configuration file). */
struct DhCoreNotification
{
  int level = 0;
  QString eventId;
  QString title;
  QString text;
};

/* Wraps the Rust core's C ABI: opens the configuration, watches it and hands
 * the values to the UI.
 *
 * The core reloads `config.toml` itself, on a background thread; when it
 * changes, `configChanged` is emitted on the GUI thread. The initial
 * notifications are held until the UI is ready and taken through
 * `takePendingNotifications ()`. */
class DhCore : public QObject
{
  Q_OBJECT
public:
  /* The one instance, or nullptr before `init ()` has run. */
  static DhCore *instance ();
  /* Opens the core. Called once from `main ()`, after `QApplication` exists.
   */
  static void init (QObject *parent = nullptr);

  ~DhCore () override;

  [[nodiscard]] QString configPath () const;
  /* The last snapshot read from the core; refreshed on every change. */
  [[nodiscard]] const DhConfigData &config () const;

  /* Writes `data` into the core and persists it to the file. */
  bool applyAndSave (const DhConfigData &data);

  /* Plugin options. `kind` is 0 for input, 1 for output. */
  [[nodiscard]] bool pluginUseConfigured (const QString &type, int kind) const;
  void setPluginUseConfigured (const QString &type, int kind, bool value);
  [[nodiscard]] bool pluginGetBool (const QString &type, int kind,
                                    const QString &key, bool *out) const;
  void pluginSetBool (const QString &type, int kind, const QString &key,
                      bool value);
  [[nodiscard]] bool pluginGetInt (const QString &type, int kind,
                                   const QString &key, qint64 *out) const;
  void pluginSetInt (const QString &type, int kind, const QString &key,
                     qint64 value);

  /* Persists the current configuration. */
  bool save ();

  /* The notifications collected while opening; emptied by the call. */
  QVector<DhCoreNotification> takePendingNotifications ();

  /* Internal: called by the C trampolines. */
  void addNotification (int level, const char *eventId, const char *title,
                        const char *text);
  void requestReload ();

Q_SIGNALS:
  void configChanged ();

private:
  explicit DhCore (QObject *parent);
  void onFileChanged ();
  void refresh ();

  DhlrcCore *core = nullptr;
  DhConfigData cached;
  QVector<DhCoreNotification> pending;
};

#endif // DHLRC_DHCORE_H
