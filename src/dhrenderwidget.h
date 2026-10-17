#ifndef DHRENDERWIDGET_H
#define DHRENDERWIDGET_H

#include "dhlrc_render.h"

#include <QImage>
#include <QPoint>
#include <QString>
#include <QWidget>
#include <memory>

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
  [[nodiscard]] const QString &problem () const { return problemText; }

  /* How many quads the scene holds and how many blocks were left out. */
  [[nodiscard]] quint64 quadCount () const;
  [[nodiscard]] quint64 skippedCount () const;

  /* Whether the picture is drawn by OpenGL rather than by the rasteriser. It
   * can turn false at any time: a GL frame that fails drops the widget back to
   * the CPU path, and `backendChanged ()` says when. */
  [[nodiscard]] bool usingGl () const { return useGpu; }

  /* Replaces the scene with one built from `region`, as the constructor does,
   * and frames it again. */
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

Q_SIGNALS:
  /* A click landed on a block, in the region's own coordinates. */
  void blockPicked (int x, int y, int z);
  /* The pointer moved onto another block, in the region's own coordinates.
   * Sent only when the answer changes, never for a move within one block. */
  void blockHovered (int x, int y, int z);
  /* The backend changed, so a status line saying which one is in use is
   * stale. */
  void backendChanged ();

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
};

#endif /* DHRENDERWIDGET_H */
