#ifndef BLOCKPICKERUI_H
#define BLOCKPICKERUI_H

#include "regionclass.h"

#include <QDialog>
#include <QFutureWatcher>
#include <QHash>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>
#include <memory>
#include <mutex>

QT_BEGIN_NAMESPACE
namespace Ui
{
class BlockPickerUI;
}
QT_END_NAMESPACE

class QComboBox;
class QLabel;
class QSpinBox;
struct DhlrcCore;

/* Chooses a block out of the version's own block catalog and sets it at one
 * position, with its properties.
 *
 * The catalog is built by the core from the resources it is configured with
 * (the version's client jar), so the list holds what the version really has
 * and every property is offered with the values its blockstates use. The
 * strings are copied out at once: the ABI only lends them until the resources
 * are configured again.
 *
 * Building the catalog reads every blockstate of the version, which takes a
 * noticeable moment the first time. That build runs on a worker while the
 * dialog shows a placeholder, so the window opens at once; copying the finished
 * list into the property combos is a short loop on the GUI thread, which keeps
 * the borrowed strings from being held across an event loop turn. A built list
 * is kept per data version, so reopening the dialog does not build it again.
 *
 * `x`, `y`, `z` is the cell the dialog writes by default, and the block there
 * is the state prefilled. "Place one block above" moves the write to `y + 1`,
 * which is how a block is added: an empty cell is hard to point at in a 3D
 * view, the block below it is not. A property the chosen block shares with the
 * prefilled state keeps the prefilled value (a stone stair keeps the
 * facing/half/shape of the oak stair it replaces); the others start from the
 * value the version's blockstates name first.
 *
 * The constructor shape is used by the 3D preview as well, so it must stay as
 * it is.
 */
class BlockPickerUI : public QDialog
{
  Q_OBJECT
public:
  BlockPickerUI (std::shared_ptr<RegionClass> region, qint32 x, qint32 y,
                 qint32 z, QWidget *parent = nullptr);
  ~BlockPickerUI () override;

  /* The state the property values are carried over from. Must be called before
   * the dialog is shown; without it the block at `x`, `y`, `z` is used, which
   * is what the block reader wants. A call made while the catalog is still
   * loading is remembered: the block is selected once the list arrives. */
  void setCurrentState (const QString &name,
                        const QList<QPair<QString, QString>> &properties);

  /* Pre-checks "place one block above", for a caller whose own action is
   * adding a block above the one it picked (the 3D view, for one). The user
   * can still untick it. */
  void setPlaceAbove (bool above);

  /* The cell the dialog would write to right now — the typed position, plus the
   * "one block above" shift — so a caller can show what was written. */
  void target (qint32 *x, qint32 *y, qint32 *z) const;

Q_SIGNALS:
  /* Emitted after a write succeeded, so the caller can refresh its view of the
   * block. The region's own `changed` fires as well. */
  void blockModified ();

private:
  /* One block of the catalog: its id and, for each property, the values the
   * version's blockstates use for it. */
  struct BlockEntry
  {
    QString name;
    QList<QPair<QString, QStringList>> properties;
  };

  /* What the catalog worker produced. `configured` says this dialog had to
   * point the core at the version, which is the only case where the catalog is
   * known to belong to the dialog's data version. */
  struct CatalogBuild
  {
    size_t count = 0;
    bool configured = false;
  };

  /* The catalogs read so far, per data version. The core keeps its own copy,
   * but that one is core-global and is dropped whenever any page configures
   * the resources again; this keeps reopening the dialog off that state. The
   * mutex also makes the one build at a time rule explicit. */
  struct CatalogCache
  {
    std::mutex mutex;
    QHash<quint32, QVector<BlockEntry>> blocks;
  };
  static CatalogCache &catalogCache ();

  Ui::BlockPickerUI *ui;
  std::shared_ptr<RegionClass> regionClass;
  qint32 blockX;
  qint32 blockY;
  qint32 blockZ;
  /* The region size, read once: `updateTarget ()` runs from the lock-change
   * signal, and a region getter inside it would take the lock again and start
   * the signal over. */
  qint32 regionX = 0;
  qint32 regionY = 0;
  qint32 regionZ = 0;
  QVector<BlockEntry> blocks;
  /* The current property rows, in the selected block's property order. */
  QList<QComboBox *> propertyCombos;
  /* The state the property values are carried over from. */
  QString currentName;
  QList<QPair<QString, QString>> currentProperties;
  QLabel *lockedHint = nullptr;
  /* The cell can be typed as well as pointed at: an empty cell cannot be
   * clicked in a 3D view, and a block a few cells away is easier to name. */
  QSpinBox *positionX = nullptr;
  QSpinBox *positionY = nullptr;
  QSpinBox *positionZ = nullptr;
  /* While the catalog is being built the list holds a placeholder instead of
   * blocks, and nothing counts as chosen yet. */
  bool catalogLoading = false;
  QFutureWatcher<CatalogBuild> *catalogWatcher = nullptr;

  /* One axis of the cell to write, offering the region's own extent. */
  QSpinBox *makePositionSpin (qint32 value, qint32 size);
  void loadCatalog ();
  /* Shows the single "loading" row the list carries until the catalog is here. */
  void showCatalogPlaceholder ();
  /* Copies the built catalog out of the core; a short loop on the GUI thread. */
  void copyCatalog ();
  /* The catalog is ready: fill the list and select what the dialog had. */
  void catalogBuilt (const CatalogBuild &built);
  /* The worker: builds the version's catalog in the core, off the GUI thread. */
  static CatalogBuild buildCatalog (DhlrcCore *core, quint32 dataVersion);
  void populateBlockList ();
  void selectBlock (const QString &name);
  void showProperties ();
  [[nodiscard]] const BlockEntry *selectedBlock () const;
  /* Refreshes the target label and greys the accept button out when the write
   * cannot work (no block chosen, cell above outside the region, locked). */
  void updateTarget ();
  void setEditable (bool editable);
  [[nodiscard]] QList<QPair<QString, QString>> chosenProperties () const;

private Q_SLOTS:
  void positionChanged_cb ();
  void filterChanged_cb (const QString &text);
  void selectionChanged_cb ();
  void aboveToggled_cb (bool checked);
  void okBtn_clicked ();
};

#endif /* BLOCKPICKERUI_H */
