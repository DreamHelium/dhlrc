/*! Renders blocks from a real client jar (or an unpacked assets directory).
 *
 * ```text
 * cargo run -p dhlrc-render --example render -- <client.jar|assets dir> <out.png> [blocks...]
 * ```
 *
 * Every argument after the output is a block (`minecraft:oak_stairs[facing=east,half=bottom]`),
 * and they are placed in a row one block apart so one picture shows them all.
 * With none, a few blocks are chosen whose looks the renderer has to get right:
 * a plain cube, a tinted one, leaves (cutout), water (translucent), a turned
 * model (`block/cross`), and a chain (a narrow box with its own uv).
 *
 * `--size WxH`, `--yaw`, `--pitch`, `--distance`, `--ortho` and `--background`
 * change the picture. All of them are also what the interactive views set.
 */

use std::collections::HashMap;

use dhlrc_resource::{DirSource, Identifier, Source, ZipSource};
use dhlrc_render::camera::Camera;
use dhlrc_render::scene::{VoxelBlock, Voxels, build};
use dhlrc_render::{Image, render};

fn main() {
    let mut arguments = std::env::args().skip(1).peekable();
    let Some(root) = arguments.next() else {
        eprintln!(
            "usage: render <client.jar|assets dir> <out.png> [block...] \
             [--size WxH] [--yaw d] [--pitch d] [--distance d] [--ortho] [--background rrggbb]"
        );
        std::process::exit(2);
    };
    let Some(output) = arguments.next() else {
        eprintln!("usage: render <client.jar|assets dir> <out.png> [block...]");
        std::process::exit(2);
    };

    let mut size = (960u32, 540u32);
    let mut camera = Camera::new([0.0; 3], 16.0);
    let mut background: Option<[u8; 3]> = None;
    let mut distance_set = false;
    let mut blocks: Vec<VoxelBlock> = Vec::new();

    while let Some(argument) = arguments.next() {
        match argument.as_str() {
            "--size" => size = parse_size(&arguments.next().unwrap_or_default()),
            "--yaw" => camera.yaw = parse_number(&arguments.next().unwrap_or_default()),
            "--pitch" => camera.pitch = parse_number(&arguments.next().unwrap_or_default()),
            "--distance" => {
                camera.distance = parse_number(&arguments.next().unwrap_or_default());
                distance_set = true;
            }
            "--ortho" => camera.orthographic = true,
            "--background" => background = Some(parse_color(&arguments.next().unwrap_or_default())),
            value => blocks.push(parse_block(value)),
        }
    }
    if blocks.is_empty() {
        blocks = default_blocks();
    }

    let source = open(&root);
    let demo = Demo::row(&blocks);
    let scene = build(source.as_ref(), &demo);
    for skipped in &scene.skipped {
        eprintln!("dhlrc: skipped {skipped}");
    }

    let fitted = camera.distance;
    camera.fit(scene.min, scene.max, size.0 as f32 / size.1.max(1) as f32);
    if distance_set {
        camera.distance = fitted;
    }
    let image = render(&scene, &camera, size.0, size.1);
    if let Some(color) = background {
        let image = over(image, color);
        save(&image, &output);
    } else {
        save(&image, &output);
    }
    println!(
        "{} quads, {} textures -> {output} ({}x{})",
        scene.quads.len(),
        scene.texture_count(),
        size.0,
        size.1
    );
}

fn save(image: &Image, output: &str) {
    if let Err(error) = image.write_png(output) {
        eprintln!("could not write {output}: {error}");
        std::process::exit(1);
    }
}

/// Puts the transparent picture over one flat colour, so a viewer that shows
/// transparency as a checkerboard does not hide dark blocks.
fn over(image: Image, color: [u8; 3]) -> Image {
    let mut out = image.clone();
    for pixel in out.pixels.chunks_mut(4) {
        let alpha = pixel[3] as f32 / 255.0;
        for channel in 0..3 {
            pixel[channel] = (pixel[channel] as f32 * alpha
                + color[channel] as f32 * (1.0 - alpha))
                .round() as u8;
        }
        pixel[3] = 255;
    }
    out
}

fn parse_size(value: &str) -> (u32, u32) {
    match value.split_once(['x', 'X']) {
        Some((width, height)) => (
            width.parse().unwrap_or(960),
            height.parse().unwrap_or(540),
        ),
        None => (960, 540),
    }
}

fn parse_number(value: &str) -> f32 {
    value.parse().unwrap_or(0.0)
}

fn parse_color(value: &str) -> [u8; 3] {
    let number = u32::from_str_radix(value.trim_start_matches('#'), 16).unwrap_or(0x303030);
    [
        (number >> 16) as u8,
        (number >> 8) as u8,
        number as u8,
    ]
}

/// `minecraft:oak_stairs[facing=east,half=bottom]` as the renderer wants it.
fn parse_block(value: &str) -> VoxelBlock {
    match value.split_once('[') {
        Some((name, properties)) => VoxelBlock {
            name: Identifier::parse(name),
            properties: properties.trim_end_matches(']').to_owned(),
        },
        None => VoxelBlock {
            name: Identifier::parse(value),
            properties: String::new(),
        },
    }
}

/// A few blocks that between them need every part of the renderer: a plain
/// cube, a tinted plant, cutout leaves, a solid log, a thin turned chain, a
/// stair whose model the state picks, and glass.
fn default_blocks() -> Vec<VoxelBlock> {
    [
        "minecraft:stone",
        "minecraft:grass_block[snowy=false]",
        "minecraft:oak_leaves",
        "minecraft:short_grass",
        "minecraft:oak_log[axis=y]",
        "minecraft:chain[axis=y]",
        "minecraft:oak_stairs[facing=east,half=bottom,shape=straight]",
        "minecraft:glass",
    ]
    .into_iter()
    .map(parse_block)
    .collect()
}

/// The source to read from: a jar when `root` is one, a directory otherwise.
fn open(root: &str) -> Box<dyn Source> {
    let path = std::path::Path::new(root);
    match ZipSource::open(path) {
        Some(source) => Box::new(source),
        None => Box::new(DirSource::new(path)),
    }
}

/// A row of blocks, one apart, as a region for the renderer.
struct Demo {
    size: [i32; 3],
    blocks: Vec<VoxelBlock>,
    positions: HashMap<i32, usize>,
}

impl Demo {
    fn row(blocks: &[VoxelBlock]) -> Self {
        let mut positions = HashMap::new();
        for index in 0..blocks.len() {
            positions.insert(index as i32 * 2, index);
        }
        Self {
            size: [(blocks.len() as i32 * 2).max(1), 1, 1],
            blocks: blocks.to_vec(),
            positions,
        }
    }
}

impl Voxels for Demo {
    fn size(&self) -> [i32; 3] {
        self.size
    }

    fn palette_len(&self) -> usize {
        self.blocks.len()
    }

    fn palette(&self, index: usize) -> Option<VoxelBlock> {
        self.blocks.get(index).cloned()
    }

    fn block(&self, x: i32, y: i32, z: i32) -> Option<usize> {
        if y != 0 || z != 0 {
            return None;
        }
        self.positions.get(&x).copied()
    }
}
