#include "dhcore.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QMetaObject>

/* The C trampolines: `dhlrc_core_open`/`dhlrc_core_watch_start` need plain C
 * function pointers, so these stay free functions and recover the instance
 * from the user pointer. */
extern "C" void
dhcore_notify_trampoline (void *user, int level, const char *eventId,
                          const char *title, const char *text)
{
  static_cast<DhCore *> (user)->addNotification (level, eventId, title, text);
}

extern "C" void
dhcore_change_trampoline (void *user, int level, const char *eventId,
                          const char *title, const char *text)
{
  static_cast<DhCore *> (user)->requestReload (level, eventId, title, text);
}

static DhCore *dhCore = nullptr;

DhCore *
DhCore::instance ()
{
  return dhCore;
}

void
DhCore::init (QObject *parent)
{
  if (!dhCore)
    dhCore = new DhCore (parent);
}

DhCore::DhCore (QObject *parent) : QObject (parent)
{
  char *error = nullptr;
  core = dhlrc_core_open (nullptr, &dhcore_notify_trampoline, this, &error);
  if (!core)
    {
      DhCoreNotification note;
      note.level = 2;
      note.eventId = QStringLiteral ("coreOpen");
      note.title = QStringLiteral ("Configuration could not be opened");
      note.text = QString::fromUtf8 (error ? error : "");
      pending.append (note);
      dhlrc_string_free (error);
      return;
    }

  dhlrc_core_watch_start (core, &dhcore_change_trampoline, this);
  refresh ();
}

DhCore::~DhCore ()
{
  if (core)
    {
      dhlrc_core_free (core);
      core = nullptr;
    }
  if (dhCore == this)
    dhCore = nullptr;
}

QString
DhCore::configPath () const
{
  if (!core)
    return {};
  return QString::fromUtf8 (dhlrc_core_config_path (core));
}

const DhConfigData &
DhCore::config () const
{
  return cached;
}

QVector<DhCore::SettingInfo>
DhCore::settings ()
{
  QVector<SettingInfo> result;
  const size_t count = dhlrc_core_setting_count ();
  result.reserve (static_cast<qsizetype> (count));
  for (size_t index = 0; index < count; ++index)
    {
      DhlrcSettingInfo info{};
      if (dhlrc_core_setting_info (index, &info) != 0)
        continue;
      SettingInfo entry;
      entry.key = QString::fromUtf8 (info.key);
      entry.group = QString::fromUtf8 (info.group);
      entry.type = info.kind;
      entry.label = QString::fromUtf8 (info.label);
      entry.tooltip = QString::fromUtf8 (info.tooltip);
      entry.defaultValue = QString::fromUtf8 (info.default_value);
      entry.longText = info.long_text;
      for (size_t choice = 0; choice < info.choice_count; ++choice)
        entry.choices.append (
            QString::fromUtf8 (dhlrc_core_setting_choice (index, choice)));
      result.append (entry);
    }
  return result;
}

QString
DhCore::settingValue (const QString &key) const
{
  if (!core)
    return {};
  const QByteArray keyUtf8 = key.toUtf8 ();
  char *value = dhlrc_core_setting_get (core, keyUtf8.constData ());
  if (!value)
    return {};
  const QString result = QString::fromUtf8 (value);
  dhlrc_string_free (value);
  return result;
}

void
DhCore::setSettingValue (const QString &key, const QString &value)
{
  if (!core)
    return;
  const QByteArray keyUtf8 = key.toUtf8 ();
  const QByteArray valueUtf8 = value.toUtf8 ();
  dhlrc_core_setting_set (core, keyUtf8.constData (), valueUtf8.constData ());
}

void
DhCore::refresh ()
{
  if (!core)
    return;

  DhlrcConfigView *view = dhlrc_core_config_view (core);
  if (!view)
    return;

  auto text = [] (const char *value)
    { return value ? QString::fromUtf8 (value) : QString (); };

  cached.memoryLimit = view->memory_limit;
  cached.limitUnit = view->limit_unit;
  cached.elapsedMilliseconds = view->elapsed_milliseconds;
  cached.selectAllRegionsInLoading = view->select_all_regions_in_loading;
  cached.loadingFileByExtension = view->loading_file_by_extension;
  cached.failThenRetry = view->fail_then_retry;
  cached.strictNbtEncoding = view->strict_nbt_encoding;
  cached.failDownloadUseCache = view->fail_download_use_cache;
  cached.defaultShowOption = view->default_show_option;
  cached.overrideSetting = view->override_setting;
  cached.baseName = text (view->base_name);
  cached.regionName = text (view->region_name);
  cached.multiRegionNamePattern = text (view->multi_region_name_pattern);
  cached.namePatternSampleFile = text (view->name_pattern_sample_file);
  cached.namePatternSampleRegion = text (view->name_pattern_sample_region);
  cached.description = text (view->description);
  cached.author = text (view->author);
  cached.overrideVersion = text (view->override_version);
  cached.cacheDirectory = text (view->cache_directory);
  cached.language = text (view->language);

  dhlrc_config_view_free (view);
}

QVector<DhCoreNotification>
DhCore::takePendingNotifications ()
{
  auto result = pending;
  pending.clear ();
  return result;
}

void
DhCore::addNotification (int level, const char *eventId, const char *title,
                         const char *text)
{
  DhCoreNotification note;
  note.level = level;
  note.eventId = QString::fromUtf8 (eventId);
  note.title = QString::fromUtf8 (title);
  note.text = QString::fromUtf8 (text);
  pending.append (note);
}

void
DhCore::requestReload (int level, const char *eventId, const char *title,
                       const char *text)
{
  /* Runs on the watcher thread: copy the strings here, then hop to the GUI
   * thread, because `pending` (a Qt container) must not be touched off it. */
  DhCoreNotification note;
  note.level = level;
  note.eventId = QString::fromUtf8 (eventId ? eventId : "");
  note.title = QString::fromUtf8 (title ? title : "");
  note.text = QString::fromUtf8 (text ? text : "");
  QMetaObject::invokeMethod (
      this,
      [this, note]
        {
          pending.append (note);
          refresh ();
          Q_EMIT configChanged ();
        },
      Qt::QueuedConnection);
}

bool
DhCore::pluginUseConfigured (const QString &type, int kind) const
{
  if (!core)
    return false;
  const QByteArray typeUtf8 = type.toUtf8 ();
  return dhlrc_core_plugin_use_configured (core, typeUtf8.constData (), kind)
         != 0;
}

void
DhCore::setPluginUseConfigured (const QString &type, int kind, bool value)
{
  if (!core)
    return;
  const QByteArray typeUtf8 = type.toUtf8 ();
  dhlrc_core_plugin_set_use_configured (core, typeUtf8.constData (), kind,
                                        value);
}

bool
DhCore::pluginGetBool (const QString &type, int kind, const QString &key,
                       bool *out) const
{
  if (!core)
    return false;
  const QByteArray typeUtf8 = type.toUtf8 ();
  const QByteArray keyUtf8 = key.toUtf8 ();
  return dhlrc_core_plugin_get_bool (core, typeUtf8.constData (), kind,
                                     keyUtf8.constData (), out)
         != 0;
}

void
DhCore::pluginSetBool (const QString &type, int kind, const QString &key,
                       bool value)
{
  if (!core)
    return;
  const QByteArray typeUtf8 = type.toUtf8 ();
  const QByteArray keyUtf8 = key.toUtf8 ();
  dhlrc_core_plugin_set_bool (core, typeUtf8.constData (), kind,
                              keyUtf8.constData (), value);
}

bool
DhCore::pluginGetInt (const QString &type, int kind, const QString &key,
                      qint64 *out) const
{
  if (!core)
    return false;
  const QByteArray typeUtf8 = type.toUtf8 ();
  const QByteArray keyUtf8 = key.toUtf8 ();
  int64_t value = 0;
  const bool found
      = dhlrc_core_plugin_get_int (core, typeUtf8.constData (), kind,
                                   keyUtf8.constData (), &value)
        != 0;
  if (found && out)
    *out = value;
  return found;
}

void
DhCore::pluginSetInt (const QString &type, int kind, const QString &key,
                      qint64 value)
{
  if (!core)
    return;
  const QByteArray typeUtf8 = type.toUtf8 ();
  const QByteArray keyUtf8 = key.toUtf8 ();
  dhlrc_core_plugin_set_int (core, typeUtf8.constData (), kind,
                             keyUtf8.constData (), value);
}

bool
DhCore::save ()
{
  if (!core)
    return false;
  char *error = nullptr;
  const bool ok = dhlrc_core_save (core, &error) == 0;
  dhlrc_string_free (error);
  return ok;
}

void
DhCore::setNotificationsEnabled (bool enabled)
{
  if (core)
    dhlrc_core_set_notifications_enabled (core, enabled);
}
