#include "dhgpuwidget.h"

#include <algorithm>
#include <cstdint>

#ifdef DH_HAVE_OPENGL
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLVersionFunctionsFactory>
#include <QSurfaceFormat>
#include <QtGlobal>

/* Desktop GLSL: the widget asks for a 3.3 core context, so this is what a
 * context that reaches us can compile. A GLES-only context is rejected by
 * `available ()` instead of being given a shader it cannot build. */
namespace
{
const char *const VERTEX_SOURCE = R"(
#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec3 aTint;
layout(location = 3) in float aLight;
uniform mat4 uMvp;
out vec2 vUv;
out vec3 vTint;
out float vLight;
void main ()
{
  gl_Position = uMvp * vec4 (aPosition, 1.0);
  vUv = aUv;
  vTint = aTint;
  vLight = aLight;
}
)";

const char *const FRAGMENT_SOURCE = R"(
#version 330 core
uniform sampler2D uTexture;
uniform int uCutout;
in vec2 vUv;
in vec3 vTint;
in float vLight;
out vec4 fragColor;
void main ()
{
  vec4 texel = texture (uTexture, vUv);
  /* The software rasteriser skips a clear pixel whatever the face is, and a
     cutout face drops everything below half alpha. */
  if (texel.a <= 0.0)
    discard;
  if (uCutout != 0 && texel.a < 0.5)
    discard;
  fragColor = vec4 (texel.rgb * vTint * vLight, texel.a);
}
)";

/* The colour the CPU view paints behind its picture. */
const float BACKGROUND[3] = { 0x20 / 255.0f, 0x28 / 255.0f, 0x30 / 255.0f };
} // namespace

DhGpuWidget::DhGpuWidget (QWidget *parent) : QOpenGLWidget (parent)
{
  setUpdateBehavior (QOpenGLWidget::NoPartialUpdate);
}

DhGpuWidget::~DhGpuWidget ()
{
  /* The context still exists while this object does; take it one last time so
   * the scene's textures and buffers do not outlive it. */
  if (context ())
    {
      makeCurrent ();
      releaseScene ();
      if (vao != 0)
        glDeleteVertexArrays (1, &vao);
      if (vbo != 0)
        glDeleteBuffers (1, &vbo);
      if (highlightVao != 0)
        glDeleteVertexArrays (1, &highlightVao);
      if (highlightVbo != 0)
        glDeleteBuffers (1, &highlightVbo);
      if (program != 0)
        glDeleteProgram (program);
      if (textures.empty () && solidBatches.empty ())
        {
          vao = 0;
          vbo = 0;
          highlightVao = 0;
          highlightVbo = 0;
          program = 0;
        }
      doneCurrent ();
    }
}

bool
DhGpuWidget::available ()
{
  /* Probed once: creating a context either works on this machine or it does
   * not, and the answer must not change between widgets. The probe also
   * settles the default surface format, so every widget built afterwards asks
   * for the same context the probe proved. */
  static const bool usable = []
  {
    QSurfaceFormat format;
    format.setRenderableType (QSurfaceFormat::OpenGL);
    format.setVersion (3, 3);
    format.setProfile (QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize (24);
    QSurfaceFormat::setDefaultFormat (format);

    QOffscreenSurface surface;
    surface.setFormat (format);
    surface.create ();
    if (!surface.isValid ())
      return false;

    QOpenGLContext context;
    context.setFormat (format);
    if (!context.create ())
      return false;
    /* A GLES context would need the other shader dialect, which this widget
     * does not carry; the caller keeps the CPU picture instead. */
    if (context.isOpenGLES () || context.format ().version () < qMakePair (3, 3))
      return false;
    if (!context.makeCurrent (&surface))
      return false;

    auto *functions
        = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core> (
            &context);
    const bool ok
        = functions != nullptr && functions->initializeOpenGLFunctions ();
    context.doneCurrent ();
    return ok;
  }();
  return usable;
}

void
DhGpuWidget::setScene (const DhlrcScene *value)
{
  if (value == scene)
    return;
  scene = value;
  /* The old scene's GL objects are dropped on the next frame, where the
   * context is current; only the reference changes here. A new scene carries
   * no overlay boxes, so their buffer is read again too. */
  sceneDirty = true;
  highlightDirty = true;
  update ();
}

void
DhGpuWidget::setCamera (const DhlrcCamera &value)
{
  camera = value;
  cameraSet = true;
  update ();
}

void
DhGpuWidget::highlightChanged ()
{
  highlightDirty = true;
  update ();
}

bool
DhGpuWidget::drew () const
{
  return drewFrame;
}

const QString &
DhGpuWidget::problem () const
{
  return problemText;
}

void
DhGpuWidget::initializeGL ()
{
  glReady = false;
  if (!initializeOpenGLFunctions ())
    {
      problemText = QStringLiteral (
          "the context does not offer the OpenGL 3.3 core functions");
      return;
    }
  if (!buildProgram ())
    return;

  glGenVertexArrays (1, &vao);
  glBindVertexArray (vao);
  glGenBuffers (1, &vbo);
  glBindBuffer (GL_ARRAY_BUFFER, vbo);
  bindQuadLayout ();

  /* The overlay boxes get their own vertex array and buffer: a bounds toggle
   * or a hover then rewrites a few kilobytes instead of the whole scene. */
  glGenVertexArrays (1, &highlightVao);
  glBindVertexArray (highlightVao);
  glGenBuffers (1, &highlightVbo);
  glBindBuffer (GL_ARRAY_BUFFER, highlightVbo);
  bindQuadLayout ();
  glBindVertexArray (0);
  glBindBuffer (GL_ARRAY_BUFFER, 0);

  glUseProgram (program);
  glUniform1i (textureLocation, 0);
  glUseProgram (0);
  glReady = true;
}

void
DhGpuWidget::bindQuadLayout ()
{
  /* position (3), uv (2), tint (3), light (1) */
  const GLsizei stride = 9 * sizeof (float);
  glEnableVertexAttribArray (0);
  glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
  glEnableVertexAttribArray (1);
  glVertexAttribPointer (1, 2, GL_FLOAT, GL_FALSE, stride,
                         reinterpret_cast<const void *> (3 * sizeof (float)));
  glEnableVertexAttribArray (2);
  glVertexAttribPointer (2, 3, GL_FLOAT, GL_FALSE, stride,
                         reinterpret_cast<const void *> (5 * sizeof (float)));
  glEnableVertexAttribArray (3);
  glVertexAttribPointer (3, 1, GL_FLOAT, GL_FALSE, stride,
                         reinterpret_cast<const void *> (8 * sizeof (float)));
}

void
DhGpuWidget::resizeGL (int, int)
{
  /* The viewport is set in `paintGL ()` from the framebuffer's own size, so
   * it is right after a device-pixel-ratio change too. */
}

void
DhGpuWidget::paintGL ()
{
  drewFrame = false;
  const qreal ratio = devicePixelRatioF ();
  const int pixelWidth = std::max (1, qRound (width () * ratio));
  const int pixelHeight = std::max (1, qRound (height () * ratio));
  glViewport (0, 0, pixelWidth, pixelHeight);
  glClearColor (BACKGROUND[0], BACKGROUND[1], BACKGROUND[2], 1.0f);
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (!glReady || scene == nullptr)
    {
      Q_EMIT frameDrawn (false);
      return;
    }
  if (sceneDirty && !rebuildScene ())
    {
      Q_EMIT frameDrawn (false);
      return;
    }
  /* Read the overlay boxes only when they changed, which is what a bounds
   * toggle or a hover does. */
  if (highlightDirty)
    rebuildHighlight ();

  glEnable (GL_DEPTH_TEST);
  glDepthFunc (GL_LESS);
  /* The scene is already culled in block space and every quad is wound
   * outwards, so the depth buffer drops what the rasteriser's own back-face
   * test would. */
  glDisable (GL_CULL_FACE);
  glEnable (GL_BLEND);
  /* Straight-alpha "over", with the alpha channel combined the same way the
   * rasteriser combines it, so the two pictures agree. */
  glBlendFuncSeparate (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                       GL_ONE_MINUS_SRC_ALPHA);

  const bool hasScene = !solidBatches.empty () || !blendedBatches.empty ();
  if (hasScene || highlightVertices > 0)
    {
      float matrix[16] = { 0 };
      dhlrc_render_matrix (scene, &camera,
                           static_cast<uint32_t> (pixelWidth),
                           static_cast<uint32_t> (pixelHeight), matrix);
      glUseProgram (program);
      glUniformMatrix4fv (mvpLocation, 1, GL_FALSE, matrix);
      glActiveTexture (GL_TEXTURE0);
      if (hasScene)
        {
          glBindVertexArray (vao);
          drawBatches (solidBatches);
          drawBatches (blendedBatches);
        }
      if (highlightVertices > 0 && !textures.empty ())
        {
          /* The overlay's quads are ordinary quads: the shared shader draws
           * them with the tint the core gave each box. They go last, so where
           * they lie on the face they trace they win the depth test, exactly
           * as the rasteriser draws them. */
          glBindVertexArray (highlightVao);
          glBindTexture (GL_TEXTURE_2D, textures[0]);
          glUniform1i (cutoutLocation, 0);
          glDrawArrays (GL_TRIANGLES, 0, highlightVertices);
        }
      glBindVertexArray (0);
      glBindTexture (GL_TEXTURE_2D, 0);
      glUseProgram (0);
    }

  drewFrame = true;
  Q_EMIT frameDrawn (true);
}

bool
DhGpuWidget::buildProgram ()
{
  auto compile = [this] (GLenum type, const char *source) -> GLuint
  {
    const GLuint shader = glCreateShader (type);
    glShaderSource (shader, 1, &source, nullptr);
    glCompileShader (shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv (shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE)
      {
        GLint length = 0;
        glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &length);
        QByteArray log (static_cast<int> (length), '\0');
        glGetShaderInfoLog (shader, length, nullptr, log.data ());
        problemText
            = QStringLiteral ("the OpenGL shader did not compile: %1")
                  .arg (QString::fromUtf8 (log).trimmed ());
        glDeleteShader (shader);
        return 0;
      }
    return shader;
  };

  const GLuint vertex = compile (GL_VERTEX_SHADER, VERTEX_SOURCE);
  if (vertex == 0)
    return false;
  const GLuint fragment = compile (GL_FRAGMENT_SHADER, FRAGMENT_SOURCE);
  if (fragment == 0)
    {
      glDeleteShader (vertex);
      return false;
    }

  program = glCreateProgram ();
  glAttachShader (program, vertex);
  glAttachShader (program, fragment);
  glLinkProgram (program);
  glDetachShader (program, vertex);
  glDetachShader (program, fragment);
  glDeleteShader (vertex);
  glDeleteShader (fragment);

  GLint linked = GL_FALSE;
  glGetProgramiv (program, GL_LINK_STATUS, &linked);
  if (linked == GL_FALSE)
    {
      GLint length = 0;
      glGetProgramiv (program, GL_INFO_LOG_LENGTH, &length);
      QByteArray log (static_cast<int> (length), '\0');
      glGetProgramInfoLog (program, length, nullptr, log.data ());
      problemText = QStringLiteral ("the OpenGL program did not link: %1")
                        .arg (QString::fromUtf8 (log).trimmed ());
      glDeleteProgram (program);
      program = 0;
      return false;
    }

  mvpLocation = glGetUniformLocation (program, "uMvp");
  textureLocation = glGetUniformLocation (program, "uTexture");
  cutoutLocation = glGetUniformLocation (program, "uCutout");
  return true;
}

bool
DhGpuWidget::rebuildScene ()
{
  releaseScene ();
  sceneDirty = false;
  if (scene == nullptr)
    return true;

  glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
  const size_t textureCount = dhlrc_render_scene_texture_count (scene);
  textures.assign (textureCount, 0);
  for (size_t index = 0; index < textureCount; ++index)
    {
      const uint8_t *pixels = nullptr;
      uint32_t width = 0;
      uint32_t height = 0;
      if (dhlrc_render_scene_texture (scene, index, &pixels, &width, &height)
              == 0
          || pixels == nullptr || width == 0 || height == 0)
        continue;
      GLuint texture = 0;
      glGenTextures (1, &texture);
      glBindTexture (GL_TEXTURE_2D, texture);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      /* Bilinear, like the rasteriser; no mipmaps are built, so the
       * minification filter has to be a non-mipmapped one. */
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei> (width),
                    static_cast<GLsizei> (height), 0, GL_RGBA,
                    GL_UNSIGNED_BYTE, pixels);
      glBindTexture (GL_TEXTURE_2D, 0);
      textures[index] = texture;
    }

  /* The two triangles of a quad are `0, 1, 2` and `0, 2, 3`. Runs of
   * consecutive quads that share a texture and an alpha rule become one batch,
   * so a frame is a handful of draw calls. The quads are walked twice — the
   * solid ones first, then the blended ones — because a batch has to be a
   * contiguous run of vertices and a translucent quad may sit between two
   * solid ones. */
  std::vector<float> vertices;
  const size_t quadCount = dhlrc_render_scene_quad_count (scene);
  vertices.reserve (quadCount * 6 * 9);
  const int corners[6] = { 0, 1, 2, 0, 2, 3 };
  for (int pass = 0; pass < 2; ++pass)
    {
      for (size_t index = 0; index < quadCount; ++index)
        {
          DhlrcQuad quad{};
          if (dhlrc_render_scene_quad (scene, index, &quad) == 0)
            continue;
          /* 2 is `alpha == translucent`; everything else covers. */
          if ((quad.alpha == 2) != (pass == 1))
            continue;
          std::vector<Batch> &batches
              = pass == 1 ? blendedBatches : solidBatches;
          const bool cutout = quad.alpha == 1;
          if (batches.empty ()
              || batches.back ().textureIndex != quad.texture
              || batches.back ().cutout != cutout)
            batches.push_back (Batch{ static_cast<int> (vertices.size () / 9),
                                      0, quad.texture, cutout });
          batches.back ().count += 6;
          for (int corner : corners)
            {
              const float *position = quad.positions[corner];
              vertices.push_back (position[0]);
              vertices.push_back (position[1]);
              vertices.push_back (position[2]);
              vertices.push_back (quad.uv[corner][0]);
              vertices.push_back (quad.uv[corner][1]);
              vertices.push_back (quad.tint[0]);
              vertices.push_back (quad.tint[1]);
              vertices.push_back (quad.tint[2]);
              vertices.push_back (quad.light);
            }
        }
    }

  glBindBuffer (GL_ARRAY_BUFFER, vbo);
  glBufferData (GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr> (vertices.size () * sizeof (float)),
                vertices.empty () ? nullptr : vertices.data (), GL_STATIC_DRAW);
  glBindBuffer (GL_ARRAY_BUFFER, 0);
  return true;
}

void
DhGpuWidget::rebuildHighlight ()
{
  highlightDirty = false;
  highlightVertices = 0;
  if (scene == nullptr || highlightVbo == 0)
    return;

  /* The overlay boxes' quads come out of the scene in the same shape as its
   * own, so they are unpacked exactly as `rebuildScene ()` unpacks the
   * scene. */
  const size_t count = dhlrc_render_scene_overlay_count (scene);
  std::vector<float> vertices;
  vertices.reserve (count * 6 * 9);
  const int corners[6] = { 0, 1, 2, 0, 2, 3 };
  for (size_t index = 0; index < count; ++index)
    {
      DhlrcQuad quad{};
      if (dhlrc_render_scene_overlay_quad (scene, index, &quad) == 0)
        continue;
      for (int corner : corners)
        {
          const float *position = quad.positions[corner];
          vertices.push_back (position[0]);
          vertices.push_back (position[1]);
          vertices.push_back (position[2]);
          vertices.push_back (quad.uv[corner][0]);
          vertices.push_back (quad.uv[corner][1]);
          vertices.push_back (quad.tint[0]);
          vertices.push_back (quad.tint[1]);
          vertices.push_back (quad.tint[2]);
          vertices.push_back (quad.light);
        }
    }
  highlightVertices = static_cast<int> (vertices.size () / 9);

  glBindBuffer (GL_ARRAY_BUFFER, highlightVbo);
  glBufferData (GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr> (vertices.size () * sizeof (float)),
                vertices.empty () ? nullptr : vertices.data (), GL_DYNAMIC_DRAW);
  glBindBuffer (GL_ARRAY_BUFFER, 0);
}

void
DhGpuWidget::drawBatches (const std::vector<Batch> &batches)
{
  for (const Batch &batch : batches)
    {
      /* The batch holds the scene's texture index, not a GL name: the two are
       * only the same by accident. */
      const unsigned int name = batch.textureIndex < textures.size ()
                                    ? textures[batch.textureIndex]
                                    : 0;
      if (name == 0)
        continue;
      glBindTexture (GL_TEXTURE_2D, name);
      glUniform1i (cutoutLocation, batch.cutout ? 1 : 0);
      glDrawArrays (GL_TRIANGLES, batch.first, batch.count);
    }
  glBindTexture (GL_TEXTURE_2D, 0);
}

void
DhGpuWidget::releaseScene ()
{
  for (unsigned int texture : textures)
    {
      if (texture != 0)
        glDeleteTextures (1, &texture);
    }
  textures.clear ();
  solidBatches.clear ();
  blendedBatches.clear ();
  /* The overlay buffer is rebuilt against whatever scene is set next. */
  highlightDirty = true;
  highlightVertices = 0;
}

#else /* DH_HAVE_OPENGL */

DhGpuWidget::DhGpuWidget (QWidget *parent) : QWidget (parent) {}

DhGpuWidget::~DhGpuWidget () = default;

bool
DhGpuWidget::available ()
{
  return false;
}

void
DhGpuWidget::setScene (const DhlrcScene *) {}

void
DhGpuWidget::setCamera (const DhlrcCamera &) {}

void
DhGpuWidget::highlightChanged () {}

bool
DhGpuWidget::drew () const
{
  return false;
}

const QString &
DhGpuWidget::problem () const
{
  static const QString noBackend
      = QStringLiteral ("this build has no OpenGL support");
  return noBackend;
}

#endif /* DH_HAVE_OPENGL */
