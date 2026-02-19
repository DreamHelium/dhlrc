#ifndef DHLRC_DHHELPUI_H
#define DHLRC_DHHELPUI_H

#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMainWindow>
#include <QScrollArea>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QWidget>

class DhHelpUI : public QWidget
{
  Q_OBJECT

public:
  explicit DhHelpUI (QWidget *parent = nullptr);
  ~DhHelpUI () override;
  static void showHelp (const QString &str);

protected:
  void resizeEvent (QResizeEvent *event) override;

private:
  QSplitter *splitter;
  QStandardItemModel *model;
  QSortFilterProxyModel *sortmodel;
  void showSome (const QString &str);
  QString loadingStr;
  QWidget *dockWidget;
  QVBoxLayout *layout;
  QVBoxLayout *labelLayout;
  QWidget *labelWidget;
  QDockWidget *dock;
  QLineEdit *lineEdit;
  QListView *listView;
  QScrollArea *scrollArea;
  QWidget *widget;
  QLabel *label;
  QHBoxLayout *hLayout;
};

#endif // DHLRC_DHHELPUI_H
