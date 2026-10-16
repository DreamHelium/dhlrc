/*! Drawing Minecraft's blocks.
 *
 * [`dhlrc_model`] turns a block into geometry — a handful of quads with a
 * texture id per face — and this crate is what draws them: it decodes the
 * textures, settles the look of each face (its tint, its shading, what its
 * alpha does), and fills an [`Image`] with a depth test so a whole region
 * comes out right whatever order its quads arrive in.
 *
 * It is a software renderer on purpose. A rendered picture then comes out the
 * same on a machine with no GPU, in a terminal and in a test, and the Qt view
 * and the terminal's `render` command draw with the same code — which is what
 * keeps them from disagreeing about what a block looks like. It lives outside
 * `dhlrc-model` so the geometry stays usable by a frontend that never draws.
 *
 * ```no_run
 * use dhlrc_render::{Camera, build_scene};
 * # let source = dhlrc_resource::ResourceSet::new();
 * # struct Nothing;
 * # impl dhlrc_render::Voxels for Nothing {
 * #     fn size(&self) -> [i32; 3] { [0; 3] }
 * #     fn palette_len(&self) -> usize { 0 }
 * #     fn palette(&self, _: usize) -> Option<dhlrc_render::VoxelBlock> { None }
 * #     fn block(&self, _: i32, _: i32, _: i32) -> Option<usize> { None }
 * # }
 * let scene = build_scene(&source, &Nothing);
 * let camera = Camera::new(scene.center, scene.radius * 2.0);
 * let image = dhlrc_render::render(&scene, &camera, 640, 480);
 * ```
 */

pub mod camera;
pub mod image;
pub mod raster;
pub mod scene;
pub mod texture;
pub mod tint;

pub use camera::{Camera, Mat4, Vec3};
pub use image::Image;
pub use raster::{Renderer, render};
pub use scene::{Highlight, Scene, SceneQuad, VoxelBlock, Voxels, build as build_scene};
pub use texture::{Alpha, Texture, Textures, texture_asset};
pub use tint::{Colormaps, Tint};

use std::fmt::{self, Display, Formatter};

use dhlrc_resource::Identifier;

/// Why a texture could not be read. A block model that cannot be resolved does
/// not stop a render: the block is left out and reported on [`Scene::skipped`].
#[derive(Debug)]
pub enum Error {
    /// The resource source has no such file.
    Missing(Identifier),
    /// The file is not the PNG it should be.
    Png(String),
}

impl Display for Error {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Error::Missing(id) => write!(formatter, "no resource for {id}"),
            Error::Png(message) => write!(formatter, "not a PNG: {message}"),
        }
    }
}

impl std::error::Error for Error {}
