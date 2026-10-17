#ifndef DHLRC_NBTREADERUI_H
#define DHLRC_NBTREADERUI_H
#include "dhtreefilter.h"
#include <QList>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QWidget>
#include <qdialog.h>

QT_BEGIN_NAMESPACE
namespace Ui
{
class NbtReaderUI;
}
QT_END_NAMESPACE

/* Shows one NBT object as a tree.
 *
 * The tree is built by `buildItems ()`, which only reads the NBT data and can
 * therefore run on a worker thread; a big file takes long enough that building
 * it on the GUI thread would freeze the window. The constructor that takes the
 * items is for exactly that case.
 */
class NbtReaderUI : public QDialog
{
  Q_OBJECT

public:
  /* Builds the tree right here. For a small NBT (the data of one entity) that
   * is fine; for a whole file use `buildItems ()` on a worker thread first. */
  explicit NbtReaderUI (const void *nbt, bool fromFile,
                        QWidget *parent = nullptr);
  /* Shows an item tree that was built elsewhere and takes ownership of the
   * items. */
  explicit NbtReaderUI (QList<QStandardItem *> roots, QWidget *parent = nullptr);
  ~NbtReaderUI () override;
  void disableClose ();

  /* The whole tree of `nbt` as its top-level items. Pure reads of the NBT
   * data, so it may be called on any thread; the items belong to the caller. */
  static QList<QStandardItem *> buildItems (const void *nbt, bool fromFile);

private:
  void initUi (QList<QStandardItem *> roots);

  Ui::NbtReaderUI *ui;
  const void *nbt;
  QStandardItemModel *model;
  DhTreeFilter *proxyModel;

private Q_SLOTS:
};

#endif // DHLRC_NBTREADERUI_H
