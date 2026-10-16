#ifndef DHLRC_DEBUGLOADINGUI_H
#define DHLRC_DEBUGLOADINGUI_H

#include <QHash>
#include <QLabel>
#include <QListView>
#include <QMainWindow>
#include <QPointer>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <qboxlayout.h>
#include <qevent.h>
#include <qscrollarea.h>

#include "pluginoptionsconfig.h"

class KMessageWidget;
class DhSettingsDialog;
class DhDirectoryUI;

class MainWindow : public QMainWindow
{
  Q_OBJECT
public:
  explicit MainWindow (QWidget *parent = nullptr);
  ~MainWindow () override;
  static void addWidgetToTopArea (QWidget *widget);
  static void addWidgetToTab (QWidget *widget, const QString &title);
  static MainWindow *instance ();

Q_SIGNALS:
  void windowClosed ();

private:
  QScrollArea *scrollArea;
  QWidget *topWidget;

  QSplitter *topSplitter;
  /** splitter is used to split the ui part
   *  from left to right. So right part shouldn't
   *  set collapsible.
   */
  QSplitter *splitter;
  QWidget *leftWidget;
  QVBoxLayout *leftLayout;
  QSortFilterProxyModel *proxyModel;
  QListView *listView;
  QLineEdit *lineEdit;
  QTabWidget *tabWidget;
  QStandardItemModel *model;
  QAction *actionSearch;
  QVBoxLayout *topLayout;
  QWidget *welcomeWidget;
  QVBoxLayout *tabLayout;
  /* One message row per notification event, reused so repeated changes to the
   * same thing (for example a file saved several times) update one row instead
   * of piling up a new one each time. Owned by the top area. */
  QHash<QString, KMessageWidget *> notificationWidgets;
  /* Owned by Qt (parented to this); shown from the menu. */
  DhSettingsDialog *settingsDialog = nullptr;
  /* The one program-directories page, owned by the tab stack once it is added.
   * A guarded pointer, so a torn-off page that is closed for good leaves this
   * empty instead of dangling. */
  QPointer<DhDirectoryUI> directoriesPage;

public Q_SLOTS:
  void tryShrinkTopWidget ();
  /* Sizes the strip to its rows. Run from `addWidgetToTopArea ()` on the next
   * event-loop pass, once the new row has been laid out. */
  void fitTopArea ();
  /* Re-measures the rows after the strip changes width and re-fits it.
   *
   * A wrapping `KMessageWidget` works its height out for the width it had and
   * caches it, so after a resize that height is wrong and the text is
   * squeezed. Both the row and the layout keep a hint, so both are dropped. */
  void refreshTopAreaRows ();
  /* Shows a message that the core reloaded the configuration file. */
  void onConfigChanged ();

private:
  /* Height the visible rows need at the width the strip currently has.
   *
   * Measured through `heightForWidth` rather than from `topLayout->sizeHint
   * ()`: for a height-for-width child the layout answers for the child's
   * preferred width, which a wrapped row is narrower than. */
  [[nodiscard]] int topAreaContentHeight () const;
  /* Pins each row to the height it reports for its own width.
   *
   * `KMessageWidget` answers `heightForWidth` but does not act on it, so a row
   * that gets narrower keeps its old height and cuts its text off. Nothing
   * else applies it, so it is applied here. */
  void applyTopAreaRowHeights ();

  /* Shows a notification (from the core) as a message row, reusing the row of
   * the same `eventId` if there is one. */
  void showNotification (int level, const QString &eventId,
                         const QString &title, const QString &text);

  /* Tells the core whether it may show desktop notifications, which is "no"
   * while this window is active and the setting says so. */
  void updateNotificationGate ();

  /* Detaches the tab at `index` into a window of its own. */
  void tearOffTab (int index);
  /* Puts a page torn off by `tearOffTab ()` back into the tab stack. */
  void dockBackPage (QWidget *page);
  /* Closes and deletes a torn-off page and its window. */
  void discardFloatingPage (QWidget *page);
  /* The position of `page` in `floatingPages`, or -1. */
  [[nodiscard]] qsizetype indexOfFloating (QWidget *page) const;
  /* Brings `page` to the front, whether it is already a tab or is floating in
   * its own window. Returns its tab index. */
  int ensureTab (QWidget *page, const QString &title);

  void openExternalNbtReader ();
  /* Brings the program-directories page to the front, creating it on first
   * use. It is one page kept for the life of the window, so its tab shows the
   * same view again when it is reopened. */
  void openProgramDirectories ();

  /* A page torn off into a window of its own.
   *
   * The window has no parent, so it is an ordinary top-level window the window
   * manager can decorate and move, unlike a dock's `Qt::Tool` window. That
   * also means neither Qt's teardown nor the tab-widget loop reaches it, so it
   * is taken down by hand (see `~MainWindow`). Its lifetime is managed
   * explicitly rather than through a `destroyed ()` hook: those fire during
   * teardown, when this list is already gone. */
  struct FloatingPage
  {
    QPointer<QWidget> page;
    QPointer<QWidget> window;
  };
  QList<FloatingPage> floatingPages;
  /* Set while the main window is closing, so the floating windows closing with
   * it are not mistaken for the user's. */
  bool closing = false;

public:
  bool eventFilter (QObject *object, QEvent *event) override;

protected:
  void closeEvent (QCloseEvent *event) override;
};

#endif // DHLRC_DEBUGLOADINGUI_H
