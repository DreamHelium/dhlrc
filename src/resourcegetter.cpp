#include "resourcegetter.h"
#include "dhcore.h"
#include <QByteArray>
#include <QString>
#include <libintl.h>
#include <qglobalstatic.h>
#include <qmap.h>
#define _(str) gettext (str)

Q_GLOBAL_STATIC (VersionMap, versionMap)

/* The core calls these on the download's thread (see
 * `dhlrc_core_download_object`); `user` is the `DhDownloader`. They emit
 * signals, which Qt queues onto the thread the receiver lives on. */
extern "C" void
dh_download_info (void *user, const char *message)
{
  auto *downloader = static_cast<DhDownloader *> (user);
  Q_EMIT downloader->info (QString::fromUtf8 (message));
}

extern "C" void
dh_download_progress (void *user, uint64_t received, uint64_t total)
{
  auto *downloader = static_cast<DhDownloader *> (user);
  const int percent
      = total > 0 ? static_cast<int> (received * 100 / total) : 0;
  Q_EMIT downloader->progress (percent);
}

extern "C" int32_t
dh_download_cancel (void *user)
{
  return static_cast<DhDownloader *> (user)->isCancelled () ? 1 : 0;
}

DhDownloader::DhDownloader (QObject *object) : QObject (object) {}

DhDownloader::~DhDownloader ()
{
  /* Ask the worker to stop, then wait for it, so it cannot call back into this
   * object once it is gone. */
  stop ();
  if (worker.joinable ())
    worker.join ();
}

void
DhDownloader::downloadObject (int version, const QString &object)
{
  if (worker.joinable ())
    worker.join ();
  cancelled = false;
  worker = std::thread ([this, version, object] { run (version, object); });
}

void
DhDownloader::stop ()
{
  cancelled = true;
}

bool
DhDownloader::isCancelled () const
{
  return cancelled.load ();
}

void
DhDownloader::run (int version, const QString &object)
{
  auto *core = DhCore::instance ();
  const QByteArray objectUtf8 = object.toUtf8 ();
  char *error = nullptr;
  const int32_t result
      = core ? dhlrc_core_download_object (
                   core->handle (), version, objectUtf8.constData (),
                   &dh_download_info, &dh_download_progress,
                   &dh_download_cancel, this, &error)
             : -1;
  QString message;
  if (result != 0)
    message = error ? QString::fromUtf8 (error) : _ ("The download failed.");
  if (error)
    dhlrc_string_free (error);
  Q_EMIT finished (message);
}

const VersionMap *
get_version_list ()
{
  if (versionMap->isEmpty ())
    {
      /* The table is embedded in the core, so the terminal frontend reads the
       * same bytes without a Qt resource. */
      size_t length = 0;
      const char *bundled
          = dhlrc_core_bundled_resource ("data_version.csv", &length);
      if (!bundled)
        return nullptr;
      auto data = QByteArray (bundled, static_cast<qsizetype> (length));
      auto lines = data.split ('\n');
      for (const auto &line : lines)
        {
          auto trimmed = line.trimmed ();
          if (trimmed.isEmpty ())
            continue;
          auto comma = trimmed.indexOf (',');
          if (comma <= 0)
            continue;

          auto name = trimmed.left (comma).trimmed ();
          bool ok = false;
          auto analysedVersion
              = trimmed.mid (comma + 1).trimmed ().toInt (&ok);

          if (!ok)
            continue;
          versionMap->insert (analysedVersion, name);
        }
    }
  return versionMap;
}

QString
get_translation_from_object (const QString &name)
{
  auto *core = DhCore::instance ();
  if (!core)
    return {};
  /* The locale is a core setting; fall back to `zh_cn` when it is unset so the
   * historical default keeps working. */
  auto locale = core->config ().language;
  if (locale.isEmpty ())
    locale = QStringLiteral ("zh_cn");
  auto key = name.toUtf8 ();
  auto localeKey = locale.toUtf8 ();
  char *translated = dhlrc_core_translate (
      core->handle (), localeKey.constData (), key.constData ());
  if (!translated)
    return {};
  auto text = QString::fromUtf8 (translated);
  dhlrc_string_free (translated);
  return text;
}
