#ifndef DHGPUWIDGET_H
#define DHGPUWIDGET_H

#include "dhlrc_render.h"

#include <QString>

/* The Qt OpenGL module is optional: a Qt built without it cannot make a
 * context at all, and the caller then keeps the CPU rasteriser. The two class
 * shapes below share every public member, so the rest of the program does not
 * have to know which one it got; only the base class and the drawing members
 * differ. */
#ifdef DH_HAVE_OPENGL
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLWidget>
#else
#include <QWidget>
#endif

#include <vector>

#ifdef DH_HAVE_OPENGL
/* Draws a `DhlrcScene` with OpenGL rather than the software rasteriser.
 *
 * The geometry is the core's own: every quad is read through
 * `dhlrc_render_scene_quad ()` and the view-projection comes from
 * `dhlrc_render_matrix ()`, so a GL frame and a CPU frame of the same camera
 * are the same picture. Quads are grouped by texture and by how their alpha
 * behaves, and the groups are buffered once per scene: a frame is then a
 * handful of `glDrawArrays ()` calls rather than one per quad.
 *
 * The scene is borrowed, not copied, so the caller must keep it alive for as
 * long as it is set. A shader that will not compile, or a context that is not
 * a desktop 3.3 one, leaves `drew ()` false — that is the signal the caller
 * falls back on, rather than a widget that shows nothing.
 *
 * Desktop `#version 330 core` shaders are used. A context that only offers
 * GLES is not handled: `available ()` reports false for it, which is the
 * answer that keeps the CPU picture.
 */
class DhGpuWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
#else
/* Without the Qt OpenGL module there is no GPU path at all: `available ()` is
 * always false and the caller never shows this widget. The public shape stays
 * the same so the call sites do not need a second `#ifdef`. */
class DhGpuWidget : public QWidget
#endif
{
  Q_OBJECT
public:
  explicit DhGpuWidget (QWidget *parent = nullptr);
  ~DhGpuWidget () override;

  /* Whether this machine can give a desktop OpenGL 3.3 core context.
   *
   * Probed once, on an offscreen context, so a caller can pick a backend
   * before it creates a widget that would otherwise show nothing. The first
   * call also sets the application's default surface format to the one the
   * widget below needs; it must therefore run before the widget is built, and
   * after `QApplication` exists. */
  static bool available ();

  /* Borrows `scene`; the caller keeps it alive. A change of pointer rebuilds
   * the buffers and textures on the next frame. */
  void setScene (const DhlrcScene *scene);
  void setCamera (const DhlrcCamera &camera);

  /* Says the boxes the scene draws over itself — the region's extent and the
   * block under the pointer — have changed, so the next frame reads them
   * again. Only those boxes' own small buffer is rebuilt; the scene's buffers
   * and textures are left alone, which is what keeps a bounds toggle and a
   * hover cheap. */
  void highlightChanged ();

  /* Whether the last draw really drew, rather than giving up over a shader or
   * a context. The caller falls back to the CPU picture while this is false. */
  [[nodiscard]] bool drew () const;
  /* Why the last draw gave up, for the caller's message. Empty when it did
   * not. */
  [[nodiscard]] const QString &problem () const;

Q_SIGNALS:
  /* Emitted after every draw with whether it succeeded, so a view can drop
   * back to the CPU on its own the moment the first frame fails. */
  void frameDrawn (bool ok);

#ifdef DH_HAVE_OPENGL
protected:
  void initializeGL () override;
  void resizeGL (int width, int height) override;
  void paintGL () override;

private:
  /* One run of quads that share a texture, an alpha rule and a pass. */
  struct Batch
  {
    int first;
    int count;
    /* The scene's own texture index, looked up in `textures` when drawn: it is
     * not a GL name. */
    unsigned int textureIndex;
    bool cutout;
  };

  /* Drops every GL object belonging to the current scene. The context must be
   * current. */
  void releaseScene ();
  /* Compiles and links the program, and creates the vertex array and buffer.
   * False leaves `problemText` set. */
  bool buildProgram ();
  /* Reads the scene through the ABI, uploads its textures and fills the
   * batches. False leaves `problemText` set. */
  bool rebuildScene ();
  /* Draws one pass, binding each batch's texture and setting its alpha rule. */
  void drawBatches (const std::vector<Batch> &batches);
  /* Points the bound vertex array's attributes at the bound vertex buffer.
   * The layout is the same for the scene and for the overlay boxes. */
  void bindQuadLayout ();
  /* Reads the overlay boxes' quads through the ABI into their own buffer. */
  void rebuildHighlight ();

  const DhlrcScene *scene = nullptr;
  DhlrcCamera camera{};
  bool cameraSet = false;
  bool sceneDirty = false;
  bool highlightDirty = true;
  bool glReady = false;
  bool drewFrame = false;
  QString problemText;

  unsigned int program = 0;
  int mvpLocation = -1;
  int textureLocation = -1;
  int cutoutLocation = -1;
  unsigned int vao = 0;
  unsigned int vbo = 0;
  /* The overlay boxes' own small buffer, so a bounds toggle or a hover never
   * rebuilds the scene. */
  unsigned int highlightVao = 0;
  unsigned int highlightVbo = 0;
  int highlightVertices = 0;
  std::vector<unsigned int> textures;
  /* Opaque and cutout quads, then the blended ones. */
  std::vector<Batch> solidBatches;
  std::vector<Batch> blendedBatches;
#endif
};

#endif /* DHGPUWIDGET_H */
