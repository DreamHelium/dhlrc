#include "dhrenderwidget.h"

#include "dhgpuwidget.h"

#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QRect>
#include <QResizeEvent>
#include <QTimer>
#include <QToolTip>
#include <QWheelEvent>
#include <libintl.h>
#include <algorithm>
#include <cmath>
#include <qnamespace.h>

#define _(str) gettext (str)

namespace
{
/* How much of a degree one pixel of a turn is worth. */
const double TURN_PER_PIXEL = 0.4;
/* How much of the distance one wheel notch takes away. */
const double ZOOM_PER_NOTCH = 0.88;

/* How long the widget waits after the last resize before it looks up what is
 * under the pointer again. A drag of the window edge sends a resize for every
 * pixel, and the projection has to follow all of them, but the pick behind the
 * outline and the tooltip only has to be right once the size has settled;
 * doing it per step is what makes the outline and the tooltip wander while the
 * picture is still changing size. */
const int HOVER_SETTLE_MS = 120;

/* How often the GL child is resized while a window edge is being dragged.
 * The child is a window of its own, so every resize makes the window system
 * lay out a new strip before the renderer's next frame arrives; at a pixel
 * per event that is many more resizes than frames, and each one is a moment
 * the strip can show the window background. Handing the size over at 30 Hz
 * keeps the child at the display's own rate while the parent paints its
 * background in the strip the child does not cover yet. */
const int RESIZE_INTERVAL_MS = 33;

/* The three 0..1 components the renderer's colour calls take. */
void
colourFloats (const QColor &colour, float *out)
{
  out[0] = static_cast<float> (colour.redF ());
  out[1] = static_cast<float> (colour.greenF ());
  out[2] = static_cast<float> (colour.blueF ());
}

/* The relative luminance of a colour, as WCAG defines it: the 0..1 value a
 * black-and-white guide would give it. Two colours whose luminances are close
 * look alike however far apart their channels are, so contrast is measured on
 * this rather than on the largest channel difference. */
double
relativeLuminance (const QColor &colour)
{
  const auto channel = [] (int value) {
    const double unit = value / 255.0;
    return unit <= 0.03928
               ? unit / 12.92
               : std::pow ((unit + 0.055) / 1.055, 2.4);
  };
  return 0.2126 * channel (colour.red ()) + 0.7152 * channel (colour.green ())
         + 0.0722 * channel (colour.blue ());
}

/* The WCAG contrast ratio of two colours: 1 for the same colour, 21 for black
 * on white. A ratio of 3 is the floor for a graphic that has to be told apart
 * from what is behind it. */
double
contrastRatio (const QColor &a, const QColor &b)
{
  const double first = relativeLuminance (a);
  const double second = relativeLuminance (b);
  return (qMax (first, second) + 0.05) / (qMin (first, second) + 0.05);
}

/* Black or white, whichever stands out more on `background`. The 0.179 is the
 * luminance where the two contrast ratios are equal. */
QColor
contrastColourFor (const QColor &background)
{
  return relativeLuminance (background) > 0.179 ? QColor (0, 0, 0)
                                                : QColor (255, 255, 255);
}

/* The colour the selection's outline is drawn in.
 *
 * It is the configured highlight colour when that stands out from the
 * background, and otherwise black or white, whichever the background is not.
 * The default highlight is black, and on the dark background it left the
 * picked cell looking untouched: the hover box was already there, so a click
 * that swapped it for an invisible box seemed to do nothing. */
QColor
highlightColourFor (const QColor &background, const QColor &configured)
{
  return contrastRatio (configured, background) >= 3.0
             ? configured
             : contrastColourFor (background);
}

/* The colour the box under the pointer is drawn in.
 *
 * It has to stand out from the background, and from the picked colour too: a
 * click picks exactly the block the pointer is already over, so if both boxes
 * had the same colour the press would leave the outline looking unchanged. On
 * the background where the contrasting black or white is the picked colour as
 * well, a saturated cyan takes over so the two stay apart. */
QColor
hoverColourFor (const QColor &background, const QColor &picked)
{
  const QColor contrast = contrastColourFor (background);
  if (contrastRatio (contrast, picked) >= 3.0)
    return contrast;
  return relativeLuminance (background) > 0.179 ? QColor (0, 100, 220)
                                                : QColor (0, 229, 255);
}
} // namespace

DhRenderWidget::DhRenderWidget (QWidget *parent) : QWidget (parent)
{
  camera.yaw = 45.0;
  camera.pitch = 30.0;
  camera.distance = 16.0;
  camera.fov = 45.0;
  setFocusPolicy (Qt::StrongFocus);
  setMinimumSize (240, 180);
  setCursor (Qt::OpenHandCursor);
  /* Every move asks what is under the pointer, so the outline and the tooltip
   * follow the mouse without a button held down. */
  setMouseTracking (true);

  /* A plain child label is the tooltip: it is transparent to mouse events, so
   * it can never swallow a drag or a click, and it can be raised over the GL
   * child when one is drawing. The text arrives through `setHoverInfo ()`,
   * because the block's name lives in the region this widget does not hold. */
  tip = new QLabel (this);
  tip->setTextFormat (Qt::PlainText);
  tip->setPalette (QToolTip::palette ());
  tip->setAutoFillBackground (true);
  tip->setFrameShape (QFrame::Box);
  tip->setAttribute (Qt::WA_TransparentForMouseEvents);
  tip->setAttribute (Qt::WA_ShowWithoutActivating);
  tip->hide ();

  /* Looks the block under the pointer up again once a resize has stopped; see
   * `resizeEvent ()`. */
  hoverSettle = new QTimer (this);
  hoverSettle->setSingleShot (true);
  hoverSettle->setInterval (HOVER_SETTLE_MS);
  connect (hoverSettle, &QTimer::timeout, this, [this] { rehover (); });

  /* Keeps the size handed to the GL child at a steady rate; see
   * `resizeEvent ()`. */
  resizeThrottle = new QTimer (this);
  resizeThrottle->setSingleShot (true);
  resizeThrottle->setInterval (RESIZE_INTERVAL_MS);
  connect (resizeThrottle, &QTimer::timeout, this,
           [this] { applyViewSize (); });

  /* The GPU child is the picture; the parent keeps the events, the CPU
   * fallback and the problem text. A machine that cannot give a context gets
   * no child at all. */
  if (DhGpuWidget::available ())
    {
      useGpu = true;
      gpu = new DhGpuWidget (this);
      /* The parent handles every drag, wheel and click, exactly as it does
       * over the CPU picture. */
      gpu->setAttribute (Qt::WA_TransparentForMouseEvents);
      gpu->setFocusPolicy (Qt::NoFocus);
      gpu->hide ();
      connect (gpu, &DhGpuWidget::frameDrawn, this,
               &DhRenderWidget::onFrameDrawn);
      /* The two backends must clear to the same colour, so the child starts
       * from the one this view paints. */
      gpu->setBackgroundColour (backgroundColour);
    }

}

DhRenderWidget::DhRenderWidget (void *core, void *region, QWidget *parent)
    : DhRenderWidget (parent)
{
  rebuild (core, region);
}

DhRenderWidget::~DhRenderWidget () { discard (); }

void
DhRenderWidget::discard ()
{
  /* The GL child borrows the scene, so it lets go before the scene goes. */
  if (gpu)
    gpu->setScene (nullptr);
  scene.reset ();
  /* The boxes were the old scene's, six floats per selected cell. */
  selectionBoxes.clear ();
}

void
DhRenderWidget::rebuild (void *core, void *region)
{
  /* Remembering what the scene came from lets a later slice change take a
   * fresh one without asking the caller, which is what the blocking
   * constructor's callers expect. */
  sceneCore = core;
  sceneRegion = region;
  buildOwnScene ();
}

void
DhRenderWidget::buildOwnScene ()
{
  if (sceneCore == nullptr || sceneRegion == nullptr)
    {
      ownScene = false;
      return;
    }
  DhlrcScene *raw = nullptr;
  if (sliceRange)
    raw = dhlrc_render_scene_new_slice (sceneCore, sceneRegion,
                                        sliceRange->first, sliceRange->second);
  else
    raw = dhlrc_render_scene_new (sceneCore, sceneRegion);
  adoptScene (std::shared_ptr<DhlrcScene> (raw, dhlrc_render_scene_free));
  /* Only now: `adoptScene ()` is also the path an outside builder's scene
   * takes, where this widget must not build again. */
  ownScene = true;
}

void
DhRenderWidget::setScene (std::shared_ptr<DhlrcScene> next)
{
  ownScene = false;
  adoptScene (std::move (next));
}

void
DhRenderWidget::adoptScene (std::shared_ptr<DhlrcScene> next)
{
  /* The outline belongs to the old scene, and a tooltip naming one of its
   * blocks must not stay up. */
  hovered = false;
  if (tip)
    tip->hide ();
  discard ();
  scene = std::move (next);
  problemText = scene
                    ? QString ()
                    : QString (_ ("The region could not be drawn: no block in "
                                  "it could be resolved. Check that the "
                                  "resource files for this version have been "
                                  "downloaded in the settings."));
  framed = false;
  /* A different slice draws a different extent, so it is framed again; a plain
   * reload of the same region keeps the camera where the user left it. */
  if (refitPending && scene != nullptr && width () > 1 && height () > 1)
    {
      dhlrc_render_fit (scene.get (), &camera, width (), height ());
      framed = true;
    }
  refitPending = false;
  if (gpu)
    {
      gpu->setScene (scene.get ());
      gpu->setGeometry (rect ());
      if (scene != nullptr && useGpu)
        {
          gpu->setCamera (camera);
          gpu->show ();
          gpu->raise ();
        }
      else
        gpu->hide ();
    }
  /* A fresh scene carries neither boxes nor colours, so both are put on it
   * here: the region's box, the selected cells, the marker and the colours.
   * The pointer's own outline follows through `rehover ()`. */
  applyColours ();
  applySelectionBoxes ();
  applyBounds ();
  rebuildHighlights ();
  applyMarker ();
  /* A reload of the same region keeps the pointer where it was, so the block
   * under it is found again on the fresh scene. */
  rehover ();
}

void
DhRenderWidget::setSlice (const std::optional<std::pair<int, int>> &next)
{
  if (next == sliceRange)
    return;
  sliceRange = next;
  refitPending = true;
  if (ownScene)
    {
      /* This widget has the region, so the fresh scene can be taken here; its
       * construction blocks for as long as the build takes, which is what the
       * blocking constructor's callers already accept. */
      buildOwnScene ();
      return;
    }
  /* The box already follows the slice; the scene itself comes from the builder
   * that holds the region, so it is only asked for. */
  applyBounds ();
  Q_EMIT sliceChanged ();
}

void
DhRenderWidget::setBoundsVisible (bool visible)
{
  if (visible == showBounds)
    return;
  showBounds = visible;
  applyBounds ();
}

void
DhRenderWidget::setRegionExtent (qint32 x, qint32 y, qint32 z)
{
  if (x == extent[0] && y == extent[1] && z == extent[2])
    return;
  extent[0] = x;
  extent[1] = y;
  extent[2] = z;
  applyBounds ();
}

bool
DhRenderWidget::boundsBox (float *box) const
{
  if (extent[0] <= 0 || extent[1] <= 0 || extent[2] <= 0)
    return false;
  int low = 0;
  int high = extent[1];
  if (sliceRange)
    {
      /* A low above the high draws no block at all, and a layer occupies the
       * cell between its own Y and the next one. */
      if (sliceRange->first > sliceRange->second)
        return false;
      low = std::max (0, sliceRange->first);
      high = std::min (extent[1], sliceRange->second + 1);
      if (low >= high)
        return false;
    }
  box[0] = 0.0f;
  box[1] = static_cast<float> (low);
  box[2] = 0.0f;
  box[3] = static_cast<float> (extent[0]);
  box[4] = static_cast<float> (high);
  box[5] = static_cast<float> (extent[2]);
  return true;
}

void
DhRenderWidget::applyBounds ()
{
  if (!scene)
    return;
  float box[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
  const bool shown = showBounds && boundsBox (box);
  dhlrc_render_scene_set_bounds (scene.get (), shown ? box : nullptr);
  /* Both boxes share the overlay's own small buffer, so the GL child is told
   * the box changed; its scene buffers are not touched. */
  if (gpu && useGpu)
    gpu->highlightChanged ();
  update ();
}

void
DhRenderWidget::applySelectionBoxes ()
{
  selectionBoxes.clear ();
  if (!scene)
    return;
  selectionBoxes.reserve (selection.size () * 6);
  for (const BlockCell &cell : selection)
    {
      /* The whole cell first: a cell the scene no longer draws leaves the box
       * alone, and the selection still gets an outline rather than nothing. */
      float box[6] = { static_cast<float> (cell[0]),
                       static_cast<float> (cell[1]),
                       static_cast<float> (cell[2]),
                       static_cast<float> (cell[0] + 1),
                       static_cast<float> (cell[1] + 1),
                       static_cast<float> (cell[2] + 1) };
      dhlrc_render_scene_block_box (scene.get (), cell[0], cell[1], cell[2],
                                    box);
      selectionBoxes.insert (selectionBoxes.end (), box, box + 6);
    }
}

void
DhRenderWidget::rebuildHighlights ()
{
  if (!scene)
    return;
  /* The selection's boxes were converted when it changed; only the hover box
   * is worked out here. The hover goes to its own call so the pointer's box and
   * a picked box over the same cell keep their own colours. */
  dhlrc_render_scene_set_highlights (
      scene.get (), selectionBoxes.empty () ? nullptr : selectionBoxes.data (),
      selectionBoxes.size () / 6);
  /* A picked block is already outlined by its selection box; keeping the
   * pointer's box as well would either hide it (both boxes have the same
   * geometry, so the one drawn last loses the depth test) or paint over it, and
   * the click would look like it did nothing. The selection's own colour takes
   * over as soon as the block is picked. */
  const BlockCell hoverCell{ hoverBlock[0], hoverBlock[1], hoverBlock[2] };
  const bool hoverPicked
      = hovered
        && std::find (selection.begin (), selection.end (), hoverCell)
               != selection.end ();
  if (hovered && !hoverPicked)
    {
      float box[6] = { static_cast<float> (hoverBlock[0]),
                       static_cast<float> (hoverBlock[1]),
                       static_cast<float> (hoverBlock[2]),
                       static_cast<float> (hoverBlock[0] + 1),
                       static_cast<float> (hoverBlock[1] + 1),
                       static_cast<float> (hoverBlock[2] + 1) };
      /* The box the block is actually drawn in, so a slab gets half a cell and
       * a pane a sliver of one; a block that drew nothing falls back to the
       * whole cell. */
      dhlrc_render_scene_block_box (scene.get (), hoverBlock[0], hoverBlock[1],
                                    hoverBlock[2], box);
      dhlrc_render_scene_set_hover (scene.get (), box);
    }
  else
    dhlrc_render_scene_set_hover (scene.get (), nullptr);
  /* The outlines share the overlay's own small buffer, so only that buffer is
   * read again, never the scene's. */
  if (gpu && useGpu)
    gpu->highlightChanged ();
  update ();
}

void
DhRenderWidget::applyColours ()
{
  if (scene)
    {
      float rgb[3] = { 0.0f, 0.0f, 0.0f };
      const QColor picked = highlightColourFor (backgroundColour, highlightColour);
      colourFloats (picked, rgb);
      dhlrc_render_scene_set_highlight_colour (scene.get (), rgb);
      colourFloats (hoverColourFor (backgroundColour, picked), rgb);
      dhlrc_render_scene_set_hover_colour (scene.get (), rgb);
      colourFloats (boundsColour, rgb);
      dhlrc_render_scene_set_bounds_colour (scene.get (), rgb);
      colourFloats (markerColour, rgb);
      dhlrc_render_scene_set_marker_colour (scene.get (), rgb);
      /* Every outline is the overlay the GL child keeps its own buffer for, so
       * it is told to read it again. */
      if (gpu && useGpu)
        gpu->highlightChanged ();
    }
  if (gpu)
    gpu->setBackgroundColour (backgroundColour);
}

void
DhRenderWidget::setHighlightColour (const QColor &colour)
{
  if (colour == highlightColour)
    return;
  highlightColour = colour;
  applyColours ();
  update ();
}

void
DhRenderWidget::setBoundsColour (const QColor &colour)
{
  if (colour == boundsColour)
    return;
  boundsColour = colour;
  applyColours ();
  update ();
}

void
DhRenderWidget::setBackgroundColour (const QColor &colour)
{
  if (colour == backgroundColour)
    return;
  backgroundColour = colour;
  /* The hover colour follows the background, so the outline colours are
   * applied again; that also hands the GL child its new clear colour. */
  applyColours ();
  update ();
}

void
DhRenderWidget::setSelectMode (bool on)
{
  if (on == selectModeOn)
    return;
  selectModeOn = on;
  /* The current selection stays; only what the next click does changes. */
  setCursor (on ? Qt::CrossCursor : Qt::OpenHandCursor);
}

void
DhRenderWidget::setMarkerCell (std::optional<std::array<int, 3>> cell)
{
  if (cell == marker)
    return;
  marker = std::move (cell);
  /* The box belongs to the current scene; with no scene it waits, and the
   * scene-adoption path puts it on the next one. */
  applyMarker ();
}

void
DhRenderWidget::setMarkerColour (const QColor &colour)
{
  if (colour == markerColour)
    return;
  markerColour = colour;
  applyColours ();
  update ();
}

void
DhRenderWidget::applyMarker ()
{
  if (!scene)
    return;
  if (!marker)
    {
      /* A null box clears the marker. */
      dhlrc_render_scene_set_marker (scene.get (), nullptr);
    }
  else
    {
      /* The whole cell first: a cell that drew nothing still gets its
       * outline, exactly as a selected cell does. */
      float box[6] = { static_cast<float> ((*marker)[0]),
                       static_cast<float> ((*marker)[1]),
                       static_cast<float> ((*marker)[2]),
                       static_cast<float> ((*marker)[0] + 1),
                       static_cast<float> ((*marker)[1] + 1),
                       static_cast<float> ((*marker)[2] + 1) };
      dhlrc_render_scene_block_box (scene.get (), (*marker)[0], (*marker)[1],
                                    (*marker)[2], box);
      dhlrc_render_scene_set_marker (scene.get (), box);
    }
  /* The marker is part of the same overlay buffer as the other boxes. */
  if (gpu && useGpu)
    gpu->highlightChanged ();
  update ();
}

void
DhRenderWidget::setSelectedCells (const QVector<BlockCell> &cells)
{
  if (cells == selection)
    return;
  selection = cells;
  /* The boxes belong to the current scene, so they are converted here and
   * again whenever a scene is adopted; with no scene the cells wait. */
  applySelectionBoxes ();
  rebuildHighlights ();
  /* A programmatic change is the same change to the buttons that
   * work on the selection as a click is. */
  Q_EMIT selectionChanged ();
}

void
DhRenderWidget::setBlockEnumerator (BlockEnumerator next)
{
  enumerator = std::move (next);
}

void
DhRenderWidget::applyCamera ()
{
  if (gpu)
    gpu->setCamera (camera);
  update ();
  /* A camera move changes which block a picture pixel names, so the outline
   * is found again under the last pointer position. A drag clears the hover
   * before it starts, so this does nothing while the mouse is down. */
  rehover ();
}

void
DhRenderWidget::onFrameDrawn (bool ok)
{
  if (ok || !useGpu)
    return;
  /* A shader that would not build, or a context that broke, leaves the child
   * drawing nothing; the rasteriser takes over rather than showing a blank
   * panel. */
  useGpu = false;
  if (gpu)
    gpu->hide ();
  update ();
  Q_EMIT backendChanged ();
}

quint64
DhRenderWidget::quadCount () const
{
  return scene ? static_cast<quint64> (dhlrc_render_scene_quads (scene.get ())) : 0;
}

quint64
DhRenderWidget::skippedCount () const
{
  return scene ? static_cast<quint64> (dhlrc_render_scene_skipped (scene.get ())) : 0;
}

bool
DhRenderWidget::orthographic () const
{
  return camera.orthographic != 0;
}

void
DhRenderWidget::setOrthographic (bool on)
{
  camera.orthographic = on ? 1 : 0;
  applyCamera ();
}

void
DhRenderWidget::resetView ()
{
  camera.yaw = 45.0;
  camera.pitch = 30.0;
  if (scene)
    {
      dhlrc_render_fit (scene.get (), &camera, width (), height ());
      framed = true;
    }
  applyCamera ();
}

void
DhRenderWidget::resizeEvent (QResizeEvent *event)
{
  QWidget::resizeEvent (event);
  /* The first real size is what the region is framed against; until it is
   * known the widget is still the default 100 by 30. */
  if (scene && !framed && width () > 1 && height () > 1)
    {
      dhlrc_render_fit (scene.get (), &camera, width (), height ());
      framed = true;
    }
  /* What sits under the pointer is not looked up per step: the picture is
   * about to change size again, and a pick plus an outline rebuild (and a
   * tooltip moved to a different corner) on every pixel of a drag is what
   * makes resizing the view feel like it is shaking. Once the size has
   * settled the block under the pointer is found again. */
  update ();
  if (hoverSettle)
    hoverSettle->start ();
  /* The GL child is a window of its own, and resizing it on every pixel of a
   * drag makes the window system lay out a new area for each step — more
   * often than the renderer can fill it. Handing it the new size at a steady
   * rate leaves the strip that is not covered yet to the parent, which paints
   * the renderer's background there; the running timer applies the last size
   * as soon as the drag pauses. */
  if (!resizeThrottle)
    return;
  if (!resizeThrottle->isActive ())
    {
      applyViewSize ();
      resizeThrottle->start ();
    }
}

void
DhRenderWidget::applyViewSize ()
{
  if (!gpu)
    {
      update ();
      return;
    }
  gpu->setGeometry (rect ());
  gpu->setCamera (camera);
}

void
DhRenderWidget::paintEvent (QPaintEvent *)
{
  /* The panel behind the picture is painted even while the GL child draws it:
   * the child is a sibling window, so any moment it is absent — a fallback, a
   * rebuilt context, a resize that has not reached it yet — the area it covers
   * must show the renderer's own background rather than whatever the backing
   * store happened to hold. It costs one fill of an area that is hidden again
   * as soon as the child has a picture. */
  QPainter painter (this);
  painter.fillRect (rect (), backgroundColour);

  /* While the GL child is drawing it covers the whole widget, so the CPU
   * picture would only be painted over. This is also the fallback path: with
   * no scene, or after a GL frame failed, the rasteriser draws here. */
  if (useGpu && scene != nullptr)
    return;
  if (!scene)
    {
      painter.setPen (Qt::white);
      painter.drawText (rect ().adjusted (12, 12, -12, -12),
                        Qt::AlignCenter | Qt::TextWordWrap, problemText);
      return;
    }

  /* While the mouse is down the picture is drawn at half size and stretched,
   * which is what keeps a turn smooth on a large region. The picture is drawn
   * at the device's own resolution, so it stays sharp on a scaled display. */
  const int step = dragging ? 2 : 1;
  const qreal ratio = devicePixelRatioF ();
  const int pictureWidth
      = std::max (1, static_cast<int> (width () * ratio) / step);
  const int pictureHeight
      = std::max (1, static_cast<int> (height () * ratio) / step);
  const uint8_t *pixels
      = dhlrc_render_image (scene.get (), &camera, pictureWidth, pictureHeight);
  if (!pixels)
    return;

  /* Wraps the renderer's buffer; it stays valid until the next draw, which is
   * after this one has been painted. */
  const QImage frame (pixels, pictureWidth, pictureHeight, pictureWidth * 4,
                      QImage::Format_RGBA8888);
  painter.setRenderHint (QPainter::SmoothPixmapTransform, step > 1);
  painter.drawImage (rect (), frame);
}

void
DhRenderWidget::mousePressEvent (QMouseEvent *event)
{
  /* The outline and the tooltip would only get in the way of a drag, and the
   * answer under the pointer is stale the moment the camera moves. */
  clearHover ();
  lastPos = event->position ().toPoint ();
  dragButton = event->button ();
  dragging = true;
  moved = false;
  setCursor (Qt::ClosedHandCursor);
}

void
DhRenderWidget::mouseMoveEvent (QMouseEvent *event)
{
  const QPoint now = event->position ().toPoint ();
  if (!dragging)
    {
      hoverAt (now);
      return;
    }
  const QPoint delta = now - lastPos;
  lastPos = now;
  if (std::abs (delta.x ()) + std::abs (delta.y ()) > 2)
    moved = true;

  if (dragButton == Qt::LeftButton)
    {
      camera.yaw
          = std::fmod (camera.yaw + delta.x () * TURN_PER_PIXEL + 360.0, 360.0);
      camera.pitch = std::clamp (camera.pitch - delta.y () * TURN_PER_PIXEL,
                                 -89.9, 89.9);
    }
  else
    {
      /* A drag of the whole height slides by one picture's worth of scene. */
      dhlrc_render_pan (&camera, -delta.x () * 2.0 / std::max (1, height ()),
                        delta.y () * 2.0 / std::max (1, height ()));
    }
  applyCamera ();
}

void
DhRenderWidget::mouseReleaseEvent (QMouseEvent *event)
{
  const bool wasDragging = dragging;
  const bool wasMoved = moved;
  dragging = false;
  dragButton = Qt::NoButton;
  setCursor (selectModeOn ? Qt::CrossCursor : Qt::OpenHandCursor);
  /* Only a click without a drag acts: in select mode it toggles the cell, and
   * outside it only the cell is reported. */
  if (wasDragging && !wasMoved && event->button () == Qt::LeftButton)
    {
      if (selectModeOn)
        toggleCellAt (event->position ().toPoint ());
      else
        pickAt (event->position ().toPoint ());
    }
  /* Where the button came up is where the pointer is, so the outline is put
   * back under it. */
  hoverAt (event->position ().toPoint ());
  update ();
}

void
DhRenderWidget::leaveEvent (QEvent *event)
{
  /* Over the window's other widgets, or out of the window: nothing is under
   * the pointer any more. */
  clearHover ();
  pointerKnown = false;
  QWidget::leaveEvent (event);
}

void
DhRenderWidget::wheelEvent (QWheelEvent *event)
{
  const double notches = event->angleDelta ().y () / 120.0;
  camera.distance = std::clamp (
      camera.distance * std::pow (ZOOM_PER_NOTCH, notches), 0.5, 4096.0);
  applyCamera ();
}

void
DhRenderWidget::keyPressEvent (QKeyEvent *event)
{
  switch (event->key ())
    {
    case Qt::Key_R:
      resetView ();
      return;
    case Qt::Key_O:
      setOrthographic (!orthographic ());
      return;
    case Qt::Key_Left:
      camera.yaw = std::fmod (camera.yaw - 5.0 + 360.0, 360.0);
      break;
    case Qt::Key_Right:
      camera.yaw = std::fmod (camera.yaw + 5.0, 360.0);
      break;
    case Qt::Key_Up:
      camera.pitch = std::min (89.9, camera.pitch + 5.0);
      break;
    case Qt::Key_Down:
      camera.pitch = std::max (-89.9, camera.pitch - 5.0);
      break;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
      camera.distance = std::max (0.5, camera.distance * ZOOM_PER_NOTCH);
      break;
    case Qt::Key_Minus:
      camera.distance = std::min (4096.0, camera.distance / ZOOM_PER_NOTCH);
      break;
    default:
      QWidget::keyPressEvent (event);
      return;
    }
  applyCamera ();
}

void
DhRenderWidget::toggleCellAt (const QPoint &at)
{
  int32_t cell[3] = { 0, 0, 0 };
  if (!cellAt (at, cell))
    {
      /* Empty space clears the selection, so a stray click is an easy way to
       * start over. An empty set is already cleared: no signal. */
      if (selection.isEmpty ())
        return;
      selection.clear ();
      applySelectionBoxes ();
      rebuildHighlights ();
      Q_EMIT selectionChanged ();
      return;
    }
  const BlockCell target{ cell[0], cell[1], cell[2] };
  const auto found = std::find (selection.begin (), selection.end (), target);
  if (found == selection.end ())
    selection.push_back (target);
  else
    selection.erase (found);
  applySelectionBoxes ();
  rebuildHighlights ();
  Q_EMIT selectionChanged ();
}

bool
DhRenderWidget::cellAt (const QPoint &at, int32_t *cell) const
{
  if (!scene)
    return false;
  /* The picture the ray is measured against is the full-size one in device
   * pixels, which is the same shape as the widget either way. */
  const qreal ratio = devicePixelRatioF ();
  const int pictureWidth = std::max (1, static_cast<int> (width () * ratio));
  const int pictureHeight = std::max (1, static_cast<int> (height () * ratio));
  return dhlrc_render_pick (scene.get (), &camera, pictureWidth, pictureHeight,
                            at.x () * ratio, at.y () * ratio, cell)
         != 0;
}

void
DhRenderWidget::pickAt (const QPoint &at)
{
  int32_t cell[3] = { 0, 0, 0 };
  if (!cellAt (at, cell))
    {
      /* Empty space still names its cell, so the panel can clear whatever the
       * last click described. */
      Q_EMIT cellPicked (-1, -1, -1);
      return;
    }
  Q_EMIT blockPicked (cell[0], cell[1], cell[2]);
  Q_EMIT cellPicked (cell[0], cell[1], cell[2]);
}

void
DhRenderWidget::setHoverInfo (const QString &text)
{
  if (!tip)
    return;
  if (text.isEmpty () || !hovered)
    {
      tip->hide ();
      return;
    }
  tip->setText (text);
  tip->adjustSize ();
  placeTip ();
  tip->show ();
  /* Over the GL child, which is the picture when one is drawing. */
  tip->raise ();
}

void
DhRenderWidget::hoverAt (const QPoint &at)
{
  hoverPoint = at;
  pointerKnown = true;
  if (!scene)
    return;

  /* The ray is measured against the full-size picture in device pixels, the
   * same picture a click measures against. */
  const qreal ratio = devicePixelRatioF ();
  const int pictureWidth = std::max (1, static_cast<int> (width () * ratio));
  const int pictureHeight = std::max (1, static_cast<int> (height () * ratio));
  int32_t block[3] = { 0, 0, 0 };
  if (dhlrc_render_pick (scene.get (), &camera, pictureWidth, pictureHeight,
                         at.x () * ratio, at.y () * ratio, block)
      == 0)
    {
      clearHover ();
      return;
    }

  if (hovered && block[0] == hoverBlock[0] && block[1] == hoverBlock[1]
      && block[2] == hoverBlock[2])
    {
      /* Still the same block: only the tooltip follows the pointer, and the
       * picture is not drawn again. */
      placeTip ();
      return;
    }

  hovered = true;
  hoverBlock[0] = block[0];
  hoverBlock[1] = block[1];
  hoverBlock[2] = block[2];
  /* The hover box joins the selected ones rather than replacing them; the box
   * the block is actually drawn in is worked out there. */
  rebuildHighlights ();
  Q_EMIT blockHovered (block[0], block[1], block[2]);
}

void
DhRenderWidget::clearHover ()
{
  /* The tooltip is hidden even when no block was hovered, so a stale one from
   * an earlier hover cannot stay up. */
  if (tip && tip->isVisible ())
    tip->hide ();
  if (!hovered)
    return;
  hovered = false;
  /* The selected cells stay outlined. */
  rebuildHighlights ();
}

void
DhRenderWidget::rehover ()
{
  if (pointerKnown)
    hoverAt (hoverPoint);
}

void
DhRenderWidget::placeTip ()
{
  if (!tip || !hovered)
    return;
  const QRect area = rect ().adjusted (4, 4, -6, -6);
  const QSize size = tip->size ();
  /* Above the pointer, where there is room: the tip must not cover the block
   * it names, or the outline that shows the block was picked would be hidden
   * under it exactly when the pick happens. Below it when the top of the view
   * is too close. */
  QPoint at = hoverPoint + QPoint (14, -size.height () - 14);
  if (at.y () < area.top ())
    at.setY (hoverPoint.y () + 18);
  if (at.x () + size.width () > area.right ())
    at.setX (hoverPoint.x () - size.width () - 12);
  at.setX (std::max (area.left (), at.x ()));
  at.setY (std::max (area.top (), at.y ()));
  tip->move (at);
}
