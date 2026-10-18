#ifndef BLOCKREADERUI_H
#define BLOCKREADERUI_H

#include "blocklistui.h"
#include "blockshowui.h"
#include "dhrenderui.h"
#include "manageregionui.h"
#include "nbtreaderui.h"
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

class QAction;
class QMenu;

/* Reads one region: shows its base data, names the block at a coordinate and
 * opens the tools that go with it (block list, block view, block properties,
 * entity NBT, the 3D view).
 *
 * The per-region pages that belong beside the reader (the modifier, the change
 * history and the material list) are pages of the workspace this reader lives
 * in, so the reader asks for them through signals instead of opening tabs of
 * its own. Every tool that is a window is tracked, so closing the reader
 * closes them too. The reader holds the region with a shared_ptr, so it also
 * survives the region being removed from the list while a tool is still open.
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

  /* Shows the region again after someone else wrote to it (the modifier's
   * edits, for one). */
  void refresh ();
  /* Whether the block names are translated, as the material list wants to
   * know. */
  [[nodiscard]] bool translation () const { return translationReady; }

  /* The tools the reader offers. They are menu actions in the main window,
   * not buttons on the page, so the page itself stays clean. */
  void modifyRegion () { Q_EMIT modifierRequested (); }
  void showChangeHistory () { Q_EMIT changesRequested (); }
  void showMaterialList () { Q_EMIT materialsRequested (); }
  void showEntityList ();
  void showEntityNbt ();
  void modifyProperty ();
  void showBlockList ();
  void showBlockView ();
  void showPreview ();

  /* What the matching menu action may do right now; the tools menu calls these
   * to grey an action out. */
  [[nodiscard]] bool canModifyRegion () const
  {
    return regionClass && !regionClass->locked ();
  }
  [[nodiscard]] bool canModifyProperty () const
  {
    return canModifyRegion () && blockHasProperties;
  }
  [[nodiscard]] bool hasBlockEntity () const { return nbt != nullptr; }
  [[nodiscard]] bool hasEntities () const;
protected:
  void closeEvent (QCloseEvent *event) override;

private:
  Ui::BlockReaderUI *ui;
  std::shared_ptr<RegionClass> regionClass;
  /* What this reader opened. A QPointer notices the ones the user closed in
   * the meantime, so `closeChildren ()` only touches what is still there. */
  QPointer<BlockShowUI> bsui = nullptr;
  QPointer<BlockListUI> blui = nullptr;
  QPointer<NbtReaderUI> nrui = nullptr;
  QPointer<DhRenderUI> renderUi = nullptr;
  /* The tools button opens this menu; its actions are greyed out while a job
   * holds the region, or when the block under the coordinates has nothing to
   * show. */
  QMenu *toolsMenu = nullptr;
  QAction *modifyAction = nullptr;
  QAction *propertyAction = nullptr;
  QAction *entityListAction = nullptr;
  QAction *entityNbtAction = nullptr;
  void updateToolActions ();
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
  /* The reader's own tools that are pages of its workspace, not tabs. */
  void changesRequested ();
  void modifierRequested ();
  void materialsRequested ();
  /* Something the tools menu depends on changed (the lock, the block under
   * the coordinates, the translation), so its actions have to be re-checked. */
  void toolsChanged ();

private Q_SLOTS:
  void textChanged_cb ();
};

#endif /* BLOCKREADERUI_H */
