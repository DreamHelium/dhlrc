/*! Turning a region of blocks into the quads a renderer draws.
 *
 * A region is a palette and an array of palette indices. Each palette entry is
 * resolved to a model once (a region uses a few hundred entries at most, and
 * resolving one reads several JSON files), and the model's quads are then
 * placed at every position that uses it.
 *
 * Two things are settled while placing a quad, because they need the
 * neighbours or the texture rather than the geometry:
 *
 * - a face whose `cullface` direction holds a block that hides it (a solid,
 *   fully opaque cube) is dropped, which is most of the faces of a filled
 *   region, and
 * - a face's `tintindex` becomes the colour it is multiplied by.
 */

use dhlrc_resource::{Identifier, Source};
use dhlrc_model::{BlockModel, Direction, Quad, quads, resolve_parts};

use crate::camera::{Hit, Ray, Vec3, cross, dot, sub};
use crate::texture::{Alpha, Textures};
use crate::tint::{Colormaps, Tint};

/// A block a palette entry names.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct VoxelBlock {
    pub name: Identifier,
    /// The block's properties as `facing=north,half=top`, or empty.
    pub properties: String,
}

/// The blocks a frontend hands the renderer.
///
/// This is the whole seam between a region (whose layout belongs to a plugin)
/// and the renderer, so the Qt frontend, the terminal frontend and a test can
/// all feed it without agreeing on anything else.
pub trait Voxels {
    /// The region's size in blocks.
    fn size(&self) -> [i32; 3];

    /// How many palette entries the region has.
    fn palette_len(&self) -> usize;

    /// What palette entry `index` names, or `None` for an entry that draws
    /// nothing.
    fn palette(&self, index: usize) -> Option<VoxelBlock>;

    /// The palette entry at `(x, y, z)`, or `None` for air and for a position
    /// outside the region.
    fn block(&self, x: i32, y: i32, z: i32) -> Option<usize>;
}

/// One quad, placed in the world and ready to draw.
#[derive(Debug, Clone)]
pub struct SceneQuad {
    /// The corners in block space, wound so the front looks outwards.
    pub positions: [Vec3; 4],
    pub uv: [[f32; 2]; 4],
    /// An index into [`Scene::textures`].
    pub texture: usize,
    pub alpha: Alpha,
    /// The colour the texture is multiplied by, or `None`.
    pub tint: Option<[u8; 3]>,
    /// Whether the face was shaded by the direction it looks.
    pub shade: bool,
    /// The light the face is drawn with: `shade`'s factor for the direction it
    /// looks, or 1 when it does not ask to be shaded. Baked here so that the
    /// software rasteriser and a GPU drawing the same scene agree by
    /// construction rather than by two copies of the rule.
    pub light: f32,
}

/// Everything one render needs.
pub struct Scene {
    pub quads: Vec<SceneQuad>,
    pub textures: Textures,
    /// The middle of the scene, in block space.
    pub center: Vec3,
    /// How far the scene reaches from its middle.
    pub radius: f32,
    /// The two corners of the box the blocks are in, for framing the view.
    pub min: Vec3,
    pub max: Vec3,
    /// Blocks that could not be resolved, each once, for a frontend to report.
    pub skipped: Vec<String>,
}

impl Scene {
    /// A scene with nothing in it, for a caller that has no region yet.
    pub fn empty() -> Self {
        Self {
            quads: Vec::new(),
            textures: Textures::new(),
            center: [0.0; 3],
            radius: 1.0,
            min: [0.0; 3],
            max: [1.0; 3],
            skipped: Vec::new(),
        }
    }

    /// How many textures the scene ended up holding.
    pub fn texture_count(&self) -> usize {
        self.textures.len()
    }

    /// The block a ray hits first, front faces only — the same faces the
    /// renderer draws. This is what a click in the Qt view turns into a block.
    pub fn pick(&self, ray: &Ray) -> Option<Hit> {
        let mut best: Option<Hit> = None;
        for quad in &self.quads {
            let normal = crate::camera::normalize(
                cross(
                    sub(quad.positions[1], quad.positions[0]),
                    sub(quad.positions[3], quad.positions[0]),
                ),
                [0.0, 1.0, 0.0],
            );
            /* A face whose back is towards the ray is one the block hides. */
            if dot(ray.direction, normal) >= 0.0 {
                continue;
            }
            for [i, j, k] in [[0, 1, 2], [0, 2, 3]] {
                let Some(distance) = ray_triangle(
                    ray,
                    quad.positions[i],
                    quad.positions[j],
                    quad.positions[k],
                ) else {
                    continue;
                };
                if best.is_some_and(|hit| hit.distance <= distance) {
                    continue;
                }
                let point = [
                    ray.origin[0] + ray.direction[0] * distance,
                    ray.origin[1] + ray.direction[1] * distance,
                    ray.origin[2] + ray.direction[2] * distance,
                ];
                /* Step off the surface into the block it belongs to, so a hit
                 * exactly on a boundary lands on this block and not the next. */
                let inside = [
                    point[0] - normal[0] * 1e-3,
                    point[1] - normal[1] * 1e-3,
                    point[2] - normal[2] * 1e-3,
                ];
                best = Some(Hit {
                    block: [
                        inside[0].floor() as i32,
                        inside[1].floor() as i32,
                        inside[2].floor() as i32,
                    ],
                    distance,
                    normal,
                });
            }
        }
        best
    }
}

/// One of a model's quads with its texture already decoded.
struct Piece {
    positions: [Vec3; 4],
    uv: [[f32; 2]; 4],
    texture: usize,
    alpha: Alpha,
    tint: Option<[u8; 3]>,
    cullface: Option<Direction>,
    shade: bool,
    light: f32,
}

/// One palette entry, resolved once.
struct Entry {
    pieces: Vec<Piece>,
    /// Whether the block hides the face of the block beside it.
    occludes: bool,
}

/// Builds the scene of `voxels`, reading models and textures through `source`.
pub fn build(source: &dyn Source, voxels: &dyn Voxels) -> Scene {
    let mut textures = Textures::new();
    let colormaps = Colormaps::load(source);
    let mut skipped = Vec::new();

    /* Resolve each palette entry once; a region draws the same entry at
     * thousands of positions. */
    let mut entries: Vec<Option<Entry>> = Vec::with_capacity(voxels.palette_len());
    for index in 0..voxels.palette_len() {
        let Some(block) = voxels.palette(index) else {
            entries.push(None);
            continue;
        };
        /* Air is an ordinary palette entry in a region, and resolving it would
         * only report a blockstate that is not there. */
        if is_empty(&block.name) {
            entries.push(None);
            continue;
        }
        match resolve_parts(source, &block.name, &block.properties) {
            Ok(parts) => entries.push(Some(resolve_entry(
                &parts,
                &block,
                source,
                &colormaps,
                &mut textures,
            ))),
            /* A block whose every state needs properties draws nothing in the
             * state it was asked about; it is not a failure of the region. */
            Err(dhlrc_model::Error::NoDefaultModel) => entries.push(None),
            Err(error) => {
                skipped.push(format!("{}: {error}", block.name));
                entries.push(None);
            }
        }
    }

    let size = voxels.size();
    let mut scene = Scene {
        quads: Vec::new(),
        textures,
        center: [size[0] as f32 / 2.0, size[1] as f32 / 2.0, size[2] as f32 / 2.0],
        radius: 0.5
            * ((size[0] * size[0] + size[1] * size[1] + size[2] * size[2]) as f32).sqrt(),
        min: [0.0; 3],
        max: [
            size[0] as f32,
            size[1] as f32,
            size[2] as f32,
        ],
        skipped,
    };

    for y in 0..size[1] {
        for z in 0..size[2] {
            for x in 0..size[0] {
                let Some(index) = voxels.block(x, y, z) else {
                    continue;
                };
                let Some(entry) = entries.get(index).and_then(Option::as_ref) else {
                    continue;
                };
                for piece in &entry.pieces {
                    if let Some(direction) = piece.cullface {
                        if hidden(voxels, &entries, x, y, z, direction) {
                            continue;
                        }
                    }
                    scene.quads.push(place(piece, x, y, z));
                }
            }
        }
    }

    scene
}

/// Whether a block is one of the kinds of air, which a region stores like any
/// other palette entry but which draws nothing and hides nothing.
fn is_empty(name: &Identifier) -> bool {
    name.namespace == "minecraft"
        && matches!(
            name.path.as_str(),
            "air" | "cave_air" | "void_air" | "structure_void"
        )
}

/// Whether the block on the `direction` side of `(x, y, z)` hides the face.
fn hidden(
    voxels: &dyn Voxels,
    entries: &[Option<Entry>],
    x: i32,
    y: i32,
    z: i32,
    direction: Direction,
) -> bool {
    let [dx, dy, dz] = direction.offset();
    let neighbour = [x + dx as i32, y + dy as i32, z + dz as i32];
    let Some(index) = voxels.block(neighbour[0], neighbour[1], neighbour[2]) else {
        return false;
    };
    entries
        .get(index)
        .and_then(Option::as_ref)
        .is_some_and(|entry| entry.occludes)
}

/// The quads of every model a state draws, with their textures decoded and
/// their tints resolved.
fn resolve_entry(
    models: &[BlockModel],
    block: &VoxelBlock,
    source: &dyn Source,
    colormaps: &Colormaps,
    textures: &mut Textures,
) -> Entry {
    let mut pieces = Vec::new();
    for model in models {
        for quad in quads(model) {
            let Quad {
                positions,
                uv,
                texture,
                tint,
                cullface,
                shade,
            } = quad;

            let index = match &texture {
                Some(id) => textures.load(source, id),
                None => textures.missing(),
            };
            let alpha = textures.get(index).alpha();
            let tint = tint
                .and_then(|index| Tint::of(&block.name, index))
                .map(|tint| colormaps.color(tint));

            pieces.push(Piece {
                positions,
                uv,
                texture: index,
                alpha,
                tint,
                cullface,
                shade,
                light: face_light(&positions, shade),
            });
        }
    }

    /* A block hides its neighbour only when it is one opaque cube: a single
     * model, that model the full box, and nothing in it see-through. A fence
     * or a stair is several pieces and hides nothing. */
    let full_cube = models.len() == 1
        && models[0].model.elements.len() == 1
        && models[0].model.elements[0].from == [0.0; 3]
        && models[0].model.elements[0].to == [16.0; 3]
        && models[0].model.elements[0].faces.len() == 6;
    let occludes = full_cube && pieces.iter().all(|piece| piece.alpha == Alpha::Opaque);

    Entry { pieces, occludes }
}

/// Moves a piece to `(x, y, z)` in block space.
fn place(piece: &Piece, x: i32, y: i32, z: i32) -> SceneQuad {
    let offset = [x as f32, y as f32, z as f32];
    SceneQuad {
        positions: piece.positions.map(|point| {
            [
                point[0] + offset[0],
                point[1] + offset[1],
                point[2] + offset[2],
            ]
        }),
        uv: piece.uv,
        texture: piece.texture,
        alpha: piece.alpha,
        tint: piece.tint,
        shade: piece.shade,
        light: piece.light,
    }
}

/// How much a face is darkened by the direction it looks, as the game does it:
/// the top in full light, the bottom at half, and the four sides in between.
/// A face that asked not to be shaded keeps full light.
fn face_light(positions: &[Vec3; 4], shade: bool) -> f32 {
    if !shade {
        return 1.0;
    }
    let normal = crate::camera::normalize(
        cross(
            sub(positions[1], positions[0]),
            sub(positions[3], positions[0]),
        ),
        [0.0, 1.0, 0.0],
    );
    let [x, y, z] = normal.map(f32::abs);
    if y >= x && y >= z {
        if normal[1] > 0.0 { 1.0 } else { 0.5 }
    } else if x >= z {
        0.6
    } else {
        0.8
    }
}

/// Where a ray meets the triangle `a`, `b`, `c`, by the Möller–Trumbore
/// test, or `None` when it misses or meets it behind the origin.
fn ray_triangle(ray: &Ray, a: Vec3, b: Vec3, c: Vec3) -> Option<f32> {
    let edge1 = sub(b, a);
    let edge2 = sub(c, a);
    let across = cross(ray.direction, edge2);
    let determinant = dot(edge1, across);
    if determinant.abs() < 1e-9 {
        return None;
    }
    let inverse = 1.0 / determinant;
    let to_a = sub(ray.origin, a);
    let u = dot(to_a, across) * inverse;
    if !(0.0..=1.0).contains(&u) {
        return None;
    }
    let along = cross(to_a, edge1);
    let v = dot(ray.direction, along) * inverse;
    if v < 0.0 || u + v > 1.0 {
        return None;
    }
    let distance = dot(edge2, along) * inverse;
    (distance > 1e-4).then_some(distance)
}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;

    use dhlrc_resource::{Identifier, Source};

    use super::{VoxelBlock, Voxels, build};

    /// A source holding the named files.
    #[derive(Default)]
    struct Files(BTreeMap<String, Vec<u8>>);

    impl Files {
        fn with(mut self, id: &str, bytes: impl Into<Vec<u8>>) -> Self {
            self.0.insert(id.to_owned(), bytes.into());
            self
        }
    }

    impl Source for Files {
        fn get(&self, id: &Identifier) -> Option<Vec<u8>> {
            self.0.get(&id.to_string()).cloned()
        }
    }

    /// A 1 by 1 opaque PNG.
    fn png() -> Vec<u8> {
        let mut bytes = Vec::new();
        {
            let mut encoder = png::Encoder::new(&mut bytes, 1, 1);
            encoder.set_color(png::ColorType::Rgba);
            encoder.set_depth(png::BitDepth::Eight);
            let mut writer = encoder.write_header().unwrap();
            writer.write_image_data(&[255, 255, 255, 255]).unwrap();
        }
        bytes
    }

    /// A source describing one full opaque cube, `minecraft:test`.
    fn cube_source() -> Files {
        Files::default()
            .with(
                "minecraft:blockstates/test.json",
                br#"{"variants":{"":{"model":"minecraft:block/test"}}}"#.to_vec(),
            )
            .with(
                "minecraft:models/block/test.json",
                br##"{"textures":{"all":"minecraft:block/tex"},"elements":[{"from":[0,0,0],"to":[16,16,16],
                    "faces":{"down":{"texture":"#all","cullface":"down"},"up":{"texture":"#all","cullface":"up"},
                    "north":{"texture":"#all","cullface":"north"},"south":{"texture":"#all","cullface":"south"},
                    "west":{"texture":"#all","cullface":"west"},"east":{"texture":"#all","cullface":"east"}}}]}"##
                    .to_vec(),
            )
            .with("minecraft:textures/block/tex.png", png())
    }

    /// A region that is solid `minecraft:test` everywhere.
    struct Solid {
        size: [i32; 3],
    }

    impl Voxels for Solid {
        fn size(&self) -> [i32; 3] {
            self.size
        }

        fn palette_len(&self) -> usize {
            1
        }

        fn palette(&self, index: usize) -> Option<VoxelBlock> {
            (index == 0).then(|| VoxelBlock {
                name: Identifier::parse("minecraft:test"),
                properties: String::new(),
            })
        }

        fn block(&self, x: i32, y: i32, z: i32) -> Option<usize> {
            let inside = (0..self.size[0]).contains(&x)
                && (0..self.size[1]).contains(&y)
                && (0..self.size[2]).contains(&z);
            inside.then_some(0)
        }
    }

    #[test]
    fn a_lone_block_draws_all_six_faces() {
        let scene = build(&cube_source(), &Solid { size: [1, 1, 1] });
        assert_eq!(scene.quads.len(), 6);
        assert_eq!(scene.texture_count(), 2, "the checker and the block's texture");
        assert!(scene.skipped.is_empty());
    }

    #[test]
    fn a_face_between_two_solid_blocks_is_dropped() {
        let scene = build(&cube_source(), &Solid { size: [2, 1, 1] });
        /* Twelve faces, less the two that meet in the middle. */
        assert_eq!(scene.quads.len(), 10);
    }

    #[test]
    fn a_block_with_no_model_is_left_out() {
        /* An empty `variants` is the `multipart` case: nothing to draw, but
         * nothing wrong with the region either. */
        let source = cube_source().with("minecraft:blockstates/test.json", br#"{"variants":{}}"#.to_vec());
        let scene = build(&source, &Solid { size: [1, 1, 1] });
        assert!(scene.quads.is_empty());
        assert!(scene.skipped.is_empty());
    }

    #[test]
    fn a_missing_blockstate_is_reported_once() {
        let source = Files::default().with("minecraft:textures/block/tex.png", png());
        let scene = build(&source, &Solid { size: [2, 1, 1] });
        assert!(scene.quads.is_empty());
        assert_eq!(scene.skipped.len(), 1);
        assert!(scene.skipped[0].contains("minecraft:test"), "{:?}", scene.skipped);
    }

    /// One block whose properties are whatever the test asks for.
    struct Fence {
        properties: &'static str,
    }

    impl Voxels for Fence {
        fn size(&self) -> [i32; 3] {
            [1, 1, 1]
        }

        fn palette_len(&self) -> usize {
            1
        }

        fn palette(&self, index: usize) -> Option<VoxelBlock> {
            (index == 0).then(|| VoxelBlock {
                name: Identifier::parse("minecraft:fence"),
                properties: self.properties.to_owned(),
            })
        }

        fn block(&self, x: i32, y: i32, z: i32) -> Option<usize> {
            (x == 0 && y == 0 && z == 0).then_some(0)
        }
    }

    #[test]
    fn a_multipart_block_draws_the_pieces_that_match() {
        let source = Files::default()
            .with(
                "minecraft:blockstates/fence.json",
                br#"{"multipart":[
                    {"apply":{"model":"minecraft:block/post"}},
                    {"when":{"north":"true"},"apply":{"model":"minecraft:block/side"}}]}"#
                    .to_vec(),
            )
            .with(
                "minecraft:models/block/post.json",
                br##"{"textures":{"all":"minecraft:block/tex"},"elements":[{"from":[6,0,6],"to":[10,16,10],
                    "faces":{"up":{"texture":"#all"},"north":{"texture":"#all"}}}]}"##
                    .to_vec(),
            )
            .with(
                "minecraft:models/block/side.json",
                br##"{"textures":{"all":"minecraft:block/tex"},"elements":[{"from":[7,6,0],"to":[9,9,6],
                    "faces":{"up":{"texture":"#all"}}}]}"##
                    .to_vec(),
            )
            .with("minecraft:textures/block/tex.png", png());

        /* The post is always drawn; the side only when a neighbour is there,
         * which the block's own properties say. */
        let alone = build(&source, &Fence { properties: "north=false" });
        assert_eq!(alone.quads.len(), 2);
        let joined = build(&source, &Fence { properties: "north=true" });
        assert_eq!(joined.quads.len(), 3);
    }

    #[test]
    fn a_ray_lands_on_the_block_it_meets() {
        use crate::camera::Camera;

        let scene = build(&cube_source(), &Solid { size: [1, 1, 1] });
        let mut camera = Camera::new([0.5, 0.5, 0.5], 4.0);
        camera.yaw = 0.0;
        camera.pitch = 0.0;

        let hit = scene
            .pick(&camera.ray(16.0, 16.0, 32, 32))
            .expect("the cube is in the middle of the picture");
        assert_eq!(hit.block, [0, 0, 0]);
        assert!(hit.normal[2] > 0.9, "the face towards the camera: {:?}", hit.normal);

        assert!(scene.pick(&camera.ray(1.0, 1.0, 32, 32)).is_none());
    }

    #[test]
    fn a_ray_through_two_blocks_lands_on_the_nearer_one() {
        use crate::camera::Ray;

        let scene = build(&cube_source(), &Solid { size: [2, 1, 1] });
        /* Looking down the row from beyond its far end. */
        let ray = Ray {
            origin: [-2.0, 0.5, 0.5],
            direction: [1.0, 0.0, 0.0],
        };
        let hit = scene.pick(&ray).unwrap();
        assert_eq!(hit.block, [0, 0, 0]);
        assert!(hit.normal[0] < -0.9, "{:?}", hit.normal);
    }
}
