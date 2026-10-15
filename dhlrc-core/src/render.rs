/*! The C ABI the Qt frontend draws a region through.
 *
 * The Qt side owns a `Region` (from `libregion_rs`) and a core handle; both
 * are passed in here as opaque pointers, and everything that needs the
 * resource layer, the geometry or the picture stays on this side. A scene is
 * built once and then drawn as often as the view is moved, so a drag only
 * re-rasterises: the models were already read.
 *
 * The C side has one rule: the pixels a draw returns belong to the scene and
 * stay valid only until the next draw on that scene (or its free). Qt copies
 * them into a `QImage` before the next frame, so that is not a burden.
 */

use std::ffi::{c_char, c_void};
use std::panic::{AssertUnwindSafe, catch_unwind};
use std::ptr;
use std::slice;

use common_rs::region::Region;
use crate::ffi::DhlrcCore;
use dhlrc_render::camera::{Hit, Ray};
use dhlrc_render::scene::{VoxelBlock, Voxels, build};
use dhlrc_render::texture::Alpha;
use dhlrc_render::{Camera, Renderer};
use crate::resource::Identifier;

/// A region's blocks, as the renderer wants to see them.
struct RegionVoxels<'a>(&'a Region);

impl Voxels for RegionVoxels<'_> {
    fn size(&self) -> [i32; 3] {
        let (x, y, z) = self.0.region_size;
        [x, y, z]
    }

    fn palette_len(&self) -> usize {
        self.0.palette_array.len()
    }

    fn palette(&self, index: usize) -> Option<VoxelBlock> {
        let entry = self.0.palette_array.get(index)?;
        let properties = entry
            .property
            .iter()
            .map(|(name, value)| format!("{name}={value}"))
            .collect::<Vec<_>>()
            .join(",");
        Some(VoxelBlock {
            name: Identifier::parse(&entry.id_name),
            properties,
        })
    }

    fn block(&self, x: i32, y: i32, z: i32) -> Option<usize> {
        let [sx, sy, sz] = self.size();
        if x < 0 || y < 0 || z < 0 || x >= sx || y >= sy || z >= sz {
            return None;
        }
        /* The region's own order, the one `region_get_index` uses. */
        let index = (sx * sz * y + sx * z + x) as usize;
        self.0.block_array.get(index).map(|id| *id as usize)
    }
}

/// The camera, as C passes it.
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct DhlrcCamera {
    pub target: [f64; 3],
    pub yaw: f64,
    pub pitch: f64,
    pub distance: f64,
    pub fov: f64,
    /// Any non-zero value uses the orthographic projection.
    pub orthographic: i32,
}

/// One quad of a scene, as a GPU wants it: four corners, their UVs, the colour
/// and light to multiply the texture by, and which texture that is.
///
/// The corners are in block space, wound so the front looks outwards, exactly
/// as the software rasteriser reads them, so a GPU drawing this draws the same
/// picture. Two triangles per quad: `0, 1, 2` and `0, 2, 3`.
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct DhlrcQuad {
    pub positions: [[f32; 3]; 4],
    pub uv: [[f32; 2]; 4],
    /// The colour the texture is multiplied by; `[1, 1, 1]` for no tint.
    pub tint: [f32; 3],
    /// Full light is 1.0, the bottom of a block is 0.5.
    pub light: f32,
    /// Which of the scene's textures to sample.
    pub texture: u32,
    /// How the texture's alpha behaves: 0 covers, 1 is cut out where it is
    /// clear, 2 is blended.
    pub alpha: i32,
}

/// What `DhlrcQuad::alpha` says.
const ALPHA_OPAQUE: i32 = 0;
const ALPHA_CUTOUT: i32 = 1;
const ALPHA_TRANSLUCENT: i32 = 2;

impl DhlrcCamera {
    fn to_camera(self) -> Camera {
        Camera {
            target: [
                self.target[0] as f32,                self.target[1] as f32,
                self.target[2] as f32,
            ],
            yaw: self.yaw as f32,
            pitch: self.pitch as f32,
            distance: self.distance as f32,
            fov: self.fov as f32,
            orthographic: self.orthographic != 0,
        }
    }
}

/// A built scene and the last picture drawn from it.
pub struct DhlrcScene {
    scene: dhlrc_render::Scene,
    /// The painter, kept so that drawing a frame after a mouse move reuses the
    /// colour and depth buffers instead of allocating a megabyte of them.
    renderer: Renderer,
    /// The skipped reasons as C strings, made once so the pointer a frontend
    /// gets back stays valid for as long as the scene does.
    skipped: Vec<std::ffi::CString>,
}

impl DhlrcScene {
    fn new(scene: dhlrc_render::Scene) -> Self {
        let skipped = scene
            .skipped
            .iter()
            .map(|reason| std::ffi::CString::new(reason.as_str()).unwrap_or_default())
            .collect();
        Self {
            scene,
            renderer: Renderer::new(),
            skipped,
        }
    }
}

/// Builds the scene of `region`, reading models and textures through `core`'s
/// configured resources. Returns null when either handle is null.
///
/// # Safety
///
/// `core` must be a `DhlrcCore *` that is still alive, and `region` a
/// `Region *` that is still alive; neither may be freed while the scene is in
/// use.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn dhlrc_render_scene_new(
    core: *mut c_void,
    region: *const c_void,
) -> *mut DhlrcScene {
    if core.is_null() || region.is_null() {
        return ptr::null_mut();
    }
    let core = unsafe { &*(core as *const DhlrcCore) };
    let region = unsafe { &*(region as *const Region) };

    catch_unwind(AssertUnwindSafe(|| {
        let scene = core
            .resources(|source| build(source, &RegionVoxels(region)))
            /* A core with no resource set at all still draws: every block then
             * asks for a texture the empty set does not have and comes out as
             * the missing-texture checker. */
            .unwrap_or_else(|| build(&Empty, &RegionVoxels(region)));
        Box::into_raw(Box::new(DhlrcScene::new(scene)))
    }))
    .unwrap_or(ptr::null_mut())
}

/// An empty resource set, for a core that could not provide one.
struct Empty;

impl crate::resource::Source for Empty {
    fn get(&self, _id: &Identifier) -> Option<Vec<u8>> {
        None
    }
}

/// Frees a scene built by [`dhlrc_render_scene_new`].
///
/// # Safety
///
/// `scene` must be a pointer this library returned, and must not be used after.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn dhlrc_render_scene_free(scene: *mut DhlrcScene) {
    if !scene.is_null() {
        drop(unsafe { Box::from_raw(scene) });
    }
}

/// How many quads the scene holds, or 0 for a null scene.
///
/// # Safety
///
/// `scene` must be a pointer this library returned.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn dhlrc_render_scene_quads(scene: *const DhlrcScene) -> usize {
    unsafe { scene.as_ref() }.map_or(0, |scene| scene.scene.quads.len())
}

/// How many palette entries were left out, for a frontend that wants to warn.
///
/// # Safety
///
/// `scene` must be a pointer this library returned.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn dhlrc_render_scene_skipped(scene: *const DhlrcScene) -> usize {
    unsafe { scene.as_ref() }.map_or(0, |scene| scene.scene.skipped.len())
}

/// One skipped palette entry's reason, borrowed from the scene.
///
/// # Safety
///
/// As [`dhlrc_render_scene_skipped`]; the pointer is valid while `scene` lives.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn dhlrc_render_scene_skipped_reason(
    scene: *const DhlrcScene,
    index: usize,
) -> *const c_char {
    let Some(scene) = (unsafe { scene.as_ref() }) else {
        return ptr::null();
    };
    scene
        .skipped
        .get(index)
        .map_or(ptr::null(), |reason| reason.as_ptr())
}

/// Points `camera` at the whole scene so it fits a `width` by `height`
/// picture. This is what a view calls when it first shows a region.
///
/// # Safety
///
/// `scene` must be a pointer this library returned; `camera` must be writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn dhlrc_render_fit(
    scene: *const DhlrcScene,
    camera: *mut DhlrcCamera,
    width: u32,
    height: u32,
) {
    let (Some(scene), Some(camera)) = (unsafe { scene.as_ref() }, unsafe { camera.as_mut() }) else {
        return;
    };
    let mut view = camera.to_camera();
    view.fit(
        scene.scene.min,
        scene.scene.max,
        width.max(1) as f32 / height.max(1) as f32,
    );
    camera.target = [
        view.target[0] as f64,
        view.target[1] as f64,
        view.target[2] as f64,
    ];
    camera.distance = view.distance as f64;
}

/// Slides the camera's target across the plane it looks at, by a fraction of
/// the picture's height: `right` positive goes towards the camera's right and
/// `up` positive towards the top. This is what a middle or right button drag
/// does in the Qt view.
///
/// # Safety
///
/// `camera` must be writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn dhlrc_render_pan(camera: *mut DhlrcCamera, right: f64, up: f64) {
    let Some(camera) = (unsafe { camera.as_mut() }) else {
        return;
    };
    let mut view = camera.to_camera();
    view.pan(right as f32, up as f32);
    camera.target = [
        view.target[0] as f64,
        view.target[1] as f64,
        view.target[2] as f64,
    ];
}

/// Draws the scene and returns the picture: `width * height * 4` RGBA bytes,
/// owned by the scene and valid until the next draw on it or its free. Null
/// when a handle is null or the size is empty.
///
/// # Safety
///
/// `scene` must be a pointer this library returned and `camera` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn dhlrc_render_image(
    scene: *mut DhlrcScene,
    camera: *const DhlrcCamera,
    width: u32,
    height: u32,
) -> *const u8 {
    let (Some(scene), Some(camera)) = (unsafe { scene.as_mut() }, unsafe { camera.as_ref() }) else {
        return ptr::null();
    };
    if width == 0 || height == 0 {
        return ptr::null();
    }
    let drawn = catch_unwind(AssertUnwindSafe(|| {
        scene
            .renderer
            .render(&scene.scene, &camera.to_camera(), width, height);
    }));
    match drawn {
        Ok(()) => scene.renderer.image().pixels.as_ptr(),
        Err(_) => ptr::null(),
    }
}

/// The block under pixel `(x, y)`, as a region coordinate in `block`, which
/// must have room for three `int32_t`. Returns 1 on a hit and 0 on a miss.
///
/// # Safety
///
/// `scene` must be a pointer this library returned, `camera` readable and
/// `block` writable for three `int32_t`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn dhlrc_render_pick(
    scene: *const DhlrcScene,
    camera: *const DhlrcCamera,
    width: u32,
    height: u32,
    x: f64,
    y: f64,
    block: *mut i32,
) -> i32 {
    let (Some(scene), Some(camera)) = (unsafe { scene.as_ref() }, unsafe { camera.as_ref() }) else {
        return 0;
    };
    if block.is_null() || width == 0 || height == 0 {
        return 0;
    }
    let view = camera.to_camera();
    let ray: Ray = view.ray(x as f32, y as f32, width, height);
    match scene.scene.pick(&ray) {
        Some(Hit {
            block: hit, ..
        }) => {
            unsafe { slice::from_raw_parts_mut(block, 3) }.copy_from_slice(&hit);
            1
        }
        None => 0,
    }
}

/// How many quads the scene holds, or 0 for a null scene. A GPU builds two
/// triangles for each.
///
/// # Safety
///
/// `scene` must be a pointer this library returned.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn dhlrc_render_scene_quad_count(scene: *const DhlrcScene) -> usize {
    unsafe { scene.as_ref() }.map_or(0, |scene| scene.scene.quads.len())
}

/// One quad of the scene, written to `quad`. Returns 1, or 0 for a null handle
/// or an index past the end.
///
/// # Safety
///
/// `scene` must be a pointer this library returned and `quad` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn dhlrc_render_scene_quad(
    scene: *const DhlrcScene,
    index: usize,
    quad: *mut DhlrcQuad,
) -> i32 {
    let (Some(scene), Some(out)) = (unsafe { scene.as_ref() }, unsafe { quad.as_mut() }) else {
        return 0;
    };
    let Some(source) = scene.scene.quads.get(index) else {
        return 0;
    };
    *out = DhlrcQuad {
        positions: source.positions,
        uv: source.uv,
        tint: source.tint.map_or([1.0; 3], |tint| {
            [
                tint[0] as f32 / 255.0,
                tint[1] as f32 / 255.0,
                tint[2] as f32 / 255.0,
            ]
        }),
        light: source.light,
        texture: source.texture as u32,
        alpha: match source.alpha {
            Alpha::Opaque => ALPHA_OPAQUE,
            Alpha::Cutout => ALPHA_CUTOUT,
            Alpha::Translucent => ALPHA_TRANSLUCENT,
        },
    };
    1
}

/// How many textures the scene holds. Index 0 is always the missing-texture
/// checker, so a quad's index is never out of range.
///
/// # Safety
///
/// `scene` must be a pointer this library returned.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn dhlrc_render_scene_texture_count(scene: *const DhlrcScene) -> usize {
    unsafe { scene.as_ref() }.map_or(0, |scene| scene.scene.textures.len())
}

/// One texture of the scene as `width * height * 4` RGBA bytes, top-down: what
/// `glTexImage2D` takes. The pixels are borrowed from the scene and stay valid
/// for as long as it does; every output pointer may be null when only some of
/// them are wanted.
///
/// # Safety
///
/// `scene` must be a pointer this library returned, and each non-null output
/// pointer must be writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn dhlrc_render_scene_texture(
    scene: *const DhlrcScene,
    index: usize,
    pixels: *mut *const u8,
    width: *mut u32,
    height: *mut u32,
) -> i32 {
    let Some(scene) = (unsafe { scene.as_ref() }) else {
        return 0;
    };
    let Some(texture) = scene.scene.textures.get_checked(index) else {
        return 0;
    };
    if !pixels.is_null() {
        unsafe { *pixels = texture.pixels().as_ptr().cast::<u8>() };
    }
    if !width.is_null() {
        unsafe { *width = texture.width };
    }
    if !height.is_null() {
        unsafe { *height = texture.height };
    }
    1
}

/// The view-projection matrix of `camera` for a `width` by `height` picture, in
/// the column-major layout a GPU wants, written to `out` as sixteen `float`s.
/// It is the very matrix the software rasteriser uses, so both draw the same
/// view.
///
/// # Safety
///
/// `scene` and `camera` must be pointers this library returned or was given,
/// and `out` writable for sixteen `float`s.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn dhlrc_render_matrix(
    scene: *const DhlrcScene,
    camera: *const DhlrcCamera,
    width: u32,
    height: u32,
    out: *mut f32,
) {
    let (Some(scene), Some(camera), Some(out)) = (
        unsafe { scene.as_ref() },
        unsafe { camera.as_ref() },
        unsafe { out.as_mut() },
    ) else {
        return;
    };
    let aspect = width.max(1) as f32 / height.max(1) as f32;
    let matrix = camera
        .to_camera()
        .view_projection(aspect, scene.scene.radius.max(0.5));
    unsafe { slice::from_raw_parts_mut(out, 16) }.copy_from_slice(&matrix.0);
}

#[cfg(test)]
mod tests {
    use super::*;
    use dhlrc_render::scene::SceneQuad;
    use dhlrc_render::texture::{Alpha, Texture, Textures};

    /// A scene with one tinted quad and one texture, standing in for a real one
    /// so the marshalling can be checked without a region and a core.
    fn scene() -> DhlrcScene {
        let mut textures = Textures::new();
        let index = textures.push(Texture::solid([10, 20, 30, 255]));
        DhlrcScene::new(dhlrc_render::Scene {
            quads: vec![SceneQuad {
                positions: [[0.0, 0.0, 0.0], [1.0, 0.0, 0.0], [1.0, 1.0, 0.0], [0.0, 1.0, 0.0]],
                uv: [[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]],
                texture: index,
                alpha: Alpha::Cutout,
                tint: Some([0, 128, 255]),
                shade: true,
                light: 0.8,
            }],
            textures,
            center: [0.5, 0.5, 0.0],
            radius: 1.0,
            min: [0.0; 3],
            max: [1.0; 3],
            skipped: Vec::new(),
        })
    }

    #[test]
    fn a_scene_hands_its_quads_and_textures_to_a_gpu() {
        let scene = scene();
        let pointer = &scene as *const DhlrcScene;

        assert_eq!(unsafe { dhlrc_render_scene_quad_count(pointer) }, 1);
        assert_eq!(unsafe { dhlrc_render_scene_texture_count(pointer) }, 2);

        let mut quad = DhlrcQuad {
            positions: [[0.0; 3]; 4],
            uv: [[0.0; 2]; 4],
            tint: [0.0; 3],
            light: 0.0,
            texture: 0,
            alpha: -1,
        };
        assert_eq!(unsafe { dhlrc_render_scene_quad(pointer, 0, &mut quad) }, 1);
        assert_eq!(quad.texture, 1);
        assert_eq!(quad.alpha, ALPHA_CUTOUT);
        assert!((quad.light - 0.8).abs() < 1e-6);
        assert!((quad.tint[1] - 128.0 / 255.0).abs() < 1e-6);
        assert_eq!(quad.positions[2], [1.0, 1.0, 0.0]);
        /* Past the end is a miss, not a crash. */
        assert_eq!(unsafe { dhlrc_render_scene_quad(pointer, 9, &mut quad) }, 0);

        let mut pixels: *const u8 = ptr::null();
        let (mut width, mut height) = (0u32, 0u32);
        assert_eq!(
            unsafe { dhlrc_render_scene_texture(pointer, 1, &mut pixels, &mut width, &mut height) },
            1
        );
        assert_eq!((width, height), (1, 1));
        assert_eq!(unsafe { *pixels }, 10, "red first");
        assert_eq!(unsafe { dhlrc_render_scene_texture(pointer, 9, &mut pixels, &mut width, &mut height) }, 0);
    }

    #[test]
    fn the_matrix_matches_what_the_rasteriser_uses() {
        let scene = scene();
        let camera = DhlrcCamera {
            target: [0.5, 0.5, 0.0],
            yaw: 0.0,
            pitch: 0.0,
            distance: 4.0,
            fov: 45.0,
            orthographic: 0,
        };
        let mut matrix = [0.0f32; 16];
        unsafe { dhlrc_render_matrix(&scene, &camera, 640, 480, matrix.as_mut_ptr()) };
        let expected = camera
            .to_camera()
            .view_projection(640.0 / 480.0, scene.scene.radius.max(0.5));
        assert_eq!(matrix, expected.0);
    }
}
