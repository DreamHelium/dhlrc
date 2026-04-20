#ifndef DHLRC_DEBUGLOADINGUI_H
#define DHLRC_DEBUGLOADINGUI_H

#include "dhconfigdialog/src/dhconfigdialog.h"
#include "manageregionui.h"

#include <QListView>
#include <QMainWindow>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

class MainWindow : public QMainWindow
{
  Q_OBJECT
public:
  explicit MainWindow (QWidget *parent = nullptr);
  ~MainWindow () override;
  ManageRegionUI *mrui = new ManageRegionUI (this);
  DhConfigDialog *dialog = nullptr;
  void addWidgetToToolBar (QWidget *widget);

private:
  QSplitter *splitter;
  QWidget *leftWidget;
  QVBoxLayout *leftLayout;
  QSortFilterProxyModel *proxyModel;
  QListView *listView;
  QLineEdit *lineEdit;
  QTabWidget *tabWidget;
  QStandardItemModel *model;
  QAction *actionSearch;
  QWidget *container;
  QVBoxLayout *topLayout;
  QVBoxLayout *allLayout;

protected:
  void resizeEvent (QResizeEvent *event) override;
};

#endif // DHLRC_DEBUGLOADINGUI_H
