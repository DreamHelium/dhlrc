#ifndef DHDOWNLOADMANAGER_H
#define DHDOWNLOADMANAGER_H

#include <QHash>
#include <QObject>
#include <QString>

class DhDownloader;

/* The downloads several pages of the programme can want at once, keyed by a
 * tag.
 *
 * A version's client jar is about 30 MB, and both the 3D preview and the
 * material list need it as soon as they are opened; without one place to share
 * them from, the second page would start a second download of the very same
 * file, and the core would be asked for it twice. Here a tag is the identity of
 * a download, so a page that wants one that is already running simply hears
 * about it through the signals below instead of starting another.
 *
 * The tag strings are the same ones the core dedups on (`client_jar:<data
 * version>` and `object:<data version>:<object>`), so the two layers agree on
 * what one download is. */
class DhDownloadManager : public QObject
{
  Q_OBJECT
public:
  /* The one manager. It lives on the GUI thread, like every page that uses
   * it. */
  static DhDownloadManager *instance ();

  /* Starts the download of that version's client jar unless it is already on
   * its way; returns whether this call started it. A caller that gets `false`
   * still receives the signals for the running download. */
  bool ensureClientJar (int dataVersion);
  /* The same for one file of a version (the block reader's language files),
   * under its own tag. */
  bool ensureObject (int dataVersion, const QString &object);
  /* Asks a running download to stop. The worker reports the cancellation as
   * that download's error, so `finished` still arrives once and the tag is
   * free again afterwards. */
  void cancel (const QString &tag);

  /* The tag a download of that kind is filed under; callers compare against
   * the tag a signal carries, so each page only reacts to what it asked for. */
  static QString clientJarTag (int dataVersion);
  static QString objectTag (int dataVersion, const QString &object);

Q_SIGNALS:
  /* A download was started by this manager. A caller that joined one already
   * running does not receive it. */
  void started (const QString &tag);
  /* 0-100. */
  void progress (const QString &tag, int percent);
  void info (const QString &tag, const QString &message);
  /* Empty when the file is in place, otherwise the reason it is not (a
   * cancellation included). Emitted once per download. */
  void finished (const QString &tag, const QString &error);

private:
  explicit DhDownloadManager (QObject *parent = nullptr);
  /* Starts the download under `tag` unless that tag is already running, and
   * returns whether it was started. */
  bool start (const QString &tag, int dataVersion, const QString &object,
              bool clientJar);

  /* The downloads still running, by tag. An entry is removed as soon as its
   * download ends, so the same tag can be asked for again later; the finished
   * `DhDownloader` itself is never reused. */
  QHash<QString, DhDownloader *> downloads;
};

#endif /* DHDOWNLOADMANAGER_H */
