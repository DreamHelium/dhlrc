#ifndef DHLRC_DHCORE_H
#define DHLRC_DHCORE_H

#include "dhlrc_core.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

/* A snapshot of the configuration, mirroring the old `DhConfig` accessors so a
 * caller can move off `DhConfig` one field at a time. */
struct DhConfigData
{
  /* The limit in bytes; the jobs compare it against the free memory. */
  qint64 memoryLimit = 0;
  int limitUnit = 0;
  qint64 elapsedMilliseconds = 0;
  bool selectAllRegionsInLoading = false;
  bool failThenRetry = false;
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
  QString language;
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

  /* One editable setting the core describes: its key, the page it belongs to,
   * the control it wants and the text to show. The strings are message ids. */
  struct SettingInfo
  {
    QString key;
    QString group;
    int type = 0; /* a DhlrcSettingType */
    QString label;
    QString tooltip;
    QStringList choices;
    QString defaultValue;
    bool longText = false;
  };

  /* Every setting, in the order the core lists them. Needs no instance. */
  [[nodiscard]] static QVector<SettingInfo> settings ();

  /* Reads one setting as text; empty when `key` is unknown. */
  [[nodiscard]] QString settingValue (const QString &key) const;
  /* Writes one setting from text; ignored when `key` is unknown. */
  void setSettingValue (const QString &key, const QString &value);

  [[nodiscard]] QString configPath () const;
  /* The last snapshot read from the core; refreshed on every change. */
  [[nodiscard]] const DhConfigData &config () const;

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

  /* Turns the desktop notifications on or off (on by default). */
  void setNotificationsEnabled (bool enabled);

  /* The notifications collected while opening; emptied by the call. */
  QVector<DhCoreNotification> takePendingNotifications ();

  /* Re-reads the configuration from the core into the cache. */
  void refresh ();

  /* The raw C handle, for the places that talk to the core directly (the
   * module libraries, for instance). */
  [[nodiscard]] DhlrcCore *
  handle () const
  {
    return core;
  }

  /* Internal: called by the C trampolines. */
  void addNotification (int level, const char *eventId, const char *title,
                        const char *text);
  /* Runs on the watcher thread; hops to this object's thread with a queued
   * notification describing the change. */
  void requestReload (int level, const char *eventId, const char *title,
                      const char *text);

Q_SIGNALS:
  void configChanged ();

private:
  explicit DhCore (QObject *parent);

  DhlrcCore *core = nullptr;
  DhConfigData cached;
  QVector<DhCoreNotification> pending;
};

#endif // DHLRC_DHCORE_H
