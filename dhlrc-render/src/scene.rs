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
use dhlrc_model::{BlockModel, Direction, Quad, Variant, quads, resolve_model, resolve_parts};

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

/// The block a view is pointing at, as the box to draw around it.
///
/// The two corners are in block space, the same space the quads are in, so a
/// highlight is a box around one block: `[x, y, z]` to `[x + 1, y + 1, z + 1]`.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Highlight {
    pub min: Vec3,
    pub max: Vec3,
}

/// How far outside the block the outline is drawn. Without it the lines lie
/// exactly on the faces they trace and fight with them for the depth buffer.
const HIGHLIGHT_OFFSET: f32 = 0.002;

/// How thick the outline's lines are, in blocks: a block is 1 across, so this
/// is a thirty-second of it — about a pixel in a view of a few dozen blocks.
const HIGHLIGHT_THICKNESS: f32 = 1.0 / 32.0;

/// The twelve edges of `highlight` as thin boxes, one quad per face.
///
/// They fit the scene's own quads so that both renderers draw the same outline
/// without either learning a new kind of geometry: the texture is never seen
/// because the tint is black, which leaves the texel black wherever it is
/// sampled.
pub fn outline(highlight: Highlight) -> Vec<SceneQuad> {
    let min = highlight.min.map(|value| value - HIGHLIGHT_OFFSET);
    let max = highlight.max.map(|value| value + HIGHLIGHT_OFFSET);
    let thickness = HIGHLIGHT_THICKNESS;
    let mut quads = Vec::with_capacity(12 * 6);

    for axis in 0..3 {
        /* An edge runs along one axis; the other two give the four corners it
         * joins, and each of those becomes a thin box. */
        let (along, across) = match axis {
            0 => (0, [1, 2]),
            1 => (1, [0, 2]),
            _ => (2, [0, 1]),
        };
        for first in [false, true] {
            for second in [false, true] {
                let mut start = [0.0f32; 3];
                let mut end = [0.0f32; 3];
                start[along] = min[along];
                end[along] = max[along];
                for (index, other) in across.iter().enumerate() {
                    let high = if index == 0 { first } else { second };
                    let value = if high { max[*other] } else { min[*other] };
                    start[*other] = value;
                    end[*other] = value;
                }
                /*
                 * The bar is the run along the edge, thickened sideways on both
                 * of the axes it does not run along.
                 */
                let mut low = start;
                let mut high = end;
                for other in across {
                    low[other] = start[other] - thickness;
                    high[other] = start[other] + thickness;
                }
                boxed(low, high, &mut quads);
            }
        }
    }
    quads
}

/// Pushes the six faces of a box as quads, all of them wound outwards.
fn boxed(low: Vec3, high: Vec3, out: &mut Vec<SceneQuad>) {
    for direction in Direction::ALL {
        out.push(SceneQuad {
            positions: face_corners(direction, low, high),
            /* The texel is never seen: this texture is the missing-texture
             * checker every scene carries at index 0, and the black tint below
             * makes whichever texel is sampled come out black. */
            uv: [[0.0, 0.0]; 4],
            texture: 0,
            alpha: Alpha::Opaque,
            tint: Some([0, 0, 0]),
            shade: false,
            light: 1.0,
        });
    }
}

/// The four corners of one face of a box, written so that the front looks along
/// `direction` — the winding the rasteriser takes for a front face.
fn face_corners(direction: Direction, low: Vec3, high: Vec3) -> [Vec3; 4] {
    let (lx, ly, lz) = (low[0], low[1], low[2]);
    let (hx, hy, hz) = (high[0], high[1], high[2]);
    match direction {
        Direction::North => [
            [hx, ly, lz],
            [lx, ly, lz],
            [lx, hy, lz],
            [hx, hy, lz],
        ],
        Direction::South => [
            [lx, ly, hz],
            [hx, ly, hz],
            [hx, hy, hz],
            [lx, hy, hz],
        ],
        Direction::West => [
            [lx, ly, lz],
            [lx, ly, hz],
            [lx, hy, hz],
            [lx, hy, lz],
        ],
        Direction::East => [
            [hx, ly, hz],
            [hx, ly, lz],
            [hx, hy, lz],
            [hx, hy, hz],
        ],
        Direction::Down => [
            [lx, ly, hz],
            [lx, ly, lz],
            [hx, ly, lz],
            [hx, ly, hz],
        ],
        Direction::Up => [
            [lx, hy, lz],
            [lx, hy, hz],
            [hx, hy, hz],
            [hx, hy, lz],
        ],
    }
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
    /// The outline of the block a view points at, drawn over the scene and
    /// after it. Empty when nothing is highlighted.
    pub highlight: Vec<SceneQuad>,
}

impl Scene {
    /// The box the block at `cell` is actually drawn in.
    ///
    /// A block is not always a whole cell: a slab is half of one, a pane a
    /// sliver of one. The outline reads better when it follows what is there,
    /// so this takes the quads that lie within the cell and returns the box
    /// around them — `None` when the cell drew nothing, which is a block that
    /// was culled away or is not there at all.
    pub fn block_box(&self, cell: [i32; 3]) -> Option<Highlight> {
        let mut min = [f32::INFINITY; 3];
        let mut max = [f32::NEG_INFINITY; 3];
        let mut found = false;
        let low = [cell[0] as f32, cell[1] as f32, cell[2] as f32];
        let high = [low[0] + 1.0, low[1] + 1.0, low[2] + 1.0];
        for quad in &self.quads {
            if !quad.positions.iter().all(|corner| {
                (0..3).all(|axis| {
                    corner[axis] >= low[axis] - 1e-3 && corner[axis] <= high[axis] + 1e-3
                })
            }) {
                continue;
            }
            found = true;
            for corner in quad.positions {
                for axis in 0..3 {
                    min[axis] = min[axis].min(corner[axis]);
                    max[axis] = max[axis].max(corner[axis]);
                }
            }
        }
        found.then_some(Highlight { min, max })
    }

    /// Points the view at a block, or at nothing, by building — or dropping —
    /// the outline that block is drawn with.
    ///
    /// The outline is part of the scene rather than something a renderer adds,
    /// so the software rasteriser and a GPU read it from the same place and
    /// draw the same box.
    pub fn set_highlight(&mut self, highlight: Option<Highlight>) {
        self.highlight = match highlight {
            Some(highlight) => outline(highlight),
            None => Vec::new(),
        };
    }
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
            highlight: Vec::new(),
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
    build_reported(source, voxels, &mut |_| {})
}

/// `build`, reporting how far it is in percent: 0 when it starts, then once per
/// vertical slice, and 100 when the scene is ready. A caller that draws a
/// progress bar raises it as the number grows.
pub fn build_reported(
    source: &dyn Source,
    voxels: &dyn Voxels,
    report: &mut dyn FnMut(i32),
) -> Scene {
    report(0);
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
        highlight: Vec::new(),
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
        report(((y + 1) * 100 / size[1].max(1)) as i32);
    }
    report(100);

    scene
}

/// Builds the scene of a single model, drawn as one block at the origin.
///
/// This is how an item icon is made: a block item names its model directly
/// (`minecraft:block/oak_stairs`), and the game draws that model rather than a
/// block state, so a stair looks like a stair and a grass block is not the snowy
/// variant. `block` is the name the model belongs to, used for the colour
/// colormaps (grass and leaves). A model with no geometry (an item sprite, whose
/// parent is `item/generated`) yields a scene with no quads.
pub fn build_model(source: &dyn Source, model: &Identifier, block: &Identifier) -> Scene {
    let mut textures = Textures::new();
    let colormaps = Colormaps::load(source);
    let Ok(resolved) = resolve_model(source, model) else {
        return Scene::empty();
    };
    let block_model = BlockModel {
        variant: Variant {
            model: model.to_string(),
            ..Variant::default()
        },
        model: resolved,
    };
    let voxel = VoxelBlock {
        name: block.clone(),
        properties: String::new(),
    };
    let entry = resolve_entry(&[block_model], &voxel, source, &colormaps, &mut textures);
    let mut quads = Vec::new();
    for piece in &entry.pieces {
        quads.push(place(piece, 0, 0, 0));
    }

    /* A model does not always fill its cell: a torch is thin, a slab is half.
     * The bounds come from the geometry so the camera frames what is drawn. */
    let mut scene = Scene {
        quads,
        textures,
        center: [0.5; 3],
        radius: 0.866,
        min: [0.0; 3],
        max: [1.0; 3],
        skipped: Vec::new(),
        highlight: Vec::new(),
    };
    if !scene.quads.is_empty() {
        let mut min = [f32::INFINITY; 3];
        let mut max = [f32::NEG_INFINITY; 3];
        for quad in &scene.quads {
            for corner in quad.positions {
                for axis in 0..3 {
                    min[axis] = min[axis].min(corner[axis]);
                    max[axis] = max[axis].max(corner[axis]);
                }
            }
        }
        scene.min = min;
        scene.max = max;
        scene.center = [
            (min[0] + max[0]) / 2.0,
            (min[1] + max[1]) / 2.0,
            (min[2] + max[2]) / 2.0,
        ];
        let extent = [max[0] - min[0], max[1] - min[1], max[2] - min[2]];
        scene.radius = (0.5 * (extent[0] * extent[0]
            + extent[1] * extent[1]
            + extent[2] * extent[2])
            .sqrt())
        .max(0.25);
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
    fn a_scene_build_reports_its_progress() {
        let mut values = Vec::new();
        let scene = super::build_reported(
            &cube_source(),
            &Solid { size: [4, 4, 4] },
            &mut |value| values.push(value),
        );
        assert!(!scene.quads.is_empty());
        assert_eq!(values.first(), Some(&0), "the build starts at 0");
        assert_eq!(values.last(), Some(&100), "and ends at 100");
        assert!(
            values.windows(2).all(|pair| pair[0] <= pair[1]),
            "progress never goes backwards: {values:?}"
        );
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

    /// The normal of a quad, from its own winding.
    fn quad_normal(quad: &super::SceneQuad) -> [f32; 3] {
        let [a, b, c, _] = quad.positions;
        let u = [b[0] - a[0], b[1] - a[1], b[2] - a[2]];
        let v = [c[0] - a[0], c[1] - a[1], c[2] - a[2]];
        let cross = [
            u[1] * v[2] - u[2] * v[1],
            u[2] * v[0] - u[0] * v[2],
            u[0] * v[1] - u[1] * v[0],
        ];
        let length = (cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]).sqrt();
        [cross[0] / length, cross[1] / length, cross[2] / length]
    }

    #[test]
    fn a_highlight_is_twelve_edges_of_thin_boxes() {
        let highlight = super::Highlight {
            min: [2.0, 1.0, 3.0],
            max: [3.0, 2.0, 4.0],
        };
        let outline = super::outline(highlight);
        /* Twelve edges, and each is a box of six faces. */
        assert_eq!(outline.len(), 12 * 6);

        /* Every face is wound outwards, so nothing of the outline is invisible
         * from outside the box: the quads of an edge come in the order of the
         * directions, each looking along its own. */
        for (index, quad) in outline.iter().enumerate() {
            let got = quad_normal(quad);
            let expected = super::Direction::ALL[index % 6].offset();
            for axis in 0..3 {
                assert!(
                    (got[axis] - expected[axis]).abs() < 1e-5,
                    "quad {index} looks {got:?}, not {expected:?}"
                );
            }
            /* Nothing strays from the block it is drawn around: the box is
             * grown by the outline's own offset and line thickness, no more. */
            for position in quad.positions {
                for axis in 0..3 {
                    /* The offset and half a line's thickness, with room over. */
                    let low = highlight.min[axis] - 0.05;
                    let high = highlight.max[axis] + 0.05;
                    assert!(
                        (low..=high).contains(&position[axis]),
                        "{position:?} at axis {axis}"
                    );
                }
            }
            assert_eq!(quad.tint, Some([0, 0, 0]), "the outline is black");
            assert_eq!(quad.texture, 0, "the checker at index 0, made black");
        }
    }

    #[test]
    fn a_block_box_follows_what_the_block_draws() {
        use super::{Alpha, Highlight, Scene, SceneQuad, Textures};

        let slab = SceneQuad {
            positions: [[0.0, 0.0, 0.0], [1.0, 0.0, 0.0], [1.0, 0.5, 0.0], [0.0, 0.5, 0.0]],
            uv: [[0.0, 0.0]; 4],
            texture: 0,
            alpha: Alpha::Opaque,
            tint: None,
            shade: false,
            light: 1.0,
        };
        let mut scene = Scene::empty();
        scene.textures = Textures::new();
        scene.quads = vec![slab];

        /* The half-height block gives a half-height box, not the whole cell. */
        let boxed = scene.block_box([0, 0, 0]).expect("the cell drew something");
        assert_eq!(boxed, Highlight { min: [0.0, 0.0, 0.0], max: [1.0, 0.5, 0.0] });

        /* A cell nothing was drawn in has no box at all. */
        assert!(scene.block_box([4, 4, 4]).is_none());
    }

    #[test]
    fn setting_a_highlight_builds_and_clears_it() {
        let mut scene = super::Scene::empty();
        assert!(scene.highlight.is_empty());

        scene.set_highlight(Some(super::Highlight {
            min: [0.0, 0.0, 0.0],
            max: [1.0, 1.0, 1.0],
        }));
        assert_eq!(scene.highlight.len(), 12 * 6);

        scene.set_highlight(None);
        assert!(scene.highlight.is_empty());
    }
}
