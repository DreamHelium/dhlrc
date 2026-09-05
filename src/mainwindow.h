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
#include <qscrollarea.h>

class MainWindow : public QMainWindow
{
  Q_OBJECT
public:
  explicit MainWindow (QWidget *parent = nullptr);
  ~MainWindow () override;
  static void addWidgetToToolBar (QWidget *widget);
  static void addWidgetToTab (QWidget *widget, const QString &title);
  static MainWindow *instance ();

private:
  QScrollArea *scrollArea;
  QWidget *topWidget;
  QSplitter *allSplitter;
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

public:
  bool eventFilter (QObject *object, QEvent *event) override;
};

#endif // DHLRC_DEBUGLOADINGUI_H
