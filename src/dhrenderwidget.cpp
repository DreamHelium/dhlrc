#include "dhrenderwidget.h"

#include "dhgpuwidget.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QWheelEvent>
#include <libintl.h>
#include <algorithm>
#include <cmath>
#include <qnamespace.h>

#define _(str) gettext (str)

namespace
{
/* The panel behind the picture, so a transparent render is still readable. */
const QColor BACKGROUND (0x20, 0x28, 0x30);
/* How much of a degree one pixel of a turn is worth. */
const double TURN_PER_PIXEL = 0.4;
/* How much of the distance one wheel notch takes away. */
const double ZOOM_PER_NOTCH = 0.88;
} // namespace

DhRenderWidget::DhRenderWidget (void *core, void *region, QWidget *parent)
    : QWidget (parent)
{
  camera.yaw = 45.0;
  camera.pitch = 30.0;
  camera.distance = 16.0;
  camera.fov = 45.0;
  setFocusPolicy (Qt::StrongFocus);
  setMinimumSize (240, 180);
  setCursor (Qt::OpenHandCursor);

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
    }

  rebuild (core, region);
}

DhRenderWidget::~DhRenderWidget () { discard (); }

void
DhRenderWidget::discard ()
{
  /* The GL child borrows the scene, so it lets go before the scene goes. */
  if (gpu)
    gpu->setScene (nullptr);
  if (scene)
    {
      dhlrc_render_scene_free (scene);
      scene = nullptr;
    }
}

void
DhRenderWidget::rebuild (void *core, void *region)
{
  discard ();
  scene = dhlrc_render_scene_new (core, region);
  problemText = scene
                    ? QString ()
                    : QString (_ ("The region could not be drawn: no block in "
                                  "it could be resolved. Check that the "
                                  "resource files for this version have been "
                                  "downloaded in the settings."));
  framed = false;
  if (gpu)
    {
      gpu->setScene (scene);
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
  update ();
}

void
DhRenderWidget::applyCamera ()
{
  if (gpu)
    gpu->setCamera (camera);
  update ();
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
  return scene ? static_cast<quint64> (dhlrc_render_scene_quads (scene)) : 0;
}

quint64
DhRenderWidget::skippedCount () const
{
  return scene ? static_cast<quint64> (dhlrc_render_scene_skipped (scene)) : 0;
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
      dhlrc_render_fit (scene, &camera, width (), height ());
      framed = true;
    }
  applyCamera ();
}

void
DhRenderWidget::resizeEvent (QResizeEvent *event)
{
  QWidget::resizeEvent (event);
  if (gpu)
    gpu->setGeometry (rect ());
  /* The first real size is what the region is framed against; until it is
   * known the widget is still the default 100 by 30. */
  if (scene && !framed && width () > 1 && height () > 1)
    {
      dhlrc_render_fit (scene, &camera, width (), height ());
      framed = true;
    }
  applyCamera ();
}

void
DhRenderWidget::paintEvent (QPaintEvent *)
{
  /* While the GL child is drawing it covers the whole widget, so the CPU
   * picture would only be painted over. This is also the fallback path: with
   * no scene, or after a GL frame failed, the rasteriser draws here. */
  if (useGpu && scene != nullptr)
    return;

  QPainter painter (this);
  painter.fillRect (rect (), BACKGROUND);
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
      = dhlrc_render_image (scene, &camera, pictureWidth, pictureHeight);
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
  lastPos = event->position ().toPoint ();
  dragButton = event->button ();
  dragging = true;
  moved = false;
  setCursor (Qt::ClosedHandCursor);
}

void
DhRenderWidget::mouseMoveEvent (QMouseEvent *event)
{
  if (!dragging)
    return;
  const QPoint now = event->position ().toPoint ();
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
  dragging = false;
  dragButton = Qt::NoButton;
  setCursor (Qt::OpenHandCursor);
  if (wasDragging && !moved && event->button () == Qt::LeftButton)
    pickAt (event->position ().toPoint ());
  update ();
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
DhRenderWidget::pickAt (const QPoint &at)
{
  if (!scene)
    return;
  /* The picture the ray is measured against is the full-size one in device
   * pixels, which is the same shape as the widget either way. */
  const qreal ratio = devicePixelRatioF ();
  const int pictureWidth = std::max (1, static_cast<int> (width () * ratio));
  const int pictureHeight = std::max (1, static_cast<int> (height () * ratio));
  int32_t block[3] = { 0, 0, 0 };
  if (dhlrc_render_pick (scene, &camera, pictureWidth, pictureHeight,
                         at.x () * ratio, at.y () * ratio, block))
    Q_EMIT blockPicked (block[0], block[1], block[2]);
}
