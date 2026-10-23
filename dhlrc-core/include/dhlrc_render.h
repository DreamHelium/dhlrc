#ifndef DHLRC_RENDER_H
#define DHLRC_RENDER_H

#include <stddef.h>
#include <stdint.h>

/* Drawing a region.
 *
 * This is the C side of `dhlrc_core`'s `render` feature (see
 * `dhlrc-core/src/render.rs`): a region and the core handle go in, a picture
 * comes out. A scene is built once — reading every model, every texture and
 * every block — and then drawn as often as the view moves, so a drag only
 * re-rasterises.
 *
 * The pixels a draw returns are owned by the scene and stay valid only until
 * the next draw on that scene or its free; copy them into a `QImage` before
 * drawing again.
 */

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct DhlrcScene DhlrcScene;

  typedef struct DhlrcCamera
  {
    double target[3];
    double yaw;
    double pitch;
    double distance;
    double fov;
    /* Any non-zero value uses the orthographic projection. */
    int32_t orthographic;
  } DhlrcCamera;

  /* A progress callback: `value` is the percentage (0-100), `user` the pointer
   * the caller passed in. Called from the thread that builds the scene. */
  typedef void (*DhlrcProgressFunc) (void *user, int32_t value);

  /* Builds the scene of `region` (a `Region *` from `libregion_rs`), reading
   * models and textures through the resources `core` has configured. NULL
   * when a handle is NULL or the scene could not be built. */
  DhlrcScene *dhlrc_render_scene_new (void *core, const void *region);

  /* Like `dhlrc_render_scene_new`, reporting how far the build is: 0 first,
   * then once per vertical slice, and 100 when the scene is ready. */
  DhlrcScene *dhlrc_render_scene_new_reported (void *core, const void *region,
                                               DhlrcProgressFunc progress,
                                               void *user);

  /* Like `dhlrc_render_scene_new`, of one horizontal slice of the region: only
   * blocks whose `y` is between `y_low` and `y_high`, both included, are drawn,
   * and the view is framed around them. `y_low` above `y_high` draws nothing. */
  DhlrcScene *dhlrc_render_scene_new_slice (void *core, const void *region,
                                            int32_t y_low, int32_t y_high);

  /* Frees a scene built by `dhlrc_render_scene_new`. NULL is allowed. */
  void dhlrc_render_scene_free (DhlrcScene *scene);

  /* How many quads the scene holds, and how many palette entries were left
   * out (a block whose model could not be read). */
  size_t dhlrc_render_scene_quads (const DhlrcScene *scene);
  size_t dhlrc_render_scene_skipped (const DhlrcScene *scene);

  /* Why one palette entry was left out. Borrowed from the scene; valid for as
   * long as it lives. NULL for an index out of range. */
  const char *dhlrc_render_scene_skipped_reason (const DhlrcScene *scene,
                                                 size_t index);

  /* Points `camera` at the whole scene so it fits a `width` by `height`
   * picture, keeping the camera's angles. Call it when a region is first
   * shown, or when the view is reset. */
  void dhlrc_render_fit (const DhlrcScene *scene, DhlrcCamera *camera,
                         uint32_t width, uint32_t height);

  /* Slides the target across the plane the camera looks at, by a fraction of
   * the picture's height: `right` positive goes towards the camera's right,
   * `up` positive towards the top. This is a middle/right button drag. */
  void dhlrc_render_pan (DhlrcCamera *camera, double right, double up);

  /* Draws the scene and returns `width * height * 4` RGBA bytes, row by row
   * from the top, on a transparent background. Owned by the scene and valid
   * until the next draw on it or its free; NULL when a handle is NULL or a
   * size is zero. */
  const uint8_t *dhlrc_render_image (DhlrcScene *scene,
                                     const DhlrcCamera *camera, uint32_t width,
                                     uint32_t height);

  /* An item icon, drawn the way the game draws a block item: the block's model
   * seen from the south-east, 30 degrees above the horizon, orthographic. Only
   * for items whose model is a block (a stair, a slab, a torch); an item drawn
   * as a flat sprite yields NULL, and the caller should use
   * `dhlrc_core_item_texture` instead.
   *
   * `size` is the width and height in pixels; `width` and `height` receive it
   * back and may be NULL. The bytes are owned (RGBA, rows from the top,
   * transparent background), free them with `dhlrc_core_bytes_free`. NULL when
   * a handle is NULL, the item is a sprite, or its model could not be read. */
  uint8_t *dhlrc_render_item_icon (void *core, const char *item, uint32_t size,
                                   uint32_t *width, uint32_t *height);

  /* The block under pixel `(x, y)`, written to `block` as a region coordinate
   * (three `int32_t`). Returns 1 on a hit and 0 on a miss. */
  int32_t dhlrc_render_pick (const DhlrcScene *scene, const DhlrcCamera *camera,
                             uint32_t width, uint32_t height, double x,
                             double y, int32_t *block);

  /* ---- Drawing it on a GPU instead -------------------------------------
   *
   * The same scene, handed over piece by piece, so a `QOpenGLWidget` can draw
   * it with the same geometry, the same light and the same textures the
   * software rasteriser uses. The pieces are borrowed from the scene and stay
   * valid for as long as it does.
   */

  typedef struct DhlrcQuad
  {
    /* Block space, wound so the front looks outwards; two triangles per quad,
     * `0, 1, 2` and `0, 2, 3`. */
    float positions[4][3];
    float uv[4][2];
    /* What the texture is multiplied by; `{1, 1, 1}` for no tint. */
    float tint[3];
    /* 1.0 in full light, down to 0.5 on a block's bottom. */
    float light;
    /* Which of the scene's textures to sample. */
    uint32_t texture;
    /* How the texture's alpha behaves: 0 covers, 1 is cut out where it is
     * clear, 2 is blended. */
    int32_t alpha;
  } DhlrcQuad;

  /* How many quads the scene holds. */
  size_t dhlrc_render_scene_quad_count (const DhlrcScene *scene);

  /* One quad, written to `quad`. Returns 1, or 0 for an index past the end. */
  int32_t dhlrc_render_scene_quad (const DhlrcScene *scene, size_t index,
                                   DhlrcQuad *quad);

  /* How many textures the scene holds. Index 0 is always the missing-texture
   * checker, so a quad's index is never out of range. */
  size_t dhlrc_render_scene_texture_count (const DhlrcScene *scene);

  /* One texture as `width * height * 4` RGBA bytes, top-down — what
   * `glTexImage2D` takes. Any output pointer may be NULL. Returns 1, or 0 for
   * an index past the end. */
  int32_t dhlrc_render_scene_texture (const DhlrcScene *scene, size_t index,
                                      const uint8_t **pixels, uint32_t *width,
                                      uint32_t *height);

  /* The view-projection matrix of `camera` for a `width` by `height` picture:
   * sixteen column-major floats, the very matrix the software rasteriser
   * uses. */
  void dhlrc_render_matrix (const DhlrcScene *scene, const DhlrcCamera *camera,
                            uint32_t width, uint32_t height, float *out16);

  /* ---- Pointing at a block -------------------------------------------------
   *
   * The block a view is over is drawn as an outline over the scene: twelve
   * thin black edges around its box. `dhlrc_render_image` draws it, and a GPU
   * reads the very same quads below, so both show the same box.
   */

  /* Points the view at a block, or at nothing: `block` is six floats, the low
   * corner then the high one in block space (one block is `x, y, z, x + 1,
   * y + 1, z + 1`), or NULL to clear the highlight. Returns 1 when a box was
   * set and 0 when it was cleared. */
  int32_t dhlrc_render_scene_set_highlight (DhlrcScene *scene,
                                            const float *block);

  /* Points the view at several blocks at once: `boxes` holds `count` boxes of
   * six floats each, laid out as one box is for the highlight above — the low
   * corner then the high one. A count of zero, or a null `boxes`, clears every
   * pick. The picks are drawn in the highlight colour, after the region's box.
   * Returns how many boxes were set. */
  int32_t dhlrc_render_scene_set_highlights (DhlrcScene *scene,
                                             const float *boxes, size_t count);

  /* Sets the colour the picked blocks' boxes are drawn in: three floats, red
   * first, each in 0..1 and clamped to it. NULL is ignored. */
  void dhlrc_render_scene_set_highlight_colour (DhlrcScene *scene,
                                                const float *rgb);

  /* Sets the colour the region's box is drawn in, with the same three floats as
   * the highlight colour. NULL is ignored. */
  void dhlrc_render_scene_set_bounds_colour (DhlrcScene *scene,
                                             const float *rgb);

  /* Marks the cell chosen in the selected-blocks list, or clears the marker:
   * `box` is the same six floats as the highlight takes, NULL clearing it. The
   * marker is drawn after the region's box and the picks, in its own colour, so
   * the cell a list points at stays visible. */
  void dhlrc_render_scene_set_marker (DhlrcScene *scene, const float *box);

  /* Sets the colour the marker's box is drawn in: three floats, red first, each
   * in 0..1 and clamped to it. NULL is ignored. */
  void dhlrc_render_scene_set_marker_colour (DhlrcScene *scene,
                                             const float *rgb);

  /* The box the block at a whole-numbered position is drawn in, as the six
   * floats `dhlrc_render_scene_set_highlight` takes: a slab gives half a cell,
   * not the whole one. Returns 1, or 0 when the cell drew nothing (a block that
   * was culled away or is not there), so a caller can fall back to the cell. */
  int32_t dhlrc_render_scene_block_box (const DhlrcScene *scene, int32_t x,
                                        int32_t y, int32_t z, float *block);

  /* Shows, or hides, the box the region is drawn in — the whole region, or the
   * slice being drawn — with the same six floats as the highlight above. It is
   * drawn in its own colour, apart from the box under the pointer. Returns 1
   * when a box was set and 0 when it was cleared. */
  int32_t dhlrc_render_scene_set_bounds (DhlrcScene *scene, const float *block);

  /* How many quads the boxes drawn over the scene come to — the region's extent
   * and every picked block together — or 0 when nothing is shown. */
  size_t dhlrc_render_scene_overlay_count (const DhlrcScene *scene);

  /* One quad of those boxes, in the same shape as a scene quad. Returns 1, or
   * 0 for an index past the end. */
  int32_t dhlrc_render_scene_overlay_quad (const DhlrcScene *scene,
                                           size_t index, DhlrcQuad *quad);

#ifdef __cplusplus
}
#endif

#endif /* DHLRC_RENDER_H */
