#ifndef BLOCKREADERUI_H
#define BLOCKREADERUI_H

#include "blockshowui.h"
#include "manageregionui.h"
#include "regionmodifyui.h"
#include "resourcegetter.h"
#include <QPointer>
#include <QWidget>
#include <memory>
#include <qobject.h>
#include <qtypes.h>

// #include "blockshowui.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
class BlockReaderUI;
}
QT_END_NAMESPACE

class BlockReaderUI : public DhWidget
{
  Q_OBJECT

public:
  BlockReaderUI (int index, std::shared_ptr<DhDownloader> downloader,
                 QWidget *parent = nullptr);
  ~BlockReaderUI ();
  static QString getBlockInfo (void *region, quint32 index,
                               const QString &path = QString{});

private:
  Ui::BlockReaderUI *ui;
  QPointer<RegionModifyUI> rmui = nullptr;
  void *region;
  //   QString uuid = {};
  void setText ();
  char *large_version = nullptr;
  bool readerIsUnlocked = false;
  void *instance = nullptr;
  BlockShowUI *bsui = nullptr;
  const void *nbt = nullptr;
  AutoLocker locker;
  QString objectPath;
  std::shared_ptr<DhDownloader> downloader;
  int version;

Q_SIGNALS:
  void changeVal (int value);
  void start ();
  void finishLoadingTranslation ();

private Q_SLOTS:
  void textChanged_cb ();
  void listBtn_clicked ();
  void entityBtn_clicked ();
  void propertyBtn_clicked ();
  void showBtn_clicked ();
};
#endif // BLOCKREADERUI_H
