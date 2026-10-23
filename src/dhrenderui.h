#ifndef DHRENDERUI_H
#define DHRENDERUI_H

#include "dhlrc_render.h"
#include "dhwidget.h"
#include "regionclass.h"

#include <QColor>
#include <QFutureWatcher>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QWidget>
#include <array>
#include <atomic>
#include <memory>
#include <optional>

class QShowEvent;
class QSpinBox;
class DhRenderWidget;

/* A window showing one region in three dimensions.
 *
 * It owns nothing of the region: the scene is a snapshot taken while the
 * region's lock is held, so the view keeps drawing even if the region is
 * changed or closed afterwards. `Reload` takes a fresh snapshot.
 *
 * The scene is built on a worker thread: for a big region that takes long
 * enough that building it here froze the whole window (and the application).
 *
 * A version's block models and textures are only in its client jar, so when the
 * cache does not have one the window fetches it — about 30 MB, once per version
 * — as long as the `DownloadClientJar` setting allows it. The fetch goes
 * through `DhDownloadManager`, so a page that already asked for the same jar
 * (the material list does) leaves this one to share that download instead of
 * starting a second. The button fetches one on demand (and cancels a fetch that
 * is running).
 */
class DhRenderUI : public DhWidget
{
  Q_OBJECT
public:
  explicit DhRenderUI (std::shared_ptr<RegionClass> region,
                       QWidget *parent = nullptr);

protected:
  /* The colours are settings of the preview, and the panel may be reshown long
   * after they were changed on the settings page, so they are read again every
   * time it comes back. */
  void showEvent (QShowEvent *event) override;

private:
  void configureResources ();
  /* Builds the scene on a worker thread and hands it to the view, which takes
   * over its ownership. */
  void startSceneBuild ();
  void sceneBuilt (std::shared_ptr<DhlrcScene> scene);
  void setBuilding (bool building);
  void rebuild ();
  void updateSummary ();
  /* Hands the toolbar's slice controls to the view: the two spin boxes when
   * "Slice" is on, the whole region when it is off. The picture follows at
   * once, without another Reload. */
  void applySlice ();
  /* Points the spin boxes at the region's own height; a region that was
   * resized gets its old range clamped into the new one. */
  void updateSliceRange ();
  void describe (int x, int y, int z);
  /* Opens the property dialog for the block that was last clicked, and takes a
   * fresh look at the region when something was written. */
  void modifyPicked ();
  /* Opens the block chooser on the clicked block; `above` writes into the cell
   * above it instead, which is how a block is added — an empty cell is hard to
   * click on in a 3D view, a block you can see is not. */
  void pickBlock (bool above);
  /* Whether the buttons that write into the region are usable right now: a
   * block was clicked, it has properties to change, and no job holds the
   * region. */
  void updateEditButtons ();
  /* Builds the text the tooltip shows for the block under the pointer and
   * hands it to the view, which puts it beside the cursor. The bottom labels
   * are not touched by a hover. */
  void describeHover (int x, int y, int z);

  /* Reads the four view colours (highlight, background, region box, marker)
   * from the settings; called when the window is built and whenever it is
   * reshown. */
  void loadColourSettings ();
  /* Hands the cached colours to the view, which keeps them for every scene it
   * adopts later. */
  void applyColours ();
  /* A colour setting as a QColor, or `fallback` when it is not a colour. */
  [[nodiscard]] QColor colourSetting (const QString &key,
                                      const QColor &fallback) const;

  /* The cells a multi-selection may pick from: every cell of the region that is
   * not air. The view holds no region, so this is the owner's part of the
   * selection. */
  void enumerateCells (const std::function<void (int, int, int)> &visit);
  /* Turns the view's click selection on or off and updates the row around it. */
  void setSelectMode (bool on);
  void clearSelection ();
  /* The number of outlined cells, as the label beside the buttons says it. */
  void updateSelectionStatus ();
  /* Lists the selected cells in a table; choosing a row marks that cell and
   * describes it, so the buttons act on the block the user picked in the
   * list. */
  void showSelectedBlocks ();
  /* Marks `cell` in the view and makes it this panel's picked cell. */
  void selectCell (const std::array<int, 3> &cell);
  /* Replaces every selected cell with one block chosen in a single block
   * chooser, as one undoable change; without a selection it falls back to the
   * one-cell behaviour of `pickBlock (false)`. */
  void replaceSelection ();

  /* Whether the version's client jar is in the cache, and where it belongs. */
  int jarState () const;
  QString jarPath () const;
  /* Starts the fetch, or asks a running one to stop. */
  void toggleFetch ();
  void startFetch ();
  void finishFetch (const QString &error);
  void setFetching (bool fetching);
  /* The tag this window's version's client jar is shared under; every signal
   * of the manager is filtered by it. */
  QString jarDownloadTag () const;

  std::shared_ptr<RegionClass> regionClass;
  DhRenderWidget *view = nullptr;
  QLabel *summary = nullptr;
  QLabel *picked = nullptr;
  /* Says that the scene is being built; replaced by the view when it is. */
  QLabel *buildingLabel = nullptr;
  /* How far the scene build is, from the renderer's own report. The worker
   * only stores the number; this window polls it, so the worker never touches
   * a widget. */
  QProgressBar *buildProgressBar = nullptr;
  QTimer *buildProgressTimer = nullptr;
  std::shared_ptr<std::atomic<int>> buildProgress;
  QPushButton *reloadButton = nullptr;
  /* The slice and the region box the view draws, and the two ends of the slice
   * as the toolbar shows them. */
  QPushButton *sliceButton = nullptr;
  QSpinBox *sliceLowSpin = nullptr;
  QSpinBox *sliceHighSpin = nullptr;
  QPushButton *boundsButton = nullptr;
  /* Whether the spin boxes have been given the region's range yet: the first
   * one offers the whole region, a later reload only clamps them. */
  bool sliceRangeKnown = false;
  QPushButton *fetchButton = nullptr;
  /* The multi-selection toggle, the button that empties the outline, the list of
   * the cells it holds, and the count beside the buttons. */
  QPushButton *selectButton = nullptr;
  QPushButton *clearSelectionButton = nullptr;
  QPushButton *selectedBlocksButton = nullptr;
  QLabel *selectionLabel = nullptr;
  /* The colours of the outline, the box, the background and the marker. They
   * are kept here because the view needs them again for every scene it
   * adopts. */
  QColor highlightColour;
  QColor backgroundColor;
  QColor boundsColour;
  QColor markerColour;
  QPushButton *modifyButton = nullptr;
  QPushButton *replaceButton = nullptr;
  QPushButton *addButton = nullptr;
  QFutureWatcher<std::shared_ptr<DhlrcScene>> *sceneWatcher = nullptr;
  bool building = false;
  /* A reload requested while a build is running, done right after it. */
  bool rebuildPending = false;
  /* The block the last click named, so the property dialog has somewhere to
   * act; `hasPicked` says whether there is one. */
  bool hasPicked = false;
  qint32 pickedX = 0;
  qint32 pickedY = 0;
  qint32 pickedZ = 0;
  /* How many properties the clicked block's palette entry has; the property
   * dialog has nothing to do for a block without any. */
  int pickedPropertyCount = 0;
  /* The region's height, kept here because the buttons' enabled state follows
   * it and refreshing them must not take the region's lock: taking it emits
   * `lockedChanged`, which is what asks for the refresh — a lock-taking getter
   * in here recurses until the stack runs out. */
  qint32 regionWidth = 0;
  qint32 regionHeight = 0;
  qint32 regionDepth = 0;
  bool updatingButtons = false;
  bool fetching = false;
};

#endif /* DHRENDERUI_H */
