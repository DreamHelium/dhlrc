#ifndef DHRENDERWIDGET_H
#define DHRENDERWIDGET_H

#include "dhlrc_render.h"

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QString>
#include <QVector>
#include <QWidget>
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

class DhGpuWidget;
class QLabel;

/* Shows a region drawn in three dimensions, and lets the user turn it.
 *
 * The heavy work is in the core's `render` feature: this widget builds a scene
 * once (`dhlrc_render_scene_new`) and then asks for a picture whenever the
 * view moves. A drag renders at half size so it keeps up and a full-size
 * picture is drawn when the drag ends.
 *
 * The picture normally comes from the GPU: a `DhGpuWidget` child draws the
 * scene's own quads with OpenGL, which needs no half-size trick and no copy
 * back to the CPU. Where this machine cannot give a context, or where the
 * first GL frame fails, the software rasteriser draws a `QImage` exactly as it
 * always did. `usingGl ()` says which one is in use.
 *
 * Dragging with the left button turns the camera, with the middle or right
 * button slides it, and the wheel comes closer or moves away. A click without
 * a drag reports the block under the pointer. `R` frames the whole region and
 * `O` switches between the perspective and orthographic projections.
 *
 * The block under the pointer is also outlined while the mouse moves over the
 * picture, and `blockHovered ()` says which one it is so a tooltip can be put
 * beside the cursor. The outline is the core's own: the scene is told the box
 * and the rasteriser draws it, while the GL child uploads the quads the core
 * hands out for the same box.
 *
 * The picture can also be limited to a horizontal slice of the region, and the
 * extent of the region being drawn — its box, its Y clamped to the slice — can
 * be shown around it in another colour. Both are the core's own too: the slice
 * is a scene built from the range and the box is one more box the scene draws,
 * so the two backends need no special case for either.
 *
 * Cells can also be selected, and every selected cell is outlined like the one
 * under the pointer. In select mode a click adds the cell under the pointer to
 * the selection or takes it out again, and a click off the scene clears the
 * selection. The outlines themselves are the core's, one box per selected
 * cell, and the hover outline joins them in the same set rather than replacing
 * it.
 *
 * A marker box can be shown around one cell, apart from the selection and in
 * its own colour: the owner uses it for the one block a list has singled out,
 * which must stay visible over the other boxes.
 *
 * The colours of the boxes and of the panel behind the picture can be chosen;
 * they are kept here and applied again whenever a new scene is adopted,
 * because a scene carries its own copy of the colours.
 */
class DhRenderWidget : public QWidget
{
  Q_OBJECT
public:
  /* Builds a scene of `region` (a `Region *` from libregion_rs), reading the
   * resources `core` has configured. The caller must hold the region's lock
   * for the duration of the call: everything the scene needs is copied out of
   * it, and it is not touched again afterwards. */
  /* A view with no scene yet: the scene arrives through `setScene ()` once it
   * has been built (typically on a worker thread). */
  explicit DhRenderWidget (QWidget *parent = nullptr);
  /* Builds the scene here and now. Callers that must not block the GUI build
   * it elsewhere and call `setScene ()` instead. */
  DhRenderWidget (void *core, void *region, QWidget *parent = nullptr);
  ~DhRenderWidget () override;

  /* Shows a scene built elsewhere and shares its ownership. A scene built for
   * a big region can take a long time, which is exactly why the build is
   * allowed to happen off the GUI thread. */
  void setScene (std::shared_ptr<DhlrcScene> next);

  /* Whether a scene could be built; `problem ()` says why when it could not. */
  [[nodiscard]] bool ready () const { return scene != nullptr; }

  /* Draws only the blocks whose Y is between the pair's two values, both
   * included, and frames the view around them; nothing means the whole region.
   * The pair is in the region's own coordinates, and a low above a high draws
   * nothing.
   *
   * A scene this widget built itself (the blocking constructor and
   * `rebuild ()`) is taken again here, so the caller must hold the region's
   * lock around this call, as it does for the constructor. A scene handed in
   * through `setScene ()` came from a builder that holds the region, so this
   * only records the range and `sliceChanged ()` asks that owner for a fresh
   * picture; building it here would freeze the GUI for as long as a big region
   * takes. */
  void setSlice (const std::optional<std::pair<int, int>> &slice);
  [[nodiscard]] std::optional<std::pair<int, int>> slice () const
  {
    return sliceRange;
  }

  /* Shows or hides the box around the region being drawn, clamped to the
   * slice when one is active. The box is part of the scene's overlay, so a
   * toggle needs no new scene: the next picture reads it. */
  void setBoundsVisible (bool visible);
  [[nodiscard]] bool boundsVisible () const { return showBounds; }

  /* The size in blocks of the region being drawn, which the box above wraps.
   * The box's other coordinates are scene space, where the region starts at
   * (0, 0, 0). */
  void setRegionExtent (qint32 x, qint32 y, qint32 z);

  [[nodiscard]] const QString &problem () const { return problemText; }

  /* How many quads the scene holds and how many blocks were left out. */
  [[nodiscard]] quint64 quadCount () const;
  [[nodiscard]] quint64 skippedCount () const;

  /* Whether the picture is drawn by OpenGL rather than by the rasteriser. It
   * can turn false at any time: a GL frame that fails drops the widget back to
   * the CPU path, and `backendChanged ()` says when. */
  [[nodiscard]] bool usingGl () const { return useGpu; }

  /* Replaces the scene with one built from `region`, as the constructor does,
   * and frames it again. The current slice is applied, so this is also what a
   * caller that lets the widget build uses to change from one slice to
   * another. */
  void rebuild (void *core, void *region);

  /* Frames the whole region again. */
  void resetView ();
  [[nodiscard]] bool orthographic () const;
  void setOrthographic (bool on);

  /* Sets the text of the tooltip shown beside the pointer. The view shows it
   * when a block is hovered and hides it otherwise; an empty text hides it.
   * The caller builds the text, because only it holds the region the block's
   * name comes from. */
  void setHoverInfo (const QString &text);

  /* ---- Selecting cells ---------------------------------------------------
   *
   * A cell is a whole-numbered block position, the same coordinates
   * `blockPicked ()` and `blockHovered ()` carry. */
  using BlockCell = std::array<int, 3>;

  /* The owner's way of visiting the region's cells: it calls the callback once
   * for every existing cell. The click selection does not need it — a click
   * already names its cell — but it is kept for owners that walk the region
   * themselves. */
  using BlockEnumerator
      = std::function<void (const std::function<void (int, int, int)> &)>;

  /* Whether a click selects cells rather than naming one. In select mode a
   * click on a block toggles it in or out of the outlined set, and a click off
   * the scene clears the set; outside it a click only reports its cell through
   * `blockPicked ()` and `cellPicked ()`. Dragging turns the camera either way. */
  void setSelectMode (bool on);
  [[nodiscard]] bool selectMode () const { return selectModeOn; }

  /* Replaces the outlined set. Called before a scene is adopted the cells are
   * kept and outlined on that scene, so this works while the scene is still
   * being built. */
  void setSelectedCells (const QVector<BlockCell> &cells);
  [[nodiscard]] const QVector<BlockCell> &selectedCells () const
  {
    return selection;
  }

  /* Hands the view the only way it can know which cells exist. Not needed for
   * the click selection; kept for owners that visit the region themselves. */
  void setBlockEnumerator (BlockEnumerator enumerator);

  /* The colour of the outline around the selected and hovered cells, and of
   * the box around the region being drawn. Applied to the scene now and kept
   * for every scene adopted later. */
  void setHighlightColour (const QColor &colour);
  void setBoundsColour (const QColor &colour);

  /* The colour painted behind the picture. The GL child clears to it too, so
   * the two backends agree. */
  void setBackgroundColour (const QColor &colour);

  /* ---- Marking one cell -------------------------------------------------
   *
   * The marker is one box drawn apart from the selection and in its own
   * colour, for the cell a list has singled out. It survives a scene rebuild
   * like the selection does. */
  void setMarkerCell (std::optional<std::array<int, 3>> cell);
  [[nodiscard]] std::optional<std::array<int, 3>> markerCell () const
  {
    return marker;
  }
  void setMarkerColour (const QColor &colour);

Q_SIGNALS:
  /* A click landed on a block, in the region's own coordinates. */
  void blockPicked (int x, int y, int z);
  /* A click outside select mode names its cell, or says there is none with
   * -1, -1, -1. The panel describes the block from this, so empty space can
   * clear a description too. */
  void cellPicked (int x, int y, int z);
  /* The pointer moved onto another block, in the region's own coordinates.
   * Sent only when the answer changes, never for a move within one block. */
  void blockHovered (int x, int y, int z);
  /* The backend changed, so a status line saying which one is in use is
   * stale. */
  void backendChanged ();
  /* Another slice was asked for on a scene this widget did not build, so the
   * owner that holds the region has to build the fresh one. */
  void sliceChanged ();
  /* A click in select mode changed the outlined set. Not sent for
   * `setSelectedCells ()`, which is the owner's own change. */
  void selectionChanged ();

protected:
  void paintEvent (QPaintEvent *event) override;
  void resizeEvent (QResizeEvent *event) override;
  void mousePressEvent (QMouseEvent *event) override;
  void mouseMoveEvent (QMouseEvent *event) override;
  void mouseReleaseEvent (QMouseEvent *event) override;
  void leaveEvent (QEvent *event) override;
  void wheelEvent (QWheelEvent *event) override;
  void keyPressEvent (QKeyEvent *event) override;

private:
  /* The implementation shared by `setScene ()` and the widget's own builds. */
  void adoptScene (std::shared_ptr<DhlrcScene> next);
  /* Builds the scene here from the region `rebuild ()` stored, with the
   * current slice. */
  void buildOwnScene ();
  /* Hands the scene the bounds box the current state asks for: the region's
   * extent, its Y clamped to the slice, or nothing when no box is shown. */
  void applyBounds ();
  /* Fills `box` with the six floats of that box. False when there is nothing
   * to show a box around — no extent yet, or a slice that draws no layer. */
  [[nodiscard]] bool boundsBox (float *box) const;
  /* Converts every selected cell to the box the scene draws it in, six floats
   * each. Redone whenever a new scene is adopted, because the boxes belong to
   * that scene. */
  void applySelectionBoxes ();
  /* Hands the scene the whole outline set — the selected cells and the cell
   * under the pointer — through the multi-box call, so a hover adds its box to
   * the selection instead of replacing it. */
  void rebuildHighlights ();
  /* Hands the scene the two outline colours, the marker's colour and the GL
   * child its background, which is what a scene adopted later must be told
   * again. */
  void applyColours ();
  /* Adds the cell under `at` to the selection or takes it out again; off the
   * scene it clears the selection. */
  void toggleCellAt (const QPoint &at);
  /* Draws the marker box around `marker` on the current scene, or clears it. */
  void applyMarker ();

  /* The cell under `at`, in the region's own coordinates. False when no cell
   * was hit or no scene is up. */
  [[nodiscard]] bool cellAt (const QPoint &at, int32_t *cell) const;
  void pickAt (const QPoint &at);
  /* Points the highlight at the block under `at`, clearing it when the pointer
   * is off the scene. Nothing is touched when the same block is still under
   * the pointer, so a move within a block never redraws. */
  void hoverAt (const QPoint &at);
  /* Takes the outline and the tooltip away. */
  void clearHover ();
  /* Picks again where the pointer last was, after the camera moved under it. */
  void rehover ();
  /* Puts the tooltip beside the last pointer position, inside the widget. */
  void placeTip ();
  void discard ();
  /* Hands the camera to the GL child and repaints; every camera change goes
   * through here so the two backends never disagree. */
  void applyCamera ();
  /* The GL child reports whether its frame drew; a failure falls back to the
   * CPU picture rather than showing nothing. */
  void onFrameDrawn (bool ok);

  std::shared_ptr<DhlrcScene> scene;
  DhlrcCamera camera{};
  QString problemText;
  /* The Y range being drawn, empty for the whole region, and whether the box
   * around it is shown. */
  std::optional<std::pair<int, int>> sliceRange;
  bool showBounds = false;
  /* The region's size in blocks, so the bounds box can wrap it. */
  qint32 extent[3] = { 0, 0, 0 };
  /* What the scene was built from, when this widget built it: a slice change
   * can then take a fresh scene here. `ownScene` says whether that is still
   * the case or the scene arrived through `setScene ()`. */
  void *sceneCore = nullptr;
  void *sceneRegion = nullptr;
  bool ownScene = false;
  /* Whether the next scene should be framed again rather than kept at the
   * camera the user left. A slice draws a different extent, so it needs its
   * own framing; a plain reload of the same region does not. */
  bool refitPending = false;
  /* The OpenGL child, when this machine can give a context, and whether it is
   * the one drawing. */
  DhGpuWidget *gpu = nullptr;
  bool useGpu = false;
  /* The pointer of the last press and whether the mouse has moved since, so a
   * click can be told from a drag. */
  QPoint lastPos;
  Qt::MouseButton dragButton = Qt::NoButton;
  bool dragging = false;
  bool moved = false;
  bool framed = false;
  /* The block the outline is around, and where the pointer was when it was
   * found, so the tooltip can follow a pointer that stays over it. */
  QLabel *tip = nullptr;
  QPoint hoverPoint;
  int hoverBlock[3] = { 0, 0, 0 };
  bool hovered = false;
  /* Whether a move has ever been seen, so the camera cannot re-pick under a
   * pointer the widget never had. */
  bool pointerKnown = false;
  /* The outlined set and the boxes it becomes on the current scene, six floats
   * per cell. The boxes are kept so a hover only adds its own box instead of
   * converting every selected cell again. */
  QVector<BlockCell> selection;
  std::vector<float> selectionBoxes;
  BlockEnumerator enumerator;
  /* Multi-select mode: whether a click toggles a cell or only reports it. */
  bool selectModeOn = false;
  /* The cell the marker box wraps, when one is shown. */
  std::optional<std::array<int, 3>> marker;
  /* The box colours and the panel behind the picture. The defaults are the
   * core's own colours and the panel colour the CPU view always painted, so a
   * view nobody configures looks exactly as it did. */
  QColor highlightColour{ 0, 0, 0 };
  QColor boundsColour{ 0, 200, 255 };
  QColor backgroundColour{ 0x20, 0x28, 0x30 };
  QColor markerColour{ 255, 32, 96 };
};

#endif /* DHRENDERWIDGET_H */
