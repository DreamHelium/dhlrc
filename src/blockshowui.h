//
// Created by dream_he on 25-7-27.
//

#ifndef BLOCKSHOWUI_H
#define BLOCKSHOWUI_H

#include "dhbuttondelegate.h"
#include <QButtonGroup>
#include <QGridLayout>
#include <QProgressDialog>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTableView>
#include <QWidget>
#include <qevent.h>

QT_BEGIN_NAMESPACE
namespace Ui
{
class BlockShowUI;
}
QT_END_NAMESPACE

class BlockShowUI : public QWidget
{
  Q_OBJECT

public:
  explicit BlockShowUI (void *region, char *&large_version,
                        QWidget *parent = nullptr);
  ~BlockShowUI () override;

Q_SIGNALS:
  void changeVal (int val);
  void closeWin ();

private:
  bool modeSwitch = false;
  void initUI ();
  DhButtonDelegate *delegate = nullptr;

protected:
  void closeEvent (QCloseEvent *event) override;

private:
  QWidget *widget;
  Ui::BlockShowUI *ui;
  void *region;
  char *&large_version;
  QStandardItemModel *model = nullptr;

private Q_SLOTS:
  void updateUI ();
};

#endif // BLOCKSHOWUI_H
