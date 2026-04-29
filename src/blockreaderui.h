#ifndef BLOCKREADERUI_H
#define BLOCKREADERUI_H

#include "blockshowui.h"
#include "manageregionui.h"
#include "regionmodifyui.h"
#include <QPointer>
#include <QWidget>

// #include "blockshowui.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
class BlockReaderUI;
}
QT_END_NAMESPACE

class BlockReaderUI : public QWidget
{
  Q_OBJECT

public:
  BlockReaderUI (int index, ManageRegionUI *mr, QWidget *parent = nullptr);
  ~BlockReaderUI ();

private:
  Ui::BlockReaderUI *ui;
  QPointer<RegionModifyUI> rmui = nullptr;
  void *region;
  //   QString uuid = {};
  void setText ();
  char *large_version = nullptr;
  bool readerIsUnlocked = false;
  void *instance = nullptr;
  void closeEvent (QCloseEvent *event) override;
  BlockShowUI *bsui = nullptr;
  const void *nbt = nullptr;
  AutoLocker locker;

Q_SIGNALS:
  void changeVal (int value);
  void closeWin ();
  ;

private Q_SLOTS:
  void textChanged_cb ();
  void listBtn_clicked ();
  void entityBtn_clicked ();
  void propertyBtn_clicked ();
  void showBtn_clicked ();
};
#endif // BLOCKREADERUI_H