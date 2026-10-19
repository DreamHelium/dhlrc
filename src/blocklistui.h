#ifndef BLOCKLISTUI_H
#define BLOCKLISTUI_H

#include <QWidget>
#include <qsortfilterproxymodel.h>
#include <qstandarditemmodel.h>

#include <atomic>
#include <memory>
#include <thread>

QT_BEGIN_NAMESPACE
namespace Ui
{
class BlockListUI;
}
QT_END_NAMESPACE

class RegionClass;

/* Lists every block of a region.
 *
 * The scan runs on a worker thread: a big region has millions of blocks, and
 * doing that on the GUI thread froze the window. The worker never touches a
 * widget or the model — it hands whole rows to the GUI thread in batches, so
 * the view and the model stay on the thread they belong to. The region is held
 * with a shared_ptr and locked while it is read, so it can neither be freed
 * nor changed under the scan.
 */
class BlockListUI : public QWidget
{
  Q_OBJECT

public:
  BlockListUI (std::shared_ptr<RegionClass> region, bool translationReady,
               QWidget *parent = nullptr);
  ~BlockListUI ();
  void updateBlockList ();
  /* Whether block names are translated. The list is built once; asking it to
   * translate again rebuilds the rows so the names appear without the window
   * being closed and opened. */
  void setTranslationReady (bool ready);

Q_SIGNALS:
  void setModel ();
  /* The list is gone as far as the user is concerned; the reader shows why. */
  void stopProcess ();
  void setValue (int value);

private:
  /* One batch of rows handed from the worker to the GUI thread. Anything the
   * GUI thread did not take (because the window was closed first) is freed by
   * the destructor. */
  struct ItemBatch
  {
    QList<QList<QStandardItem *>> rows;
    ItemBatch () = default;
    ItemBatch (const ItemBatch &) = delete;
    ItemBatch &operator= (const ItemBatch &) = delete;
    ~ItemBatch ()
    {
      for (auto &row : rows)
        qDeleteAll (row);
    }
  };

  /* The scan itself; runs on the worker thread and only posts batches. */
  void scanBlocks (QVector<QString> translations, bool ignoreAir);

  std::shared_ptr<RegionClass> regionClass;
  bool translationReady;
  Ui::BlockListUI *ui;
  QStandardItemModel *model;
  QSortFilterProxyModel *proxyModel;
  std::thread worker;
  std::atomic<bool> stop{ false };
  /* Whether the user chose to list air as well; asked once, in the
   * constructor. */
  bool ignoreAir = false;

private Q_SLOTS:
  void textChanged_cb (const QString &str);
};

#endif // BLOCKLISTUI_H
