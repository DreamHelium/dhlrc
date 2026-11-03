#include "notificationcenterui.h"

#include <QDebug>

#include "dhlog.h"

#include <libintl.h>
#define _(str) gettext (str)

#include <QAbstractItemView>
#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QPalette>
#include <QPushButton>
#include <QScrollBar>
#include <QStringList>
#include <QStyle>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QThread>
#include <QVBoxLayout>
#include <QVariant>
#include <QVector>

#include <algorithm>

namespace
{
/* The notification history is kept outside the page so it survives the page
 * being closed and built again, and so the singleton can be created late
 * without losing what arrived before it. The GUI thread is the only writer,
 * but the mutex keeps `record ()`'s queued cross-thread path honest. */
constexpr int maxNotifications = 200;

struct StoredNotification
{
  QDateTime time;
  int level = 0;
  QString eventId;
  QString title;
  QString text;
};

QMutex &
notificationMutex ()
{
  static QMutex mutex;
  return mutex;
}

QVector<StoredNotification> &
notificationRecords ()
{
  static QVector<StoredNotification> records;
  return records;
}

void
appendNotificationRecord (const StoredNotification &record)
{
  QMutexLocker locker (&notificationMutex ());
  auto &records = notificationRecords ();
  records.append (record);
  if (records.size () > maxNotifications)
    records.removeFirst ();
}

QVector<StoredNotification>
notificationSnapshot ()
{
  QMutexLocker locker (&notificationMutex ());
  return notificationRecords ();
}

int
notificationCount ()
{
  QMutexLocker locker (&notificationMutex ());
  return notificationRecords ().size ();
}

void
clearNotificationRecords ()
{
  QMutexLocker locker (&notificationMutex ());
  notificationRecords ().clear ();
}

/* The table columns, named so the code says what it fills. */
constexpr int timeColumn = 0;
constexpr int levelColumn = 1;
constexpr int titleColumn = 2;
constexpr int textColumn = 3;
constexpr int messageColumn = 2;

/* The roles a row keeps beyond its visible text: the log sequence (used to
 * keep an out-of-order queued message in place) and the full timestamp (shown
 * in a tooltip and copied instead of the short time). */
constexpr int sequenceRole = Qt::UserRole;
constexpr int fullTimeRole = Qt::UserRole + 1;

/* A message or a notification can contain line breaks; a table row cannot
 * show them without becoming ragged, so they are escaped the way the daily
 * file does it. The raw text stays in the tooltip. */
QString
singleLine (const QString &text)
{
  QString result = text;
  result.replace (QLatin1Char ('\r'), QStringLiteral ("\\r"));
  result.replace (QLatin1Char ('\n'), QStringLiteral ("\\n"));
  return result;
}

/* `0` information, `1` warning, `2` and above error — the scale
 * `MainWindow::showNotification ()` and `KMessageWidget` use. Returns -1 for
 * the log's debug messages, which have no notification equivalent. */
int
severity (int level)
{
  return level >= 2 ? 2 : level == 1 ? 1 : 0;
}

int
severity (dh::log::Level level)
{
  switch (level)
    {
    case dh::log::Level::Debug:
      return -1;
    case dh::log::Level::Info:
      return 0;
    case dh::log::Level::Warning:
      return 1;
    case dh::log::Level::Critical:
      return 2;
    case dh::log::Level::Off:
      break;
    }
  return 0;
}

QString
notificationLevelText (int level)
{
  if (level >= 2)
    return _ ("Error");
  if (level == 1)
    return _ ("Warning");
  return _ ("Information");
}

QString
logLevelText (dh::log::Level level)
{
  switch (level)
    {
    case dh::log::Level::Debug:
      return _ ("Debug");
    case dh::log::Level::Info:
      return _ ("Info");
    case dh::log::Level::Warning:
      return _ ("Warning");
    case dh::log::Level::Critical:
      return _ ("Critical");
    case dh::log::Level::Off:
      break;
    }
  return _ ("Info");
}

/* The colour of a level: the theme's link colour for information, amber for a
 * warning, red for an error, grey for debug. Colouring the level cell (rather
 * than the whole row) keeps the text readable on every palette. */
QColor
levelColor (QWidget *widget, int severityValue)
{
  switch (severityValue)
    {
    case -1:
      return QColor (128, 128, 128);
    case 0:
      return widget->palette ().color (QPalette::Link);
    case 1:
      return QColor (190, 120, 0);
    default:
      return QColor (200, 45, 45);
    }
}

QIcon
levelIcon (QWidget *widget, int severityValue)
{
  const QStyle::StandardPixmap pixmap
      = severityValue >= 2   ? QStyle::SP_MessageBoxCritical
        : severityValue == 1 ? QStyle::SP_MessageBoxWarning
                             : QStyle::SP_MessageBoxInformation;
  return widget->style ()->standardIcon (pixmap);
}

/* A table with the shared view settings: rows rather than cells, no editing,
 * the last column taking the leftover width. */
QTableWidget *
createTable (const QStringList &headers)
{
  auto *table = new QTableWidget (0, headers.size ());
  table->setHorizontalHeaderLabels (headers);
  table->setEditTriggers (QAbstractItemView::NoEditTriggers);
  table->setSelectionBehavior (QAbstractItemView::SelectRows);
  table->setSelectionMode (QAbstractItemView::ExtendedSelection);
  table->setAlternatingRowColors (true);
  table->setWordWrap (false);
  table->setShowGrid (false);
  table->verticalHeader ()->setVisible (false);
  table->horizontalHeader ()->setStretchLastSection (true);
  table->horizontalHeader ()->setHighlightSections (false);
  return table;
}

QTableWidgetItem *
makeItem (const QString &text)
{
  auto *item = new QTableWidgetItem (text);
  item->setFlags (Qt::ItemIsSelectable | Qt::ItemIsEnabled);
  return item;
}

void
addNotificationRow (QTableWidget *table, int row,
                    const StoredNotification &record)
{
  table->insertRow (row);
  const int severityValue = severity (record.level);
  const QColor color = levelColor (table, severityValue);

  auto *time = makeItem (record.time.toString (
      QStringLiteral ("yyyy-MM-dd HH:mm:ss")));
  time->setData (fullTimeRole,
                 record.time.toString (Qt::ISODateWithMs));
  table->setItem (row, timeColumn, time);

  auto *level = makeItem (notificationLevelText (record.level));
  level->setIcon (levelIcon (table, severityValue));
  level->setForeground (QBrush (color));
  table->setItem (row, levelColumn, level);

  auto *title = makeItem (singleLine (record.title));
  title->setToolTip (record.title);
  table->setItem (row, titleColumn, title);

  auto *text = makeItem (singleLine (record.text));
  text->setToolTip (record.text);
  if (!record.eventId.isEmpty ())
    text->setData (Qt::ToolTipRole,
                   QStringLiteral ("%1\n%2")
                       .arg (record.eventId, record.text));
  table->setItem (row, textColumn, text);
}

void
addLogRow (QTableWidget *table, int row, const dh::log::LogMessage &message)
{
  table->insertRow (row);
  const int severityValue = severity (message.level);
  const QColor color = levelColor (table, severityValue);

  auto *time = makeItem (
      message.time.toString (QStringLiteral ("HH:mm:ss.zzz")));
  time->setData (sequenceRole, QVariant::fromValue (message.sequence));
  time->setData (fullTimeRole,
                 message.time.toString (Qt::ISODateWithMs));
  table->setItem (row, timeColumn, time);

  auto *level = makeItem (logLevelText (message.level));
  level->setIcon (levelIcon (table, severityValue));
  level->setForeground (QBrush (color));
  table->setItem (row, levelColumn, level);

  auto *text = makeItem (singleLine (message.text));
  text->setToolTip (message.text);
  table->setItem (row, messageColumn, text);
}

bool
textMatches (const QString &filter, const QString &text)
{
  return filter.isEmpty ()
         || text.contains (filter, Qt::CaseInsensitive);
}

bool
notificationMatches (int minimumLevel, const QString &filter,
                     const StoredNotification &record)
{
  if (record.level < minimumLevel)
    return false;
  return textMatches (filter, record.title)
         || textMatches (filter, record.text);
}

bool
logMatches (int minimumLevel, const QString &filter,
            const dh::log::LogMessage &message)
{
  if (minimumLevel >= 0
      && static_cast<int> (message.level) < minimumLevel)
    return false;
  return textMatches (filter, message.text);
}
}

void
dh::tasks::report (int level, const QString &eventId, const QString &title,
                   const QString &text)
{
  /* The log tab reads the Qt message log, so one line here is enough for it;
   * the notification row keeps the user-facing wording. */
  if (level >= 2)
    qCritical ().noquote () << title << "-" << text;
  else if (level == 1)
    qWarning ().noquote () << title << "-" << text;
  else
    qInfo ().noquote () << title << "-" << text;
  NotificationCenterUI::instance ()->record (level, eventId, title, text);
}

NotificationCenterUI *
NotificationCenterUI::instance ()
{
  /* A leaked-but-alive singleton: the main window keeps its pages until it is
   * destroyed, and the history has to outlive any page teardown. `QPointer`
   * only protects against an unexpected deletion. */
  static QPointer<NotificationCenterUI> page;
  if (!page)
    page = new NotificationCenterUI;
  return page;
}

NotificationCenterUI::NotificationCenterUI (QWidget *parent)
    : DhWidget (parent)
{
  setWindowTitle (_ ("Notifications & Log"));
  resize (980, 620);

  auto *layout = new QVBoxLayout (this);
  layout->setContentsMargins (0, 0, 0, 0);

  tabs = new QTabWidget ();
  tabs->setObjectName (QStringLiteral ("notificationTabs"));
  layout->addWidget (tabs);

  tabs->addTab (createNotificationsTab (), QString ());
  tabs->addTab (createLogTab (), _ ("Log"));
  updateNotificationTabTitle ();

  /* The log arrives from any thread through the store; the explicit queued
   * connection means the table is only ever touched on the GUI thread. */
  connect (dh::log::store (), &dh::log::LogStore::messageAdded, this,
           &NotificationCenterUI::onLogMessage, Qt::QueuedConnection);

  /* A snapshot after the connection: a message that queued an event while
   * the snapshot was taken is already in it, and `logSnapshotSequence` makes
   * `onLogMessage ()` drop that duplicate. */
  logMessages = dh::log::store ()->messages ();
  if (!logMessages.isEmpty ())
    logSnapshotSequence = logMessages.last ().sequence;
  rebuildLog ();

  rebuildNotifications ();

  /* The path says "today", so it has to be refreshed when the tab is opened
   * again (possibly after midnight). */
  connect (tabs, &QTabWidget::currentChanged, this,
           [this] (int) { updateLogPathLabel (); });
  updateLogPathLabel ();
}

NotificationCenterUI::~NotificationCenterUI () = default;

QWidget *
NotificationCenterUI::createNotificationsTab ()
{
  auto *page = new QWidget ();
  auto *layout = new QVBoxLayout (page);

  auto *filters = new QHBoxLayout ();
  filters->addWidget (new QLabel (_ ("Level:")));

  notificationLevelFilter = new QComboBox ();
  notificationLevelFilter->setObjectName (
      QStringLiteral ("notificationLevelFilter"));
  notificationLevelFilter->addItem (_ ("All"), 0);
  notificationLevelFilter->addItem (_ ("Warning and above"), 1);
  notificationLevelFilter->addItem (_ ("Errors"), 2);
  filters->addWidget (notificationLevelFilter);

  notificationTextFilter = new QLineEdit ();
  notificationTextFilter->setObjectName (
      QStringLiteral ("notificationTextFilter"));
  notificationTextFilter->setPlaceholderText (
      _ ("Filter by title or text"));
  notificationTextFilter->setClearButtonEnabled (true);
  filters->addWidget (notificationTextFilter, 1);

  auto *copy = new QPushButton (_ ("Copy"));
  copy->setToolTip (
      _ ("Copy the selected notifications, or all of them when none is "
         "selected."));
  filters->addWidget (copy);

  auto *clear = new QPushButton (_ ("Clear"));
  clear->setToolTip (_ ("Forget the notification history."));
  filters->addWidget (clear);

  layout->addLayout (filters);

  notificationTable
      = createTable ({ _ ("Time"), _ ("Level"), _ ("Title"), _ ("Text") });
  notificationTable->setObjectName (QStringLiteral ("notificationTable"));
  auto *header = notificationTable->horizontalHeader ();
  header->setSectionResizeMode (timeColumn, QHeaderView::ResizeToContents);
  header->setSectionResizeMode (levelColumn, QHeaderView::ResizeToContents);
  header->setSectionResizeMode (titleColumn, QHeaderView::Interactive);
  header->resizeSection (titleColumn, 220);
  layout->addWidget (notificationTable, 1);

  connect (notificationLevelFilter, &QComboBox::currentIndexChanged, this,
           &NotificationCenterUI::rebuildNotifications);
  connect (notificationTextFilter, &QLineEdit::textChanged, this,
           &NotificationCenterUI::rebuildNotifications);
  connect (copy, &QPushButton::clicked, this,
           &NotificationCenterUI::copyNotifications);
  connect (clear, &QPushButton::clicked, this,
           &NotificationCenterUI::clearNotifications);

  return page;
}

QWidget *
NotificationCenterUI::createLogTab ()
{
  auto *page = new QWidget ();
  auto *layout = new QVBoxLayout (page);

  logPathLabel = new QLabel ();
  logPathLabel->setWordWrap (true);
  logPathLabel->setTextInteractionFlags (Qt::TextSelectableByMouse);
  logPathLabel->setStyleSheet (QStringLiteral ("color:gray;"));
  layout->addWidget (logPathLabel);

  auto *filters = new QHBoxLayout ();
  filters->addWidget (new QLabel (_ ("Level:")));

  logLevelFilter = new QComboBox ();
  logLevelFilter->setObjectName (QStringLiteral ("logLevelFilter"));
  logLevelFilter->addItem (_ ("All"), -1);
  logLevelFilter->addItem (_ ("Debug and above"), 0);
  logLevelFilter->addItem (_ ("Info and above"), 1);
  logLevelFilter->addItem (_ ("Warning and above"), 2);
  logLevelFilter->addItem (_ ("Critical"), 3);
  filters->addWidget (logLevelFilter);

  logTextFilter = new QLineEdit ();
  logTextFilter->setObjectName (QStringLiteral ("logTextFilter"));
  logTextFilter->setPlaceholderText (_ ("Filter the log"));
  logTextFilter->setClearButtonEnabled (true);
  filters->addWidget (logTextFilter, 1);

  auto *copy = new QPushButton (_ ("Copy"));
  copy->setToolTip (
      _ ("Copy the selected log lines, or all of them when none is "
         "selected."));
  filters->addWidget (copy);

  auto *clear = new QPushButton (_ ("Clear view"));
  clear->setToolTip (
      _ ("Empty this view. The messages stay in the log file and the "
         "program's in-memory log."));
  filters->addWidget (clear);

  openLogFolderButton = new QPushButton (_ ("Open Log Folder"));
  filters->addWidget (openLogFolderButton);

  layout->addLayout (filters);

  logTable = createTable ({ _ ("Time"), _ ("Level"), _ ("Message") });
  logTable->setObjectName (QStringLiteral ("logTable"));
  auto *header = logTable->horizontalHeader ();
  header->setSectionResizeMode (timeColumn, QHeaderView::ResizeToContents);
  header->setSectionResizeMode (levelColumn, QHeaderView::ResizeToContents);
  layout->addWidget (logTable, 1);

  connect (logLevelFilter, &QComboBox::currentIndexChanged, this,
           &NotificationCenterUI::rebuildLog);
  connect (logTextFilter, &QLineEdit::textChanged, this,
           &NotificationCenterUI::rebuildLog);
  connect (copy, &QPushButton::clicked, this, &NotificationCenterUI::copyLog);
  connect (clear, &QPushButton::clicked, this,
           &NotificationCenterUI::clearLogView);
  connect (openLogFolderButton, &QPushButton::clicked, this,
           [] { dh::log::openLogDirectory (); });

  return page;
}

void
NotificationCenterUI::record (int level, const QString &eventId,
                              const QString &title, const QString &text)
{
  /* The main window calls this on the GUI thread; a plugin may call it from a
   * worker. Queue rather than touch the table from the wrong thread. */
  if (QThread::currentThread () != thread ())
    {
      QMetaObject::invokeMethod (
          this,
          [this, level, eventId, title, text] {
            record (level, eventId, title, text);
          },
          Qt::QueuedConnection);
      return;
    }

  StoredNotification stored;
  stored.time = QDateTime::currentDateTime ();
  stored.level = level;
  stored.eventId = eventId;
  stored.title = title;
  stored.text = text;
  appendNotificationRecord (stored);

  updateNotificationTabTitle ();

  /* At most 200 rows, and each notification arrives at human speed, so a
   * rebuild is cheaper than keeping a second filtered index in step. */
  const int scroll = notificationTable->verticalScrollBar ()->value ();
  rebuildNotifications ();
  notificationTable->verticalScrollBar ()->setValue (
      std::min (scroll,
                notificationTable->verticalScrollBar ()->maximum ()));
}

void
NotificationCenterUI::onLogMessage (const dh::log::LogMessage &message)
{
  /* Already present in the snapshot taken when the page was built. */
  if (message.sequence <= logSnapshotSequence)
    return;

  /* A message from another thread can arrive after a newer one; keep the
   * vector ordered so a rebuild shows the true order. */
  const auto insertion = std::lower_bound (
      logMessages.begin (), logMessages.end (), message.sequence,
      [] (const dh::log::LogMessage &record, quint64 sequence) {
        return record.sequence < sequence;
      });
  logMessages.insert (insertion, message);
  if (logMessages.size () > dh::log::LogStore::capacity)
    logMessages.removeFirst ();

  if (!logMatches (logLevelFilter->currentData ().toInt (),
                   logTextFilter->text ().trimmed (), message))
    return;

  /* Follow the tail only when the user was already there; scrolling while
   * someone reads older lines would lose their place. */
  auto *bar = logTable->verticalScrollBar ();
  const bool follow = bar->value () >= bar->maximum ();

  const int lastRow = logTable->rowCount () - 1;
  const quint64 lastSequence
      = lastRow < 0
            ? 0
            : logTable->item (lastRow, timeColumn)
                  ->data (sequenceRole)
                  .toULongLong ();
  if (lastRow < 0 || lastSequence < message.sequence)
    {
      addLogRow (logTable, logTable->rowCount (), message);
      if (follow)
        logTable->scrollToBottom ();
    }
  else
    {
      /* Out of order relative to the visible rows; rebuilding is rare and
       * keeps the table consistent with `logMessages`. */
      rebuildLog ();
    }

  /* Keep the view bounded even if the user scrolled away from the tail. */
  while (logTable->rowCount () > dh::log::LogStore::capacity)
    logTable->removeRow (0);
}

void
NotificationCenterUI::rebuildNotifications ()
{
  const int minimumLevel = notificationLevelFilter->currentData ().toInt ();
  const QString filter = notificationTextFilter->text ().trimmed ();
  const auto records = notificationSnapshot ();

  auto *bar = notificationTable->verticalScrollBar ();
  const int scroll = bar->value ();
  notificationTable->setRowCount (0);

  /* Newest first: a notification that just appeared belongs at the top. */
  int row = 0;
  for (auto it = records.crbegin (); it != records.crend (); ++it)
    {
      if (notificationMatches (minimumLevel, filter, *it))
        addNotificationRow (notificationTable, row++, *it);
    }
  bar->setValue (std::min (scroll, bar->maximum ()));
}

void
NotificationCenterUI::rebuildLog ()
{
  const int minimumLevel = logLevelFilter->currentData ().toInt ();
  const QString filter = logTextFilter->text ().trimmed ();

  auto *bar = logTable->verticalScrollBar ();
  const bool follow = bar->value () >= bar->maximum ();
  logTable->setRowCount (0);

  /* The log reads like a console: oldest first, newest at the bottom. */
  int row = 0;
  for (const auto &message : logMessages)
    {
      if (logMatches (minimumLevel, filter, message))
        addLogRow (logTable, row++, message);
    }
  if (follow)
    logTable->scrollToBottom ();
}

void
NotificationCenterUI::copyNotifications ()
{
  copyTable (notificationTable);
}

void
NotificationCenterUI::copyLog ()
{
  copyTable (logTable);
}

void
NotificationCenterUI::clearNotifications ()
{
  clearNotificationRecords ();
  notificationTable->setRowCount (0);
  updateNotificationTabTitle ();
}

void
NotificationCenterUI::clearLogView ()
{
  logMessages.clear ();
  logTable->setRowCount (0);
}

void
NotificationCenterUI::copyTable (QTableWidget *table)
{
  QModelIndexList selected = table->selectionModel ()->selectedRows ();
  std::sort (selected.begin (), selected.end (),
             [] (const QModelIndex &left, const QModelIndex &right) {
               return left.row () < right.row ();
             });

  QVector<int> rows;
  if (selected.isEmpty ())
    {
      rows.reserve (table->rowCount ());
      for (int row = 0; row < table->rowCount (); row++)
        rows.append (row);
    }
  else
    {
      rows.reserve (selected.size ());
      for (const auto &index : selected)
        rows.append (index.row ());
    }
  if (rows.isEmpty ())
    return;

  QStringList lines;
  lines.reserve (rows.size ());
  for (int row : rows)
    {
      QStringList cells;
      cells.reserve (table->columnCount ());
      for (int column = 0; column < table->columnCount (); column++)
        {
          const auto *item = table->item (row, column);
          QString text = item ? item->text () : QString ();
          /* The visible time is short; copy the full one. */
          if (column == timeColumn && item)
            {
              const QString full = item->data (fullTimeRole).toString ();
              if (!full.isEmpty ())
                text = full;
            }
          cells.append (text);
        }
      lines.append (cells.join (QLatin1Char ('\t')));
    }
  QApplication::clipboard ()->setText (lines.join (QLatin1Char ('\n')));
}

void
NotificationCenterUI::updateNotificationTabTitle ()
{
  if (!tabs)
    return;
  tabs->setTabText (0, QString (_ ("Notifications (%1)"))
                           .arg (notificationCount ()));
}

void
NotificationCenterUI::updateLogPathLabel ()
{
  if (!logPathLabel)
    return;
  const QString path = dh::log::currentLogFile ();
  if (path.isEmpty ())
    {
      logPathLabel->setText (_ ("No log file is available."));
      openLogFolderButton->setEnabled (false);
      return;
    }
  logPathLabel->setText (
      QString (_ ("Today's log file: %1")).arg (path));
  openLogFolderButton->setEnabled (true);
}
