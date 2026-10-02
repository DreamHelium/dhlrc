#ifndef DHLRC_DEBUGLOADINGUI_H
#define DHLRC_DEBUGLOADINGUI_H

#include <QListView>
#include <QMainWindow>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <qevent.h>
#include <qscrollarea.h>

#include "pluginoptionsconfig.h"

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

public:
  bool eventFilter (QObject *object, QEvent *event) override;

protected:
  void closeEvent (QCloseEvent *event) override;
};

#endif // DHLRC_DEBUGLOADINGUI_H
