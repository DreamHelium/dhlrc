#ifndef DHLOG_H
#define DHLOG_H

#include <QString>

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

#endif /* DHLOG_H */
