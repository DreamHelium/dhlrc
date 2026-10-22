#include "dhlog.h"

#include <QDate>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QStandardPaths>
#include <QThread>
#include <QUrl>

#include <atomic>
#include <cstdio>
#include <cstdlib>

namespace
{
/* State shared by every thread that logs. `mutex` guards all of it; the
 * handler that was installed before ours is kept in an atomic so a nested
 * message (one raised while the file is being written) can be forwarded
 * without taking the mutex again and deadlocking. */
struct LogState
{
  QMutex mutex;
  QString cacheDirectory;
  dh::log::Level minimumLevel = dh::log::Level::Info;
  bool initialized = false;
  QFile file;
  QDate fileDate;
  bool openFailed = false;
};

LogState &
state ()
{
  static LogState instance;
  return instance;
}

std::atomic<QtMessageHandler> previousHandler{ nullptr };

/* Set while our handler runs on this thread. A message raised from inside the
 * handler (a warning from QFile, say) is only forwarded, never written again,
 * so the mutex is never re-entered. */
thread_local bool insideHandler = false;

dh::log::Level
messageLevel (QtMsgType type)
{
  switch (type)
    {
    case QtDebugMsg:
      return dh::log::Level::Debug;
    case QtInfoMsg:
      return dh::log::Level::Info;
    case QtWarningMsg:
      return dh::log::Level::Warning;
    case QtCriticalMsg:
    case QtFatalMsg:
      return dh::log::Level::Critical;
    }
  return dh::log::Level::Info;
}

QString
levelTag (dh::log::Level level)
{
  switch (level)
    {
    case dh::log::Level::Debug:
      return QStringLiteral ("[DEBUG]");
    case dh::log::Level::Info:
      return QStringLiteral ("[INFO]");
    case dh::log::Level::Warning:
      return QStringLiteral ("[WARNING]");
    case dh::log::Level::Critical:
      return QStringLiteral ("[CRITICAL]");
    case dh::log::Level::Off:
      break;
    }
  return QStringLiteral ("[OFF]");
}

/* `<cache directory>/logs`, resolving the platform default when `initialize ()`
 * was given no directory. Called with the mutex held. */
QString
resolveLogDirectory (const LogState &logState)
{
  QString base = logState.cacheDirectory;
  if (base.isEmpty ())
    base = QStandardPaths::writableLocation (QStandardPaths::CacheLocation);
  if (base.isEmpty ())
    return {};
  return QDir (base).filePath (QStringLiteral ("logs"));
}

QString
logFileName (const QDate &date)
{
  return QStringLiteral ("dhlrc-%1.log").arg (date.toString (Qt::ISODate));
}

/* One line per record: local timestamp with milliseconds and UTC offset, level
 * tag, Qt message category, source file and line, thread id, then the message.
 * Line breaks inside the message are escaped so a record never spans two
 * lines. */
QString
formatRecord (QtMsgType type, const QMessageLogContext &context,
              const QString &message)
{
  QString text = message;
  text.replace (QLatin1Char ('\r'), QStringLiteral ("\\r"));
  text.replace (QLatin1Char ('\n'), QStringLiteral ("\\n"));

  const auto threadId
      = reinterpret_cast<quintptr> (QThread::currentThreadId ());

  QString line;
  /* `ttt` is the ISO 8601 offset ("+08:00"); plain `t` would give the zone's
   * abbreviation, which is ambiguous. */
  line += QDateTime::currentDateTime ().toString (
      QStringLiteral ("yyyy-MM-dd HH:mm:ss.zzz ttt"));
  line += QLatin1Char (' ');
  line += levelTag (messageLevel (type));
  line += QLatin1Char (' ');
  line += (context.category && *context.category)
              ? QString::fromUtf8 (context.category)
              : QStringLiteral ("default");
  line += QLatin1Char (' ');
  line += context.file ? QString::fromUtf8 (context.file)
                       : QStringLiteral ("?");
  line += QLatin1Char (':');
  line += QString::number (context.line);
  line += QStringLiteral (" tid=0x");
  line += QString::number (threadId, 16);
  line += QLatin1Char (' ');
  line += text;
  return line;
}

/* Appends the record to today's file, opening (and rotating) it first when
 * needed. Called with the mutex held. */
void
writeRecord (QtMsgType type, const QMessageLogContext &context,
             const QString &message)
{
  LogState &logState = state ();
  const QDate today = QDate::currentDate ();

  if (!logState.file.isOpen () && logState.openFailed
      && logState.fileDate == today)
    return;

  if (!logState.file.isOpen () || logState.fileDate != today)
    {
      if (logState.file.isOpen ())
        logState.file.close ();
      logState.fileDate = today;
      logState.openFailed = false;

      const QString directory = resolveLogDirectory (logState);
      if (directory.isEmpty () || !QDir ().mkpath (directory))
        {
          logState.openFailed = true;
          return;
        }
      logState.file.setFileName (
          QDir (directory).filePath (logFileName (today)));
      if (!logState.file.open (QIODevice::WriteOnly | QIODevice::Append))
        {
          logState.openFailed = true;
          return;
        }
    }

  const QByteArray record
      = formatRecord (type, context, message).toUtf8 () + '\n';
  logState.file.write (record);
  /* Flushed on every record so a crash still leaves the log on disk. */
  logState.file.flush ();
}

/* Hands the message to the handler that was installed before ours, or, when
 * there was none, prints what Qt's default handler would have printed. */
void
forwardMessage (QtMsgType type, const QMessageLogContext &context,
                const QString &message)
{
  const QtMessageHandler previous = previousHandler.load ();
  if (previous)
    {
      previous (type, context, message);
      return;
    }

  /* Qt's default handler writes "<category>: <message>" to stderr unless the
   * category is the default one. `QT_MESSAGE_PATTERN`, when set, is not
   * reproduced; nothing in this project sets it. */
  const QByteArray local = message.toLocal8Bit ();
  if (context.category && *context.category
      && qstrcmp (context.category, "default") != 0)
    std::fprintf (stderr, "%s: %s\n", context.category, local.constData ());
  else
    std::fprintf (stderr, "%s\n", local.constData ());
  std::fflush (stderr);
}

void
messageHandler (QtMsgType type, const QMessageLogContext &context,
                const QString &message)
{
  if (insideHandler)
    {
      forwardMessage (type, context, message);
      return;
    }

  insideHandler = true;
  {
    LogState &logState = state ();
    QMutexLocker locker (&logState.mutex);
    if (logState.initialized && logState.minimumLevel != dh::log::Level::Off
        && static_cast<int> (messageLevel (type))
               >= static_cast<int> (logState.minimumLevel))
      writeRecord (type, context, message);
  }

  forwardMessage (type, context, message);
  insideHandler = false;

  /* Qt's default handler ends the program on a fatal message; forwarding to a
   * custom handler does not, so keep that behaviour here. */
  if (type == QtFatalMsg)
    std::abort ();
}
}

dh::log::Level
dh::log::levelFromString (const QString &name, Level fallback)
{
  const QString normalized = name.trimmed ().toLower ();
  if (normalized == QLatin1String ("debug"))
    return Level::Debug;
  if (normalized == QLatin1String ("info"))
    return Level::Info;
  if (normalized == QLatin1String ("warning")
      || normalized == QLatin1String ("warn"))
    return Level::Warning;
  if (normalized == QLatin1String ("critical")
      || normalized == QLatin1String ("crit"))
    return Level::Critical;
  if (normalized == QLatin1String ("off") || normalized == QLatin1String ("none"))
    return Level::Off;
  return fallback;
}

void
dh::log::initialize (const QString &cacheDirectory)
{
  LogState &logState = state ();
  {
    QMutexLocker locker (&logState.mutex);
    logState.cacheDirectory = cacheDirectory;
    if (logState.initialized)
      {
        /* Called twice: only the destination changes. */
        if (logState.file.isOpen ())
          logState.file.close ();
        logState.fileDate = QDate ();
        logState.openFailed = false;
        return;
      }
    logState.initialized = true;
  }

  previousHandler.store (qInstallMessageHandler (messageHandler));
}

void
dh::log::shutdown ()
{
  LogState &logState = state ();
  QtMessageHandler previous = nullptr;
  {
    QMutexLocker locker (&logState.mutex);
    if (!logState.initialized)
      return;
    logState.initialized = false;
    if (logState.file.isOpen ())
      {
        logState.file.flush ();
        logState.file.close ();
      }
    logState.fileDate = QDate ();
    logState.openFailed = false;
    previous = previousHandler.exchange (nullptr);
  }
  qInstallMessageHandler (previous);
}

void
dh::log::setLevel (Level newLevel)
{
  LogState &logState = state ();
  QMutexLocker locker (&logState.mutex);
  logState.minimumLevel = newLevel;
}

dh::log::Level
dh::log::level ()
{
  LogState &logState = state ();
  QMutexLocker locker (&logState.mutex);
  return logState.minimumLevel;
}

QString
dh::log::logDirectory ()
{
  LogState &logState = state ();
  QMutexLocker locker (&logState.mutex);
  return resolveLogDirectory (logState);
}

QString
dh::log::currentLogFile ()
{
  LogState &logState = state ();
  QMutexLocker locker (&logState.mutex);
  const QString directory = resolveLogDirectory (logState);
  if (directory.isEmpty ())
    return {};
  return QDir (directory).filePath (logFileName (QDate::currentDate ()));
}

void
dh::log::openLogDirectory ()
{
  const QString directory = logDirectory ();
  if (directory.isEmpty ())
    return;
  if (!QDir ().mkpath (directory))
    return;
  QDesktopServices::openUrl (QUrl::fromLocalFile (directory));
}
