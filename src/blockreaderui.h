#ifndef BLOCKREADERUI_H
#define BLOCKREADERUI_H

#include "blocklistui.h"
#include "blockshowui.h"
#include "dhrenderui.h"
#include "manageregionui.h"
#include "materiallistui.h"
#include "nbtreaderui.h"
#include "regionchangesui.h"
#include "regionmodifyui.h"
#include "resourcegetter.h"
#include <QPointer>
#include <QWidget>
#include <memory>
#include <qobject.h>
#include <qtypes.h>

QT_BEGIN_NAMESPACE
namespace Ui
{
class BlockReaderUI;
}
QT_END_NAMESPACE

/* Reads one region: shows its base data, names the block at a coordinate and
 * opens the tools that go with it (block list, block view, block properties,
 * entity NBT, the region modifier and the 3D view).
 *
 * Every tool it opens is tracked, so closing the reader closes them too: a
 * window left behind would keep showing a region the reader kept alive, and a
 * page in the tab bar would outlive the reader that owns it. The reader holds
 * the region with a shared_ptr, so it also survives the region being removed
 * from the list while a tool is still open.
 */
class BlockReaderUI : public DhWidget
{
  Q_OBJECT

public:
  BlockReaderUI (int index, std::shared_ptr<DhDownloader> downloader,
                 QWidget *parent = nullptr);
  ~BlockReaderUI () override;
  static QString getBlockInfo (void *region, quint32 index,
                               bool hasTranslation = false);
protected:
  void closeEvent (QCloseEvent *event) override;

private:
  Ui::BlockReaderUI *ui;
  std::shared_ptr<RegionClass> regionClass;
  /* What this reader opened. A QPointer notices the ones the user closed in
   * the meantime, so `closeChildren ()` only touches what is still there. */
  QPointer<RegionModifyUI> rmui = nullptr;
  QPointer<BlockShowUI> bsui = nullptr;
  QPointer<BlockListUI> blui = nullptr;
  QPointer<NbtReaderUI> nrui = nullptr;
  QPointer<DhRenderUI> renderUi = nullptr;
  QPointer<RegionChangesUI> changesUi = nullptr;
  QPointer<MaterialListUI> materialUi = nullptr;
  void closeChildren ();
  void setText ();
  const void *nbt = nullptr;
  /* Whether the block the coordinates name has properties, so the property
   * button can stay disabled while a job holds the region. */
  bool blockHasProperties = false;
  bool translationReady = false;
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

#endif /* BLOCKREADERUI_H */
