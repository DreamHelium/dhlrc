/*! Turning a resolved model into the quads a renderer draws.
 *
 * Each box of a model becomes one quad per face it declares. A face's four
 * corners are fixed by the game's rules: the UV rectangle comes from the face's
 * `uv`, or — when it has none — from the box's own extent, and each corner of
 * that rectangle sits at a particular corner of the box, picked so that the
 * quad's front (the winding a renderer keeps) faces *out* of the box. Corner
 * positions are in 0..1 block space and UVs in 0..1 texture space, both ready
 * to be handed to a GPU.
 *
 * A blockstate variant turns the whole model about the block's centre, and the
 * texture usually turns with it. `uvlock` asks for the opposite: the texture
 * stays lined up with the world, which is what keeps the grain of a staircase's
 * planks the right way round in every facing.
 *
 * Two things the game also does are left to the caller, because they need the
 * texture rather than the geometry: `tint` is reported, not applied, and a face
 * whose `cullface` has a solid neighbour can simply be dropped.
 */

use dhlrc_resource::Identifier;

use crate::{
    Axis, BlockModel, Direction, Element, ElementRotation, Face, ResolvedModel, Variant,
};

/// Model units in one block.
const UNITS: f32 = 16.0;

/// One face of a box, as four corners.
#[derive(Debug, Clone, PartialEq)]
pub struct Quad {
    /// The corners in 0..1 block space, wound so the front faces outwards.
    pub positions: [[f32; 3]; 4],
    /// The UV of each corner in 0..1 texture space, matching `positions`.
    pub uv: [[f32; 2]; 4],
    /// The texture drawn, or `None` when the face's variable leads nowhere.
    pub texture: Option<Identifier>,
    /// The colormap index (grass, leaves), or `None` for no tint.
    pub tint: Option<u32>,
    /// The face this one is hidden behind, when a solid block is there.
    pub cullface: Option<Direction>,
    /// Whether to shade the face by the direction it looks.
    pub shade: bool,
}

/// Every quad of `block`, turned the way its blockstate asks for.
pub fn quads(block: &BlockModel) -> Vec<Quad> {
    let mut quads = Vec::new();
    for element in &block.model.elements {
        for (direction, face) in &element.faces {
            quads.push(quad(&block.model, *direction, element, face, &block.variant));
        }
    }
    quads
}

fn quad(
    model: &ResolvedModel,
    direction: Direction,
    element: &Element,
    face: &Face,
    variant: &Variant,
) -> Quad {
    let grid = grid(direction, element, face);
    let (u1, u2, v1, v2) = grid;
    let corners = [(u1, v1), (u1, v2), (u2, v2), (u2, v1)];

    let mut positions = corners.map(|(u, v)| {
        let point = corner(direction, grid, u, v, element);
        let point = match &element.rotation {
            Some(rotation) => turn_element(point, rotation),
            None => point,
        };
        [point[0] / UNITS, point[1] / UNITS, point[2] / UNITS]
    });
    let turned = variant.x != 0 || variant.y != 0;
    if turned {
        for point in &mut positions {
            *point = turn_variant(*point, variant.x, variant.y);
        }
    }

    /* With `uvlock` the texture is tied to the world rather than to the model,
     * so the corners are read back from where the turn put them. Without it the
     * texture rides along on the corners, which is the common case. */
    let mut uv = if variant.uvlock && turned {
        lock_uv(corners, direction, variant, &positions)
    } else {
        corners.map(|(u, v)| [u / UNITS, v / UNITS])
    };
    uv = turn_uv(uv, face.rotation);

    Quad {
        positions,
        uv,
        texture: model.texture(&face.texture),
        tint: face.tint,
        cullface: face.cullface,
        shade: element.shade,
    }
}

/// The UVs of a `uvlock` face, in 0..1 texture space.
///
/// The quad has been turned, so it now lies on the face `turn_direction` names.
/// Each corner is projected onto that face's own `u` and `v` axes — the ones
/// `grid` lays a face's default rectangle out along — and the low end of the
/// rectangle is put at the low end of each axis. A corner that stayed put
/// therefore keeps its UV, and a face whose plane turned under the texture gets
/// the texture turned back.
fn lock_uv(
    corners: [(f32, f32); 4],
    direction: Direction,
    variant: &Variant,
    positions: &[[f32; 3]; 4],
) -> [[f32; 2]; 4] {
    let (u_axis, v_axis) = uv_axes(turn_direction(direction, variant.x, variant.y));
    let (u1, u2) = (corners[0].0, corners[2].0);
    let (v1, v2) = (corners[0].1, corners[1].1);

    let mut across = [0.0f32; 4];
    let mut down = [0.0f32; 4];
    for index in 0..4 {
        across[index] = dot(positions[index], u_axis);
        down[index] = dot(positions[index], v_axis);
    }
    let (across_min, across_max) = extent(across);
    let (down_min, down_max) = extent(down);

    let mut uv = [[0.0f32; 2]; 4];
    for index in 0..4 {
        let u = if nearer(across[index], across_min, across_max) {
            u1
        } else {
            u2
        };
        let v = if nearer(down[index], down_min, down_max) {
            v1
        } else {
            v2
        };
        uv[index] = [u / UNITS, v / UNITS];
    }
    uv
}

/// The directions a face's `u` and `v` increase along, in block space: the way
/// `grid` lays the default rectangle out, so `u` rises from the face's `u1`
/// edge and `v` from its `v1` edge.
fn uv_axes(direction: Direction) -> ([f32; 3], [f32; 3]) {
    match direction {
        Direction::Down => ([1.0, 0.0, 0.0], [0.0, 0.0, -1.0]),
        Direction::Up => ([1.0, 0.0, 0.0], [0.0, 0.0, 1.0]),
        Direction::North => ([-1.0, 0.0, 0.0], [0.0, -1.0, 0.0]),
        Direction::South => ([1.0, 0.0, 0.0], [0.0, -1.0, 0.0]),
        Direction::West => ([0.0, 0.0, 1.0], [0.0, -1.0, 0.0]),
        Direction::East => ([0.0, 0.0, -1.0], [0.0, -1.0, 0.0]),
    }
}

/// The direction a face ends up looking once the blockstate has turned the
/// model: `y` first, then `x`, the order `turn_variant` uses.
fn turn_direction(direction: Direction, x: i32, y: i32) -> Direction {
    let mut normal = direction.offset();
    if y != 0 {
        let (sin, cos) = (y as f32).to_radians().sin_cos();
        normal = [
            normal[0] * cos + normal[2] * sin,
            normal[1],
            -normal[0] * sin + normal[2] * cos,
        ];
    }
    if x != 0 {
        let (sin, cos) = (x as f32).to_radians().sin_cos();
        normal = [
            normal[0],
            normal[1] * cos - normal[2] * sin,
            normal[1] * sin + normal[2] * cos,
        ];
    }
    /* The turn is always a multiple of a quarter turn, so the normal lands on
     * an axis and the largest component names it. */
    let named = [
        (normal[0].abs(), normal[0] > 0.0),
        (normal[1].abs(), normal[1] > 0.0),
        (normal[2].abs(), normal[2] > 0.0),
    ];
    let axis = if named[0].0 >= named[1].0 && named[0].0 >= named[2].0 {
        0
    } else if named[1].0 >= named[2].0 {
        1
    } else {
        2
    };
    let positive = named[axis].1;
    match (axis, positive) {
        (0, true) => Direction::East,
        (0, false) => Direction::West,
        (1, true) => Direction::Up,
        (1, false) => Direction::Down,
        (2, true) => Direction::South,
        _ => Direction::North,
    }
}

/// The smallest and largest of four numbers.
fn extent(values: [f32; 4]) -> (f32, f32) {
    let mut low = values[0];
    let mut high = values[0];
    for value in values {
        low = low.min(value);
        high = high.max(value);
    }
    (low, high)
}

/// Whether `value` is at the `low` end of `low..high` rather than the high one.
/// A face with no extent either way keeps its first value.
fn nearer(value: f32, low: f32, high: f32) -> bool {
    (value - low).abs() <= (value - high).abs()
}

fn dot(a: [f32; 3], b: [f32; 3]) -> f32 {
    a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
}

/// The `(u1, u2, v1, v2)` rectangle a face samples, in 0..16 model units.
fn grid(direction: Direction, element: &Element, face: &Face) -> (f32, f32, f32, f32) {
    if let Some([u1, v1, u2, v2]) = face.uv {
        return (u1, u2, v1, v2);
    }
    let [x1, y1, z1] = element.from;
    let [x2, y2, z2] = element.to;
    /* The default is the box's own extent, turned so the texture reads the
     * right way up on each face (as the game does it). */
    match direction {
        Direction::Down => (x1, x2, UNITS - z2, UNITS - z1),
        Direction::Up => (x1, x2, z1, z2),
        Direction::North => (UNITS - x2, UNITS - x1, UNITS - y2, UNITS - y1),
        Direction::South => (x1, x2, UNITS - y2, UNITS - y1),
        Direction::West => (z1, z2, UNITS - y2, UNITS - y1),
        Direction::East => (UNITS - z2, UNITS - z1, UNITS - y2, UNITS - y1),
    }
}

/// The box point a face's `(u, v)` samples, in 0..16 model units.
///
/// The uv rectangle is stretched over the face's own extent rather than used
/// as a position, so a texture on a partial box stays on the box: a chain's
/// 3-unit strip keeps its `uv` of `[3, 0, 0, 16]` and is still drawn at
/// `x` 6.5..9.5. For a box that fills the face the two readings agree, which is
/// how the game gets away with the default uv below.
fn corner(direction: Direction, grid: (f32, f32, f32, f32), u: f32, v: f32, element: &Element) -> [f32; 3] {
    let (u1, u2, v1, v2) = grid;
    let [x1, y1, z1] = element.from;
    let [x2, y2, z2] = element.to;
    match direction {
        Direction::Down => [
            along(u, u1, u2, x1, x2),
            y1,
            along(v, v1, v2, z2, z1),
        ],
        Direction::Up => [
            along(u, u1, u2, x1, x2),
            y2,
            along(v, v1, v2, z1, z2),
        ],
        Direction::North => [
            along(u, u1, u2, x2, x1),
            along(v, v1, v2, y2, y1),
            z1,
        ],
        Direction::South => [
            along(u, u1, u2, x1, x2),
            along(v, v1, v2, y2, y1),
            z2,
        ],
        Direction::West => [
            x1,
            along(v, v1, v2, y2, y1),
            along(u, u1, u2, z1, z2),
        ],
        Direction::East => [
            x2,
            along(v, v1, v2, y2, y1),
            along(u, u1, u2, z2, z1),
        ],
    }
}

/// Where `t` falls between `t1` and `t2`, placed the same way between `a1` and
/// `a2`. A rectangle with no width keeps its first value rather than dividing
/// by zero.
fn along(t: f32, t1: f32, t2: f32, a1: f32, a2: f32) -> f32 {
    if (t2 - t1).abs() < 1e-6 {
        return a1;
    }
    a1 + (t - t1) * (a2 - a1) / (t2 - t1)
}

/// Turns a point the way an element's own `rotation` asks: about `origin`, by
/// `angle` degrees around `axis`; with `rescale`, the two axes the rotation is
/// not about are then stretched by `1/cos`, which is what keeps a turned plane
/// (a `block/cross` plant) as wide as it was written.
fn turn_element(point: [f32; 3], rotation: &ElementRotation) -> [f32; 3] {
    let [ox, oy, oz] = rotation.origin;
    let radians = rotation.angle.to_radians();
    let (sin, cos) = radians.sin_cos();
    let x = point[0] - ox;
    let y = point[1] - oy;
    let z = point[2] - oz;

    let turned = match rotation.axis {
        Axis::X => [x, y * cos - z * sin, y * sin + z * cos],
        Axis::Y => [x * cos + z * sin, y, -x * sin + z * cos],
        Axis::Z => [x * cos - y * sin, x * sin + y * cos, z],
    };

    /* A quarter turn has no rescale to do (the game only ever writes multiples
     * of 22.5), but a degenerate cosine would send every corner to infinity. */
    let stretch = if rotation.rescale {
        let cosine = cos.abs();
        if cosine < 1e-4 { 1.0 } else { 1.0 / cosine }
    } else {
        1.0
    };
    let scaled = match rotation.axis {
        Axis::X => [turned[0], turned[1] * stretch, turned[2] * stretch],
        Axis::Y => [turned[0] * stretch, turned[1], turned[2] * stretch],
        Axis::Z => [turned[0] * stretch, turned[1] * stretch, turned[2]],
    };

    [
        scaled[0] + ox,
        scaled[1] + oy,
        scaled[2] + oz,
    ]
}

/// Turns the texture on a face, as `rotation` asks: a permutation of the four
/// UVs, clockwise (so 90 sends each corner the value of the one before it).
fn turn_uv(mut uv: [[f32; 2]; 4], rotation: i32) -> [[f32; 2]; 4] {
    let steps = rotation.rem_euclid(360) / 90;
    if steps == 0 {
        return uv;
    }
    let original = uv;
    for index in 0..4 {
        uv[index] = original[(index + steps as usize) % 4];
    }
    uv
}

/// Turns a point the way a blockstate variant asks: about the block's centre,
/// `y` before `x`.
fn turn_variant(point: [f32; 3], x: i32, y: i32) -> [f32; 3] {
    let mut point = point;
    if y != 0 {
        point = turn_about(point, 1, y as f32);
    }
    if x != 0 {
        point = turn_about(point, 0, x as f32);
    }
    point
}

/// Turns `point` about the axis `0`/`1`/`2` through the block's centre.
fn turn_about(point: [f32; 3], axis: usize, degrees: f32) -> [f32; 3] {
    let radians = degrees.to_radians();
    let (sin, cos) = radians.sin_cos();
    let (a, b) = match axis {
        0 => (point[1] - 0.5, point[2] - 0.5),
        1 => (point[2] - 0.5, point[0] - 0.5),
        _ => (point[0] - 0.5, point[1] - 0.5),
    };
    let (turned_a, turned_b) = (a * cos - b * sin, a * sin + b * cos);
    let mut turned = point;
    match axis {
        0 => {
            turned[1] = turned_a + 0.5;
            turned[2] = turned_b + 0.5;
        }
        1 => {
            turned[2] = turned_a + 0.5;
            turned[0] = turned_b + 0.5;
        }
        _ => {
            turned[0] = turned_a + 0.5;
            turned[1] = turned_b + 0.5;
        }
    }
    turned
}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;

    use crate::{BlockModel, Direction, Element, ElementRotation, Face, ResolvedModel, Variant};

    use super::{Quad, quads};

    /// A cube of `from`..`to` whose every face samples `#all`, as a model.
    fn cube(from: [f32; 3], to: [f32; 3]) -> ResolvedModel {
        let mut faces = BTreeMap::new();
        for direction in Direction::ALL {
            faces.insert(
                direction,
                Face {
                    texture: "#all".to_owned(),
                    uv: None,
                    cullface: Some(direction),
                    rotation: 0,
                    tint: None,
                },
            );
        }
        ResolvedModel {
            textures: BTreeMap::from([("all".to_owned(), "minecraft:block/stone".to_owned())]),
            elements: vec![Element {
                from,
                to,
                rotation: None,
                shade: true,
                faces,
            }],
            ambient_occlusion: true,
            chain: Vec::new(),
        }
    }

    fn block(model: ResolvedModel) -> BlockModel {
        BlockModel {
            variant: Variant::default(),
            model,
        }
    }

    fn normal(quad: &Quad) -> [f32; 3] {
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
    fn a_cube_has_one_outward_quad_per_face() {
        let quads = quads(&block(cube([0.0; 3], [16.0; 3])));
        assert_eq!(quads.len(), 6);

        for quad in &quads {
            let facing = quad.cullface.expect("the cube culls every face");
            let expected = facing.offset();
            let got = normal(quad);
            for axis in 0..3 {
                assert!(
                    (got[axis] - expected[axis]).abs() < 1e-5,
                    "the {} face winds the wrong way: {got:?}",
                    facing.name()
                );
            }
        }
    }

    #[test]
    fn a_full_face_covers_the_whole_texture() {
        let quads = quads(&block(cube([0.0; 3], [16.0; 3])));
        for quad in &quads {
            let mut us: Vec<f32> = quad.uv.iter().map(|uv| uv[0]).collect();
            let mut vs: Vec<f32> = quad.uv.iter().map(|uv| uv[1]).collect();
            us.sort_by(f32::total_cmp);
            vs.sort_by(f32::total_cmp);
            assert_eq!(us, [0.0, 0.0, 1.0, 1.0]);
            assert_eq!(vs, [0.0, 0.0, 1.0, 1.0]);
        }
    }

    #[test]
    fn a_partial_box_uses_its_own_extent() {
        /* A box only half as wide and deep in x and z: its up face covers a
         * quarter of the texture, the corner of it the box occupies. */
        let quads = quads(&block(cube([0.0, 0.0, 0.0], [8.0, 16.0, 8.0])));
        let up = quads
            .iter()
            .find(|quad| quad.cullface == Some(Direction::Up))
            .unwrap();
        let mut us: Vec<f32> = up.uv.iter().map(|uv| uv[0]).collect();
        let mut vs: Vec<f32> = up.uv.iter().map(|uv| uv[1]).collect();
        us.sort_by(f32::total_cmp);
        vs.sort_by(f32::total_cmp);
        assert_eq!(us, [0.0, 0.0, 0.5, 0.5]);
        assert_eq!(vs, [0.0, 0.0, 0.5, 0.5]);
    }

    #[test]
    fn an_explicit_uv_is_used_as_given() {
        let mut model = cube([0.0; 3], [16.0; 3]);
        model.elements[0]
            .faces
            .get_mut(&Direction::North)
            .unwrap()
            .uv = Some([4.0, 0.0, 12.0, 16.0]);
        let quads = quads(&block(model));
        let north = quads
            .iter()
            .find(|quad| quad.cullface == Some(Direction::North))
            .unwrap();
        let mut us: Vec<f32> = north.uv.iter().map(|uv| uv[0]).collect();
        us.sort_by(f32::total_cmp);
        assert_eq!(us, [0.25, 0.25, 0.75, 0.75]);
    }

    #[test]
    fn the_texture_and_its_extras_are_carried_through() {
        let mut model = cube([0.0; 3], [16.0; 3]);
        model.elements[0]
            .faces
            .get_mut(&Direction::Up)
            .unwrap()
            .tint = Some(0);
        let quads = quads(&block(model));
        let up = quads
            .iter()
            .find(|quad| quad.cullface == Some(Direction::Up))
            .unwrap();
        assert_eq!(up.tint, Some(0));
        assert_eq!(
            up.texture.as_ref().map(|id| id.to_string()).as_deref(),
            Some("minecraft:block/stone")
        );
        assert!(up.shade);
    }

    #[test]
    fn a_texture_rotation_permutes_the_uvs() {
        let mut model = cube([0.0; 3], [16.0; 3]);
        model.elements[0]
            .faces
            .get_mut(&Direction::Up)
            .unwrap()
            .rotation = 90;
        let quads = quads(&block(model));
        let up = quads
            .iter()
            .find(|quad| quad.cullface == Some(Direction::Up))
            .unwrap();
        /* Unrotated the corners are (0,0) (0,1) (1,1) (1,0); a quarter turn
         * clockwise sends each corner the next one's UV. */
        assert_eq!(up.uv, [[0.0, 1.0], [1.0, 1.0], [1.0, 0.0], [0.0, 0.0]]);
    }

    #[test]
    fn a_variant_rotation_turns_the_whole_model() {
        let mut model = block(cube([0.0; 3], [16.0; 3]));
        model.variant.y = 90;
        let quads = quads(&model);
        /* The north face (normal -Z) is turned a quarter turn about y, so it
         * now looks along -X. */
        let north = quads
            .iter()
            .find(|quad| quad.cullface == Some(Direction::North))
            .unwrap();
        let got = normal(north);
        assert!((got[0] + 1.0).abs() < 1e-5, "normal was {got:?}");
    }

    #[test]
    fn a_model_without_elements_draws_nothing() {
        let quads = quads(&block(ResolvedModel::default()));
        assert!(quads.is_empty());
    }

    #[test]
    fn a_turned_plane_is_stretched_back_to_its_width() {
        /* The shape of `block/cross`: a plane at z=8, a quarter turn about y
         * and `rescale`, so the plant still fills the block's diagonal. */
        let element = Element {
            from: [0.8, 0.0, 8.0],
            to: [15.2, 16.0, 8.0],
            rotation: Some(ElementRotation {
                origin: [8.0, 8.0, 8.0],
                axis: crate::Axis::Y,
                angle: 45.0,
                rescale: true,
            }),
            shade: false,
            faces: BTreeMap::from([(
                Direction::North,
                Face {
                    texture: "#all".to_owned(),
                    uv: Some([0.0, 0.0, 16.0, 16.0]),
                    cullface: None,
                    rotation: 0,
                    tint: None,
                },
            )]),
        };
        let model = ResolvedModel {
            textures: BTreeMap::from([("all".to_owned(), "minecraft:block/stone".to_owned())]),
            elements: vec![element],
            ambient_occlusion: false,
            chain: Vec::new(),
        };

        let quad = quads(&block(model)).remove(0);
        let mut xs: Vec<f32> = quad.positions.iter().map(|point| point[0]).collect();
        let mut zs: Vec<f32> = quad.positions.iter().map(|point| point[2]).collect();
        xs.sort_by(f32::total_cmp);
        zs.sort_by(f32::total_cmp);
        assert!(
            (xs[0] - 0.05).abs() < 1e-5 && (xs[3] - 0.95).abs() < 1e-5,
            "the plane keeps its width in x: {xs:?}"
        );
        assert!(
            (zs[0] - 0.05).abs() < 1e-5 && (zs[3] - 0.95).abs() < 1e-5,
            "and reaches the far corner in z: {zs:?}"
        );
        /* A point is as far below the diagonal as it is to the side of it. */
        for point in &quad.positions {
            assert!((point[0] - 0.5 + (point[2] - 0.5)).abs() < 1e-5);
        }
    }

    /// The quad of the face looking `direction`.
    fn face(quads: &[super::Quad], direction: Direction) -> super::Quad {
        quads
            .iter()
            .find(|quad| quad.cullface == Some(direction))
            .cloned()
            .expect("every face of the cube is there")
    }

    /// A full cube whose blockstate turns it the way `x`/`y` ask.
    fn turned(x: i32, y: i32, uvlock: bool) -> BlockModel {
        let mut block = block(cube([0.0; 3], [16.0; 3]));
        block.variant.x = x;
        block.variant.y = y;
        block.variant.uvlock = uvlock;
        block
    }

    #[test]
    fn a_turned_face_lands_on_the_right_direction() {
        use super::turn_direction;
        assert_eq!(turn_direction(Direction::North, 0, 90), Direction::West);
        assert_eq!(turn_direction(Direction::Up, 0, 90), Direction::Up);
        assert_eq!(turn_direction(Direction::East, 180, 0), Direction::East);
        assert_eq!(turn_direction(Direction::Up, 90, 0), Direction::South);
    }

    #[test]
    fn uvlock_leaves_the_geometry_alone() {
        let plain = quads(&turned(0, 90, false));
        let locked = quads(&turned(0, 90, true));
        assert_eq!(plain.len(), locked.len());
        for (plain, locked) in plain.iter().zip(locked.iter()) {
            assert_eq!(plain.positions, locked.positions);
        }
    }

    #[test]
    fn uvlock_without_a_turn_changes_nothing() {
        assert_eq!(quads(&turned(0, 0, true)), quads(&turned(0, 0, false)));
    }

    #[test]
    fn uvlock_keeps_a_side_face_as_it_was() {
        /* A quarter turn about y moves a side face onto another side face and
         * the texture already lines up there, so nothing should change. */
        for direction in [Direction::North, Direction::South, Direction::West, Direction::East] {
            assert_eq!(
                face(&quads(&turned(0, 90, false)), direction).uv,
                face(&quads(&turned(0, 90, true)), direction).uv,
                "the {} face",
                direction.name()
            );
        }
    }

    #[test]
    fn uvlock_turns_the_top_of_a_turned_block_back() {
        /* The top cannot move to another face, so without uvlock its texture
         * would come out a quarter turn round; with it, the texture stays put. */
        let up = face(&quads(&turned(0, 90, true)), Direction::Up);
        assert_eq!(up.uv, [[0.0, 1.0], [1.0, 1.0], [1.0, 0.0], [0.0, 0.0]]);

        let loose = face(&quads(&turned(0, 90, false)), Direction::Up);
        assert_eq!(loose.uv, [[0.0, 0.0], [0.0, 1.0], [1.0, 1.0], [1.0, 0.0]]);
    }

    #[test]
    fn uvlock_turns_a_side_face_back_when_the_block_is_upside_down() {
        /* An upside-down staircase turns the model a half turn about x, which
         * leaves the east face where it is but would flip its texture. */
        let east = face(&quads(&turned(180, 0, true)), Direction::East);
        assert_eq!(east.uv, [[1.0, 1.0], [1.0, 0.0], [0.0, 0.0], [0.0, 1.0]]);

        let loose = face(&quads(&turned(180, 0, false)), Direction::East);
        assert_eq!(loose.uv, [[0.0, 0.0], [0.0, 1.0], [1.0, 1.0], [1.0, 0.0]]);
    }

    #[test]
    fn uvlock_keeps_the_rectangle_a_reversed_uv_wrote() {
        /* A rectangle whose `u1` is above its `u2` (a chain's 3-unit strip)
         * still puts `u1` on the same edge of the box. */
        let mut model = cube([0.0; 3], [16.0; 3]);
        model.elements[0]
            .faces
            .get_mut(&Direction::Up)
            .unwrap()
            .uv = Some([12.0, 0.0, 4.0, 16.0]);
        let mut block = block(model);
        block.variant.y = 90;
        block.variant.uvlock = true;
        let up = face(&quads(&block), Direction::Up);
        let mut us: Vec<f32> = up.uv.iter().map(|uv| uv[0]).collect();
        us.sort_by(f32::total_cmp);
        assert_eq!(us, [0.25, 0.25, 0.75, 0.75]);
    }
}
