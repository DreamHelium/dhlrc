#ifndef DHLRC_DHHELPUI_H
#define DHLRC_DHHELPUI_H

#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMainWindow>
#include <QPushButton>
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
  static void free ();

protected:
  // void resizeEvent (QResizeEvent *event) override;

private:
  QStandardItemModel *model;
  QSortFilterProxyModel *sortmodel;
  void showSome (const QString &str);
  QWidget *layoutWidget;
  QString loadingStr;
  QVBoxLayout *layout;
  QVBoxLayout *labelLayout;
  QWidget *labelWidget;
  QLineEdit *lineEdit;
  QListView *listView;
  QScrollArea *scrollArea;
  QLabel *label;
  QHBoxLayout *hLayout;
  QPushButton *pushButton;
};

#endif // DHLRC_DHHELPUI_H
