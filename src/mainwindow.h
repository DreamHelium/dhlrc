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
class ExternalNbtReaderUI;

class MainWindow : public QMainWindow
{
  Q_OBJECT
public:
  explicit MainWindow (QWidget *parent = nullptr);
  ~MainWindow () override;
  /* Makes `window` close when the main window closes, through the
   * `windowClosed ()` signal. A window the program opens registers itself
   * once, in its constructor; a window that belongs to another window (a
   * reader's tools) is closed by that window's own hierarchy instead. */
  static void ownWindow (QWidget *window);
  static void addWidgetToTopArea (QWidget *widget);
  static void addWidgetToTab (QWidget *widget, const QString &title);
  /* Adds `widget` as a tab and selects it. With a non-empty `key` the tab is
   * identified by it: when a page already holds that key, that page is brought
   * forward instead and `widget` is discarded. */
  static void addKeyedTab (QWidget *widget, const QString &title,
                           const QString &key);
  /* The page — a tab or one torn off into a window — added with `key`, or
   * nullptr. */
  static QWidget *keyedTab (const QString &key);
  /* Brings an existing page forward, docking it back if it was torn off. */
  static void showTab (QWidget *page, const QString &title);
  /* Sets the title of a page that is in the tab bar or torn off into a window. */
  static void setPageTitle (QWidget *page, const QString &title);
  /* Removes the tab holding `widget` and deletes it; does nothing when the
   * widget is not one of the tabs. */
  static void closeTab (QWidget *widget);
  /* Opens the workspace of the region at `index` on its reader page, or
   * brings the workspace that is open for that region forward. */
  static void openRegionReader (int index);
  static MainWindow *instance ();
  /* Opens the settings dialog, or raises it when it is already open, and
   * selects the page for `group` — the untranslated group message id the core's
   * schema uses ("General", "Reader", "3D Preview", …). A group the dialog does
   * not have falls back to the first page; an empty one leaves the page alone. */
  static void openSettings (const QString &group = QString ());

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

  /* Opens an external NBT reader: as a tab, or as a window of its own when
   * `newWindow` is set. A repeat call opens another reader; the popup lists the
   * ones already open. */
  void openExternalNbtReader (bool newWindow = false);
  /* The open external NBT readers as a popup under the sidebar row, named
   * after the file each read, plus the two ways to open another one. */
  void showNbtReaderMenu ();
  /* Brings a reader to the front, whether it is a tab or a window. */
  void showNbtReader (ExternalNbtReaderUI *reader);
  /* Runs what the sidebar row carries. Single click and Enter (the list's
   * `clicked` and `activated` signals) both come here. */
  void openSidebarEntry (const QModelIndex &index);
  /* Brings the program-directories page to the front, creating it on first
   * use. It is one page kept for the life of the window, so its tab shows the
   * same view again when it is reopened. */
  void openProgramDirectories ();
  /* The region list as a popup under the sidebar row: one entry per region,
   * checked when that region's reader is already open, and picking one opens
   * (or brings forward) its reader. */
  void showRegionReaderMenu ();

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
