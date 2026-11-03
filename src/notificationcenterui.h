#ifndef NOTIFICATIONCENTERUI_H
#define NOTIFICATIONCENTERUI_H

#include "dhlog.h"
#include "dhwidget.h"

#include <QDateTime>
#include <QVector>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTabWidget;

/* The "Notifications & Log" page: what the program told the user, and what it
 * wrote to the log.
 *
 * The notifications used to be transient message widgets; this page keeps the
 * last few hundred of them, with the time, level and text, so a warning that
 * disappeared before it was read can still be looked up. The log tab shows the
 * same messages the daily file receives, live, through the in-memory sink in
 * `dhlog.h`.
 *
 * The page is a singleton on purpose: notifications may arrive before the tab
 * is opened (they are mirrored from `MainWindow::showNotification ()`), and
 * the history has to survive the user closing and opening the page. The
 * `record ()` slot is the entry point for that mirroring. */
class NotificationCenterUI : public DhWidget
{
  Q_OBJECT

public:
  explicit NotificationCenterUI (QWidget *parent = nullptr);
  ~NotificationCenterUI () override;

  /* The one page, created on first use. */
  static NotificationCenterUI *instance ();

public Q_SLOTS:
  /* Mirrors one user-visible notification. `level` uses the same scale as
   * `MainWindow::showNotification ()`: 0 information, 1 warning, 2 or more
   * error. `eventId` is only kept for the row's tooltip; it may be empty.
   *
   * Safe to call before the page is shown. If it is called from a thread other
   * than the page's, the call is queued to the GUI thread. */
  void record (int level, const QString &eventId, const QString &title,
               const QString &text);

private Q_SLOTS:
  /* Appends one message from the in-memory log sink. Always delivered on the
   * GUI thread (the store signal is connected queued). */
  void onLogMessage (const dh::log::LogMessage &message);

  void copyNotifications ();
  void copyLog ();
  void clearNotifications ();
  void clearLogView ();

private:
  /* Builds one tab. */
  QWidget *createNotificationsTab ();
  QWidget *createLogTab ();

  /* Refills a table from the retained records, honouring the filters. Used
   * after a filter change, a clear, and when the page is first built. */
  void rebuildNotifications ();
  void rebuildLog ();

  /* Copies the selected rows of `table`, or every row when nothing is
   * selected, as tab-separated text. */
  void copyTable (QTableWidget *table);

  /* Keeps the tab title's count in step with the history. */
  void updateNotificationTabTitle ();
  /* Re-reads today's log file path (it changes at midnight). */
  void updateLogPathLabel ();

  QTabWidget *tabs = nullptr;

  QComboBox *notificationLevelFilter = nullptr;
  QLineEdit *notificationTextFilter = nullptr;
  QTableWidget *notificationTable = nullptr;

  QComboBox *logLevelFilter = nullptr;
  QLineEdit *logTextFilter = nullptr;
  QTableWidget *logTable = nullptr;
  QLabel *logPathLabel = nullptr;
  QPushButton *openLogFolderButton = nullptr;

  /* The log messages this view retained. Cleared by "Clear view" so a cleared
   * tab stays clear; the store itself keeps its own ring. */
  QVector<dh::log::LogMessage> logMessages;
  /* The newest sequence already present in the initial snapshot. Messages at
   * or below it were loaded from `messages ()` and must not be appended a
   * second time when their queued signal arrives. */
  quint64 logSnapshotSequence = 0;
};

namespace dh::tasks
{
/* Records one task event in the notification centre and in the log, so the
 * work a page starts (loading files, saving regions, replacing a batch) shows
 * up in both places. `level` uses the NotificationCenterUI scale: 0
 * information, 1 warning, 2 or more error. */
void report (int level, const QString &eventId, const QString &title,
             const QString &text);
}

#endif /* NOTIFICATIONCENTERUI_H */
