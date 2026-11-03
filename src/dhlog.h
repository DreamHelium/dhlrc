#ifndef DHLOG_H
#define DHLOG_H

#include <QDateTime>
#include <QMetaType>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QVector>

/* File logging for the Qt frontend.
 *
 * The program's terminal output already comes from Qt's message handler
 * (`qDebug ()`, `qWarning ()` and so on); this installs a handler of its own
 * that writes the same messages to a daily file under the cache directory
 * while still passing them on to the handler that was installed before, so
 * nothing changes on the terminal.
 *
 * `initialize ()` has to be called once, early in `main ()`, after the core
 * knows where the cache directory is. The level can come from the
 * `--log-level` command-line option or the `DHLRC_LOG_LEVEL` environment
 * variable; see `main.cpp`. */
namespace dh
{
namespace log
{
/* The minimum severity written to the file, in increasing order. `Off`
 * disables the file entirely; messages still reach the terminal. */
enum class Level
{
  Debug,
  Info,
  Warning,
  Critical,
  Off
};

/* One message kept in memory, for the in-app log viewer. `sequence` grows with
 * every accepted message, so a reader that takes a snapshot can tell the
 * messages it already has from the ones that arrive later; it is assigned by
 * `LogStore::append ()` and must be left at zero by callers. */
struct LogMessage
{
  QDateTime time;
  Level level = Level::Info;
  QString text;
  quint64 sequence = 0;
};

/* The in-memory tail of the log: the last `capacity` messages, in the order
 * they were emitted.
 *
 * The installed message handler appends from whatever thread logged, so the
 * buffer is behind a mutex. `messageAdded ()` is emitted after the record is
 * stored, with the store's lock released, so a receiver can connect from the
 * GUI thread and let Qt queue the delivery instead of touching the viewer
 * from a worker thread. */
class LogStore : public QObject
{
  Q_OBJECT
public:
  /* How many messages are kept. Older ones are dropped as new ones arrive. */
  static constexpr int capacity = 2000;

  /* A copy of the buffer, oldest first. Safe from any thread. */
  QVector<LogMessage> messages () const;
  /* How many messages the buffer currently holds, 0..`capacity`. */
  int count () const;

  /* Adds one message. The installed handler is the only caller; anything else
   * would make the viewer show something that was never logged. Safe from any
   * thread. */
  void append (const LogMessage &message);

Q_SIGNALS:
  /* A message was stored. The signal may be emitted from a worker thread, so a
   * GUI receiver should connect with `Qt::QueuedConnection`. */
  void messageAdded (const dh::log::LogMessage &message);

private:
  explicit LogStore (QObject *parent = nullptr);

  mutable QMutex mutex;
  QVector<LogMessage> records;
  quint64 nextSequence = 0;

  friend LogStore *store ();
};

/* The one store, created on first use. It lives for the whole run so messages
 * logged early are still available to the page that shows them. */
LogStore *store ();

/* Parses a level name (`debug`, `info`, `warning`, `critical`, `off`),
 * ignoring case and surrounding whitespace. Anything else — including an
 * empty string — yields `fallback`. */
Level levelFromString (const QString &name, Level fallback = Level::Info);

/* Installs the message handler and remembers where the log files go.
 * `cacheDirectory` is the program's cache directory; the files are written to
 * its `logs` subdirectory. The directory is created lazily, on the first
 * message, so a run that only prints `--help` leaves nothing behind. When
 * `cacheDirectory` is empty the platform's cache location is used instead.
 *
 * Calling it again only changes the directory. */
void initialize (const QString &cacheDirectory);

/* Uninstalls the handler and closes the file. Messages after this point go to
 * the handler that was installed before `initialize ()` again. */
void shutdown ();

/* Changes the minimum severity written to the file. Safe from any thread. */
void setLevel (Level level);

/* The current minimum severity. */
Level level ();

/* The directory the log files are written to, i.e. `<cache directory>/logs`,
 * or an empty string when no cache directory can be resolved at all. */
QString logDirectory ();

/* The file today's messages go to, i.e.
 * `<log directory>/dhlrc-YYYY-MM-DD.log`, or an empty string when there is no
 * log directory. */
QString currentLogFile ();

/* Creates the log directory if needed and opens it in the file manager. Does
 * nothing when there is no log directory. Used by `Help -> Open Log Folder`. */
void openLogDirectory ();
}
}

Q_DECLARE_METATYPE (dh::log::LogMessage)

#endif /* DHLOG_H */
