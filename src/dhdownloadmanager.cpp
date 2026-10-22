#include "dhdownloadmanager.h"

#include "resourcegetter.h"

DhDownloadManager *
DhDownloadManager::instance ()
{
  /* Only the GUI thread touches the map, and the manager is meant to outlive
   * every page that asks for a download: a function-local static needs no
   * locking and is destroyed after the windows are gone. */
  static DhDownloadManager manager;
  return &manager;
}

DhDownloadManager::DhDownloadManager (QObject *parent) : QObject (parent) {}

QString
DhDownloadManager::clientJarTag (int dataVersion)
{
  return QStringLiteral ("client_jar:") + QString::number (dataVersion);
}

QString
DhDownloadManager::objectTag (int dataVersion, const QString &object)
{
  return QStringLiteral ("object:") + QString::number (dataVersion)
         + QLatin1Char (':') + object;
}

bool
DhDownloadManager::ensureClientJar (int dataVersion)
{
  return start (clientJarTag (dataVersion), dataVersion, QString (),
                /* clientJar */ true);
}

bool
DhDownloadManager::ensureObject (int dataVersion, const QString &object)
{
  return start (objectTag (dataVersion, object), dataVersion, object,
                /* clientJar */ false);
}

void
DhDownloadManager::cancel (const QString &tag)
{
  DhDownloader *downloader = downloads.value (tag);
  if (!downloader)
    return;
  /* The worker notices the flag between blocks and reports the cancellation as
   * the download's error, so the tag is left in the map: the one `finished`
   * that is still coming removes it and tells every page that joined the
   * download that it ended. Dropping it here instead would lose that signal for
   * the pages that did not ask for the cancellation. */
  downloader->stop ();
}

bool
DhDownloadManager::start (const QString &tag, int dataVersion,
                          const QString &object, bool clientJar)
{
  if (downloads.contains (tag))
    return false;

  /* A `DhDownloader` carries one worker, so a tag that is not running gets a
   * fresh one; the entry is removed when it ends, which is what makes a later
   * request for the same tag start again. */
  auto *downloader = new DhDownloader (this);
  downloads.insert (tag, downloader);

  /* The core calls the handlers on the worker thread; the signals therefore
   * arrive queued on this object's thread, where the pages are. The map is
   * checked before relaying: a signal that was queued before the download
   * ended (or was cancelled) would otherwise be taken for progress of whatever
   * download holds the tag by the time it arrives. */
  connect (downloader, &DhDownloader::progress, this,
           [this, downloader, tag] (int percent)
             {
               if (downloads.value (tag) != downloader)
                 return;
               Q_EMIT progress (tag, percent);
             });
  connect (downloader, &DhDownloader::info, this,
           [this, downloader, tag] (const QString &message)
             {
               if (downloads.value (tag) != downloader)
                 return;
               Q_EMIT info (tag, message);
             });
  connect (downloader, &DhDownloader::finished, this,
           [this, downloader, tag] (const QString &error)
             {
               if (downloads.value (tag) != downloader)
                 return;
               downloads.remove (tag);
               downloader->deleteLater ();
               Q_EMIT finished (tag, error);
             });

  if (clientJar)
    downloader->downloadClientJar (dataVersion);
  else
    downloader->downloadObject (dataVersion, object);

  /* After the worker was handed the work, so a listener of `started` already
   * sees the tag in `downloads` (and a cancel it makes finds it there). */
  Q_EMIT started (tag);
  return true;
}
