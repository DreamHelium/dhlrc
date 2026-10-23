/*! Drawing a scene's quads into an image.
 *
 * A quad becomes two triangles, which are filled with a depth test so the
 * picture comes out right whatever order the quads arrive in. What a face
 * looks like is settled here, because this is the first place that has both
 * the geometry and the texture:
 *
 * - a face whose normal points away from the camera is not drawn,
 * - a face that asks for it is darkened by the direction it looks,
 * - a tint multiplies the texture, and
 * - the texture's alpha decides whether the face covers, drops or blends.
 *
 * The fill runs in horizontal bands, each band its own thread over its own rows
 * of the colour and depth buffers. Everything a pixel does reads and writes
 * only that pixel — the depth test, the blend and the texture sample — and a
 * band owns every row of its pixels, so the bands write exactly the bytes one
 * thread over the whole picture would have, in the same order per pixel. That
 * is what makes the parallel fill invisible in the output.
 *
 * The threads are scoped `std::thread`s rather than a work-stealing pool: a
 * frame is one fork-join of a few dozen bands, which needs no scheduler, and
 * the crate stays free of a dependency it would otherwise carry everywhere.
 */

use std::thread;

use crate::camera::{Camera, Mat4, Vec3, cross, dot, normalize, sub};
use crate::image::Image;
use crate::scene::{Scene, SceneQuad};
use crate::texture::{Alpha, Texture};

/// The image a scene's quads make, `width` by `height`, started transparent.
pub fn render(scene: &Scene, camera: &Camera, width: u32, height: u32) -> Image {
    let mut renderer = Renderer::new();
    renderer.render(scene, camera, width, height);
    /* The renderer is discarded here, so its buffer is handed over rather than
     * copied out of it. */
    std::mem::replace(&mut renderer.image, Image::new(0, 0))
}

/// A renderer whose buffers live as long as it does.
///
/// [`render`] allocates and clears a colour and a depth buffer on every call,
/// which at a quarter of a megapixel is more than a megabyte of `memset`
/// before the first triangle is drawn — the price the Qt view and the
/// terminal's `render` pay again on every mouse move. A `Renderer` keeps both
/// buffers, reuses them when the next picture is the size of the last one, and
/// hands out a borrowed image, so a frontend that draws frame after frame pays
/// for the pixels it changes and nothing else.
pub struct Renderer {
    image: Image,
    depth: Vec<f32>,
    /// The frame's triangles, kept so that a frame does not reallocate them.
    triangles: Vec<Triangle>,
}

impl Renderer {
    /// A renderer with no buffers yet; the first render sizes them.
    pub fn new() -> Self {
        Self {
            image: Image::new(0, 0),
            depth: Vec::new(),
            triangles: Vec::new(),
        }
    }

    /// Draws `scene` and returns the picture, reusing the buffers of the last
    /// render when they already have the right size.
    pub fn render(&mut self, scene: &Scene, camera: &Camera, width: u32, height: u32) -> &Image {
        let rows = band_rows(height.max(1));
        self.render_bands(scene, camera, width, height, rows)
    }

    /// The picture of the last [`Renderer::render`].
    pub fn image(&self) -> &Image {
        &self.image
    }

    /// Draws with `rows_per_band` rows in each band. [`Renderer::render`] picks
    /// that from the picture and the machine; a test picks it to force a band
    /// boundary wherever it likes.
    fn render_bands(
        &mut self,
        scene: &Scene,
        camera: &Camera,
        width: u32,
        height: u32,
        rows_per_band: u32,
    ) -> &Image {
        let width = width.max(1);
        let height = height.max(1);
        /* `resize` keeps the allocation when the size has not changed; the
         * clearing is left to the bands, which own every row between them and
         * so can clear in parallel. */
        self.image.width = width;
        self.image.height = height;
        self.image.pixels.resize(width as usize * height as usize * 4, 0);
        self.depth.resize(width as usize * height as usize, f32::INFINITY);

        let aspect = width as f32 / height as f32;
        let matrix = camera.view_projection(aspect, scene.radius.max(0.5));
        let eye = camera.eye();
        self.prepare_quads(scene, &matrix, eye, width, height);

        let columns = width as usize;
        let color_row = columns * 4;
        let rows_per_band = (rows_per_band.max(1) as usize).min(height as usize);
        let triangles = &self.triangles;
        let textures = &scene.textures;
        thread::scope(|scope| {
            for (band, (color, depths)) in self
                .image
                .pixels
                .chunks_mut(rows_per_band * color_row)
                .zip(self.depth.chunks_mut(rows_per_band * columns))
                .enumerate()
            {
                scope.spawn(move || {
                    color.fill(0);
                    depths.fill(f32::INFINITY);
                    let first_row = (band * rows_per_band) as i64;
                    let rows = (color.len() / color_row) as i64;
                    for triangle in triangles {
                        fill_band(
                            triangle,
                            textures.get(triangle.texture),
                            color,
                            depths,
                            columns,
                            first_row,
                            rows,
                        );
                    }
                });
            }
        });
        &self.image
    }

    /// Projects and clips the scene's quads into this frame's triangles.
    ///
    /// The quads are prepared in parallel chunks, but the list keeps the
    /// scene's own order: two see-through faces over the same pixel blend in
    /// the order they are drawn, and that order has to be the one the
    /// single-threaded fill used. A chunk is kept large — preparing a quad is a
    /// handful of multiplies, so a small one would be all dispatch.
    ///
    /// The highlight's outline goes last, so that where it lands on the very
    /// same depth as a face it wins — it is the thing the view is pointing at.
    fn prepare_quads(&mut self, scene: &Scene, matrix: &Mat4, eye: Vec3, width: u32, height: u32) {
        self.triangles.clear();
        for quads in [&scene.quads, &scene.overlay] {
            if quads.is_empty() {
                continue;
            }
            let chunk = quads.len().div_ceil(workers() * 4).max(64);
            let mut prepared: Vec<Vec<Triangle>> = (0..quads.len().div_ceil(chunk))
                .map(|_| Vec::new())
                .collect();
            thread::scope(|scope| {
                for (out, quads) in prepared.iter_mut().zip(quads.chunks(chunk)) {
                    scope.spawn(move || {
                        out.reserve(quads.len() * 2);
                        for quad in quads {
                            prepare(quad, matrix, eye, width, height, out);
                        }
                    });
                }
            });
            for mut chunk in prepared {
                self.triangles.append(&mut chunk);
            }
        }
    }
}

impl Default for Renderer {
    fn default() -> Self {
        Self::new()
    }
}

/// The shortest band, in rows.
///
/// A band pays for one pass over the frame's triangles before it fills
/// anything, so a very short band spends more time looking at geometry that
/// misses it than drawing. Sixteen rows is tens of thousands of pixels — at
/// 900 across, more than fourteen thousand — which measured as noise next to
/// the fill, and it keeps the number of bands a picture of a given height
/// makes bounded as a scene grows.
const MIN_BAND_ROWS: u32 = 16;

/// How many rows each band gets, as a whole picture of `height` rows is split.
///
/// Bands are cut about four to a worker. That is more than one each on
/// purpose, and it was measured to pay: the rows of a view hold very different
/// amounts of geometry (the far edge of a region is a sliver, its near face
/// fills the picture), so a picture cut into few, tall bands leaves most of
/// the machine waiting at the end of the frame.
fn band_rows(height: u32) -> u32 {
    let height = height.max(1);
    let wanted = (workers() as u32).saturating_mul(4).clamp(1, height);
    let rows = height.div_ceil(wanted);
    /* A band pays for one pass over the frame's triangles before it fills
     * anything, so a very short band spends its time looking at geometry that
     * misses it. Sixteen rows is tens of thousands of pixels — at 900 across,
     * more than fourteen thousand — which measured as noise next to the fill,
     * and it keeps the number of bands a tall picture makes bounded. */
    rows.max(MIN_BAND_ROWS.min(height))
}

/// How many threads a frame is cut across: the machine's own, or one when it
/// will not say.
fn workers() -> usize {
    thread::available_parallelism()
        .map(|count| count.get())
        .unwrap_or(1)
}

/// The corners a quad can have once the near plane has cut it. The plane meets
/// a closed polygon an even number of times, so four corners become six at the
/// very most; the array is rounded up for slack.
const MAX_CORNERS: usize = 8;

/// A vertex in clip space, carrying what has to be interpolated across it.
#[derive(Clone)]
struct Vertex {
    clip: [f32; 4],
    uv: [f32; 2],
}

impl Vertex {
    fn lerp(from: &Vertex, to: &Vertex, t: f32) -> Vertex {
        let mut clip = [0.0f32; 4];
        for axis in 0..4 {
            clip[axis] = from.clip[axis] + (to.clip[axis] - from.clip[axis]) * t;
        }
        Vertex {
            clip,
            uv: [
                from.uv[0] + (to.uv[0] - from.uv[0]) * t,
                from.uv[1] + (to.uv[1] - from.uv[1]) * t,
            ],
        }
    }
}

/// A vertex on the picture, with what the fill needs.
#[derive(Clone, Copy)]
struct Screen {
    x: f32,
    y: f32,
    /// The depth, in the projection's own units; smaller is nearer.
    z: f32,
    uv: [f32; 2],
    /// `1 / w`, which is what interpolates linearly in screen space.
    inv_w: f32,
}

/// One triangle of one quad, in the form the fill wants, with everything that
/// does not depend on a pixel already worked out. A band only has to compare
/// its rows against `min_y`/`max_y` until it finds a triangle that reaches it.
struct Triangle {
    a: Screen,
    b: Screen,
    c: Screen,
    /// The barycentric divisor, turned positive.
    area: f32,
    texture: usize,
    alpha: Alpha,
    tint: Option<[u8; 3]>,
    light: f32,
    /// The pixel rectangle the triangle can touch, clamped to the picture.
    min_x: i64,
    max_x: i64,
    min_y: i64,
    max_y: i64,
}

/// Projects and clips one quad, appending the triangles it makes to `out`.
fn prepare(
    quad: &SceneQuad,
    matrix: &Mat4,
    eye: Vec3,
    width: u32,
    height: u32,
    out: &mut Vec<Triangle>,
) {
    let normal = normalize(
        cross(
            sub(quad.positions[1], quad.positions[0]),
            sub(quad.positions[3], quad.positions[0]),
        ),
        [0.0, 1.0, 0.0],
    );
    /* The winding of a model's face points out of its box, so a face the
     * camera is behind is one the block itself hides. */
    if dot(normal, sub(eye, quad.positions[0])) <= 0.0 {
        return;
    }
    let light = quad.light;

    let mut polygon = [(); 4].map(|()| Vertex {
        clip: [0.0; 4],
        uv: [0.0; 2],
    });
    for (corner, (point, uv)) in quad.positions.iter().zip(quad.uv).enumerate() {
        polygon[corner] = Vertex {
            clip: matrix.project(*point),
            uv,
        };
    }
    let mut clipped = [(); MAX_CORNERS].map(|()| Vertex {
        clip: [0.0; 4],
        uv: [0.0; 2],
    });
    let corners = clip_near(&polygon, &mut clipped);
    if corners < 3 {
        return;
    }

    let mut screen = [Screen {
        x: 0.0,
        y: 0.0,
        z: 0.0,
        uv: [0.0; 2],
        inv_w: 0.0,
    }; MAX_CORNERS];
    let mut low_x = f32::INFINITY;
    let mut high_x = f32::NEG_INFINITY;
    let mut low_y = f32::INFINITY;
    let mut high_y = f32::NEG_INFINITY;
    for corner in 0..corners {
        let vertex = &clipped[corner];
        let inv_w = 1.0 / vertex.clip[3];
        let x = (vertex.clip[0] * inv_w * 0.5 + 0.5) * width as f32;
        let y = (1.0 - (vertex.clip[1] * inv_w * 0.5 + 0.5)) * height as f32;
        low_x = low_x.min(x);
        high_x = high_x.max(x);
        low_y = low_y.min(y);
        high_y = high_y.max(y);
        screen[corner] = Screen {
            x,
            y,
            z: vertex.clip[2] * inv_w,
            uv: vertex.uv,
            inv_w,
        };
    }
    /* A quad that cannot reach a pixel centre is dropped here, before two
     * triangles are set up for it. A view zoomed into a corner of a region
     * spends most of its quads this way. */
    if high_x < 0.0 || low_x >= width as f32 || high_y < 0.0 || low_y >= height as f32 {
        return;
    }

    for corner in 1..corners - 1 {
        let a = screen[0];
        let (b, c) = (screen[corner], screen[corner + 1]);
        let signed = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        if signed.abs() < 1e-9 {
            continue;
        }
        let (b, c, area) = if signed < 0.0 {
            (c, b, -signed)
        } else {
            (b, c, signed)
        };

        let min_x = a.x.min(b.x).min(c.x).floor().max(0.0) as i64;
        let max_x = a.x.max(b.x).max(c.x).ceil().min(width as f32 - 1.0) as i64;
        let min_y = a.y.min(b.y).min(c.y).floor().max(0.0) as i64;
        let max_y = a.y.max(b.y).max(c.y).ceil().min(height as f32 - 1.0) as i64;
        if min_x > max_x || min_y > max_y {
            continue;
        }

        out.push(Triangle {
            a,
            b,
            c,
            area,
            texture: quad.texture,
            alpha: quad.alpha,
            tint: quad.tint,
            light,
            min_x,
            max_x,
            min_y,
            max_y,
        });
    }
}

/// Keeps the part of a polygon in front of the camera: everything with a `w`
/// above zero. A quad that straddles the camera plane would otherwise be
/// projected through it and come out as a smear across the picture.
fn clip_near(polygon: &[Vertex; 4], out: &mut [Vertex; MAX_CORNERS]) -> usize {
    const EPSILON: f32 = 1e-4;
    let mut len = 0;
    for index in 0..polygon.len() {
        let current = &polygon[index];
        let next = &polygon[(index + 1) % polygon.len()];
        let current_in = current.clip[3] > EPSILON;
        let next_in = next.clip[3] > EPSILON;
        if current_in {
            out[len] = current.clone();
            len += 1;
        }
        if current_in != next_in {
            let t = (EPSILON - current.clip[3]) / (next.clip[3] - current.clip[3]);
            out[len] = Vertex::lerp(current, next, t);
            len += 1;
        }
    }
    len
}

/// Fills the part of one triangle that falls in `first_row..first_row + rows`.
///
/// `color` and `depths` hold only the band's rows, so a pixel is addressed by
/// its offset from `first_row`. The edge functions still work in the picture's
/// own coordinates, which keeps every intermediate value the one the
/// whole-picture fill would have computed.
fn fill_band(
    triangle: &Triangle,
    texture: &Texture,
    color: &mut [u8],
    depths: &mut [f32],
    width: usize,
    first_row: i64,
    rows: i64,
) {
    let last_row = first_row + rows - 1;
    if triangle.max_y < first_row || triangle.min_y > last_row {
        return;
    }
    let min_y = triangle.min_y.max(first_row);
    let max_y = triangle.max_y.min(last_row);
    let a = &triangle.a;
    let b = &triangle.b;
    let c = &triangle.c;
    let area = triangle.area;

    for y in min_y..=max_y {
        let row = (y - first_row) as usize * width;
        for x in triangle.min_x..=triangle.max_x {
            let px = x as f32 + 0.5;
            let py = y as f32 + 0.5;

            /* The edge functions are the barycentric weights, one per corner. */
            let wa = ((c.x - b.x) * (py - b.y) - (c.y - b.y) * (px - b.x)) / area;
            let wb = ((a.x - c.x) * (py - c.y) - (a.y - c.y) * (px - c.x)) / area;
            let wc = ((b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x)) / area;
            if wa < 0.0 || wb < 0.0 || wc < 0.0 {
                continue;
            }

            let depth = wa * a.z + wb * b.z + wc * c.z;
            let at = row + x as usize;
            if depth >= depths[at] {
                continue;
            }

            let inv_w = wa * a.inv_w + wb * b.inv_w + wc * c.inv_w;
            let u = (wa * a.uv[0] * a.inv_w + wb * b.uv[0] * b.inv_w + wc * c.uv[0] * c.inv_w)
                / inv_w;
            let v = (wa * a.uv[1] * a.inv_w + wb * b.uv[1] * b.inv_w + wc * c.uv[1] * c.inv_w)
                / inv_w;

            let texel = texture.sample(u, v);
            let mut alpha = texel[3] as f32 / 255.0;
            if triangle.alpha == Alpha::Cutout && alpha < 0.5 {
                continue;
            }
            if alpha <= 0.0 {
                continue;
            }
            alpha = alpha.min(1.0);

            let mut shaded = [
                texel[0] as f32 * triangle.light,
                texel[1] as f32 * triangle.light,
                texel[2] as f32 * triangle.light,
            ];
            if let Some(tint) = triangle.tint {
                for channel in 0..3 {
                    shaded[channel] *= tint[channel] as f32 / 255.0;
                }
            }

            let pixel = at * 4;
            if alpha >= 0.999 {
                for channel in 0..3 {
                    color[pixel + channel] = shaded[channel].round().clamp(0.0, 255.0) as u8;
                }
                color[pixel + 3] = 255;
            } else {
                /* Straight-alpha "over": what is already there shows through. */
                let behind = color[pixel + 3] as f32 / 255.0;
                let out_alpha = alpha + behind * (1.0 - alpha);
                for channel in 0..3 {
                    let mixed = (shaded[channel] * alpha
                        + color[pixel + channel] as f32 * behind * (1.0 - alpha))
                        / out_alpha.max(1e-6);
                    color[pixel + channel] = mixed.round().clamp(0.0, 255.0) as u8;
                }
                color[pixel + 3] = (out_alpha * 255.0).round() as u8;
            }
            /* A face that is mostly there hides what is behind it; a wisp of
             * water does not. */
            if alpha >= 0.5 {
                depths[at] = depth;
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{Renderer, render};
    use crate::camera::Camera;
    use crate::image::Image;
    use crate::scene::{BOUNDS_COLOUR, HIGHLIGHT_COLOUR, MARKER_COLOUR, Scene, SceneQuad};
    use crate::texture::{Alpha, Texture, Textures};

    /// A one-unit quad in the plane `z`, wound so its front looks along `+z`.
    fn quad(z: f32, texture: usize, alpha: Alpha, tint: Option<[u8; 3]>) -> SceneQuad {
        SceneQuad {
            positions: [[0.0, 0.0, z], [1.0, 0.0, z], [1.0, 1.0, z], [0.0, 1.0, z]],
            uv: [[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]],
            texture,
            alpha,
            tint,
            shade: false,
            light: 1.0,
        }
    }

    /// A scene with one texture and the quads given, looking at `[0.5, 0.5, 0]`.
    fn scene(quads: Vec<SceneQuad>, color: [u8; 4]) -> Scene {
        let mut textures = Textures::new();
        /* Index 1 is the white texel an outline's tint is drawn with. */
        textures.push(Texture::solid([255, 255, 255, 255]));
        let index = textures.push(Texture::solid(color));
        Scene {
            quads: quads
                .into_iter()
                .map(|mut quad| {
                    quad.texture = index;
                    quad
                })
                .collect(),
            textures,
            center: [0.5, 0.5, 0.0],
            radius: 1.0,
            min: [0.0; 3],
            max: [1.0; 3],
            skipped: Vec::new(),
            bounds: None,
            highlights: Vec::new(),
            highlight_colour: HIGHLIGHT_COLOUR,
            bounds_colour: BOUNDS_COLOUR,
            marker: None,
            marker_colour: MARKER_COLOUR,
            overlay: Vec::new(),
        }
    }

    /// A camera in front of the quads, looking straight at them.
    fn camera() -> Camera {
        let mut camera = Camera::new([0.5, 0.5, 0.0], 4.0);
        camera.yaw = 0.0;
        camera.pitch = 0.0;
        camera
    }

    /// A scene of quads sixteen units across, framed by a camera twice that
    /// away: the quads reach every corner of the picture.
    fn wide_scene(quads: Vec<SceneQuad>, color: [u8; 4]) -> Scene {
        let mut textures = Textures::new();
        /* Index 1 is the white texel an outline's tint is drawn with. */
        textures.push(Texture::solid([255, 255, 255, 255]));
        let index = textures.push(Texture::solid(color));
        Scene {
            quads: quads
                .into_iter()
                .map(|mut quad| {
                    quad.texture = index;
                    quad
                })
                .collect(),
            textures,
            center: [0.0; 3],
            radius: 12.0,
            min: [-8.0; 3],
            max: [8.0; 3],
            skipped: Vec::new(),
            bounds: None,
            highlights: Vec::new(),
            highlight_colour: HIGHLIGHT_COLOUR,
            bounds_colour: BOUNDS_COLOUR,
            marker: None,
            marker_colour: MARKER_COLOUR,
            overlay: Vec::new(),
        }
    }

    /// A plane in `z` seen at an angle, with UVs that run past the texture so
    /// the sample wraps, and part of it behind the camera when `behind` asks.
    fn wide_quad(z: f32, behind: bool, tint: Option<[u8; 3]>) -> SceneQuad {
        let far = if behind { -20.0 } else { z };
        SceneQuad {
            positions: [
                [-8.0, -8.0, far],
                [8.0, -8.0, z],
                [8.0, 8.0, z],
                [-8.0, 8.0, far],
            ],
            uv: [[0.0, 0.0], [4.0, 0.0], [4.0, 4.0], [0.0, 4.0]],
            texture: 0,
            alpha: Alpha::Translucent,
            tint,
            shade: false,
            light: 0.8,
        }
    }

    /// Renders with `rows` rows in each band, whatever the heuristics would
    /// have picked, so a test can put a band boundary wherever it likes.
    fn banded(scene: &Scene, camera: &Camera, width: u32, height: u32, rows: u32) -> Image {
        let mut renderer = Renderer::new();
        renderer.render_bands(scene, camera, width, height, rows);
        std::mem::replace(&mut renderer.image, Image::new(0, 0))
    }

    #[test]
    fn a_face_towards_the_camera_is_drawn() {
        let scene = scene(vec![quad(0.5, 0, Alpha::Opaque, None)], [255, 0, 0, 255]);
        let image = render(&scene, &camera(), 32, 32);
        assert_eq!(image.pixel(16, 16), [255, 0, 0, 255]);
        assert_eq!(image.pixel(0, 0), [0, 0, 0, 0], "the corner stays clear");
    }

    #[test]
    fn a_face_pointing_away_is_not_drawn() {
        let mut flipped = quad(0.5, 0, Alpha::Opaque, None);
        flipped.positions.swap(0, 2);
        let scene = scene(vec![flipped], [255, 0, 0, 255]);
        let image = render(&scene, &camera(), 32, 32);
        assert_eq!(image.pixel(16, 16), [0, 0, 0, 0]);
    }

    #[test]
    fn the_nearer_of_two_faces_wins() {
        let scene = scene(
            vec![
                quad(0.9, 0, Alpha::Opaque, None),
                quad(0.2, 0, Alpha::Opaque, None),
            ],
            [255, 0, 0, 255],
        );
        let image = render(&scene, &camera(), 32, 32);
        assert_eq!(image.pixel(16, 16), [255, 0, 0, 255]);
    }

    #[test]
    fn a_cutout_face_drops_its_clear_pixels() {
        let scene = scene(
            vec![quad(0.5, 0, Alpha::Cutout, None)],
            [255, 0, 0, 0],
        );
        let image = render(&scene, &camera(), 32, 32);
        assert_eq!(image.pixel(16, 16), [0, 0, 0, 0]);
    }

    #[test]
    fn a_tint_multiplies_the_texture() {
        let scene = scene(
            vec![quad(0.5, 0, Alpha::Opaque, Some([0, 128, 255]))],
            [255, 255, 255, 255],
        );
        let image = render(&scene, &camera(), 32, 32);
        assert_eq!(image.pixel(16, 16), [0, 128, 255, 255]);
    }

    /// A band owns whole rows, and a pixel is only ever touched by triangles
    /// that cover it, so cutting the picture into bands — any number of rows
    /// each, and at any row — must reproduce the whole-picture fill exactly.
    #[test]
    fn the_highlight_outline_is_drawn_over_the_scene() {
        use crate::scene::Highlight;

        let mut scene = scene(vec![quad(0.0, 0, Alpha::Opaque, None)], [200, 60, 60, 255]);
        let plain = render(&scene, &camera(), 64, 64);
        scene.set_highlight(Some(Highlight {
            min: [0.0, 0.0, 0.0],
            max: [1.0, 1.0, 0.0],
        }));
        let marked = render(&scene, &camera(), 64, 64);

        assert_ne!(plain.pixels, marked.pixels, "the outline has to be visible");
        /* The outline is black; the face under it is red, so the picture now
         * holds pixels that are neither. */
        let black = marked
            .pixels
            .chunks(4)
            .filter(|pixel| pixel[0] < 60 && pixel[1] < 60 && pixel[2] < 60)
            .count();
        assert!(black > 20, "only {black} outline pixels came out");

        /* Clearing it gives the plain picture back. */
        scene.set_highlight(None);
        assert_eq!(render(&scene, &camera(), 64, 64).pixels, plain.pixels);
    }

    #[test]
    fn the_region_box_is_drawn_in_its_own_colour() {
        use crate::scene::Highlight;

        /* The outline's tint must survive the texture: a box drawn through the
         * missing-texture checker would come out in the checker's colours. */
        let mut scene = scene(vec![quad(0.0, 0, Alpha::Opaque, None)], [200, 60, 60, 255]);
        scene.set_bounds(Some(Highlight {
            min: [0.0, 0.0, 0.0],
            max: [1.0, 1.0, 0.0],
        }));
        let image = render(&scene, &camera(), 64, 64);
        let wanted = crate::scene::bounds_colour();
        let found = image.pixels.chunks(4).any(|pixel| {
            pixel[0] == wanted[0] && pixel[1] == wanted[1] && pixel[2] == wanted[2]
        });
        assert!(found, "the region box was not drawn in {wanted:?}");
    }

    #[test]
    fn bands_draw_exactly_what_one_pass_draws() {
        let cases = [
            (
                "one quad across every band",
                wide_scene(vec![wide_quad(0.0, false, None)], [255, 64, 0, 255]),
            ),
            (
                "two quads at different depths, blended",
                wide_scene(
                    vec![
                        wide_quad(3.0, false, None),
                        wide_quad(-3.0, false, Some([0, 255, 0])),
                    ],
                    [255, 255, 255, 128],
                ),
            ),
            (
                "a quad through the near plane",
                wide_scene(vec![wide_quad(4.0, true, None)], [10, 200, 255, 255]),
            ),
        ];
        /* An odd size, so the last band is a short one whichever row count is
         * tried. */
        let (width, height) = (97, 61);
        for (name, scene) in cases {
            let camera = Camera::new(scene.center, 20.0);
            let whole = banded(&scene, &camera, width, height, height);
            for rows in [1, 2, 3, 7, 16, 31, 60, 61, 4096] {
                assert_eq!(
                    banded(&scene, &camera, width, height, rows),
                    whole,
                    "{name}: bands of {rows} rows"
                );
            }
            assert_eq!(
                render(&scene, &camera, width, height),
                whole,
                "{name}: the shipped render"
            );
        }
    }

    /// The buffers a frame allocates are the ones the next frame uses, so a
    /// second render of the same size must land on the same picture as the
    /// first and must not have grown them.
    #[test]
    fn a_reused_renderer_redraws_the_same_picture() {
        let scene = wide_scene(vec![wide_quad(0.0, false, None)], [255, 64, 0, 255]);
        let camera = Camera::new(scene.center, 20.0);
        let mut renderer = Renderer::new();
        let first = renderer.render(&scene, &camera, 64, 48).clone();
        let color_capacity = renderer.image.pixels.capacity();
        let depth_capacity = renderer.depth.capacity();
        let second = renderer.render(&scene, &camera, 64, 48).clone();
        assert_eq!(first, second);
        assert_eq!(renderer.image.pixels.capacity(), color_capacity);
        assert_eq!(renderer.depth.capacity(), depth_capacity);
        assert_eq!(first.width, 64);
        assert_eq!(first.height, 48);
        assert_eq!(renderer.image(), &second);
    }
}
