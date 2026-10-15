/*! Times the renderer on a made-up region, so a change can be judged.
 *
 * ```text
 * cargo run --release -p dhlrc-render --example bench -- <client.jar|assets dir> \
 *     [--region 32] [--size 900x640] [--frames 20]
 * ```
 *
 * The region is a solid cube of stone, which is what a viewer spends its time
 * on: the culling leaves its surface (six faces of `region²`) to rasterise, and
 * the whole thing to mesh. Both are reported, because they are paid for at
 * different times — the mesh once, the picture on every mouse move.
 *
 * The build profile matters enormously here: an unoptimised build of this
 * rasteriser is an order of magnitude slower than an optimised one, so run it
 * with `--release` to see what a user would feel.
 *
 * The timed frames go through a [`Renderer`], which keeps its buffers between
 * frames the way a dragging frontend does, and the free `render` is timed
 * beside it to show what allocating both buffers every frame costs.
 */

use std::time::Instant;

use dhlrc_render::camera::Camera;
use dhlrc_render::raster::Renderer;
use dhlrc_render::render;
use dhlrc_render::scene::{VoxelBlock, Voxels, build};
use dhlrc_resource::{DirSource, Identifier, Source, ZipSource};

fn main() {
    let mut arguments = std::env::args().skip(1);
    let Some(root) = arguments.next() else {
        eprintln!("usage: bench <client.jar|assets dir> [--region N] [--size WxH] [--frames N]");
        std::process::exit(2);
    };

    let mut side = 32;
    let mut size = (900u32, 640u32);
    let mut frames = 20;
    while let Some(argument) = arguments.next() {
        match argument.as_str() {
            "--region" => side = parse(&arguments.next().unwrap_or_default(), 32),
            "--size" => {
                let value = arguments.next().unwrap_or_default();
                let (width, height) = value.split_once(['x', 'X']).unwrap_or(("900", "640"));
                size = (
                    parse(width, 900) as u32,
                    parse(height, 640) as u32,
                );
            }
            "--frames" => frames = parse(&arguments.next().unwrap_or_default(), 20),
            _ => {}
        }
    }

    let source = open(&root);
    let voxels = Cube {
        side: side as i32,
    };

    let started = Instant::now();
    let scene = build(source.as_ref(), &voxels);
    let meshed = started.elapsed();
    for skipped in &scene.skipped {
        eprintln!("dhlrc: skipped {skipped}");
    }

    let mut camera = Camera::new(scene.center, 1.0);
    camera.fit(
        scene.min,
        scene.max,
        size.0 as f32 / size.1.max(1) as f32,
    );

    /* A few frames are thrown away first: caches and the allocator settle. */
    let mut renderer = Renderer::new();
    for _ in 0..3 {
        std::hint::black_box(renderer.render(&scene, &camera, size.0, size.1));
    }

    let mut times = Vec::with_capacity(frames);
    for _ in 0..frames {
        let started = Instant::now();
        let image = renderer.render(&scene, &camera, size.0, size.1);
        times.push(started.elapsed());
        std::hint::black_box(image);
    }
    times.sort();

    /* The free function allocates both buffers on every call, which is what a
     * frontend that does not keep a renderer pays; timing it beside the reused
     * one shows what the buffers are worth on their own. */
    let mut fresh = Vec::with_capacity(frames);
    for _ in 0..frames {
        let started = Instant::now();
        let image = render(&scene, &camera, size.0, size.1);
        fresh.push(started.elapsed());
        std::hint::black_box(image);
    }
    let fresh_mean: std::time::Duration =
        fresh.iter().sum::<std::time::Duration>() / frames.max(1) as u32;

    let total: std::time::Duration = times.iter().sum();
    let mean = total / frames.max(1) as u32;
    let pixels = size.0 as f64 * size.1 as f64;
    println!(
        "region {side}³, {} quads, {} textures, {} fill threads",
        scene.quads.len(),
        scene.texture_count(),
        std::thread::available_parallelism()
            .map(|count| count.get())
            .unwrap_or(1)
    );
    println!("  mesh:   {:>8.1} ms", meshed.as_secs_f64() * 1000.0);
    print!("  render: {:>8.1} ms mean", mean.as_secs_f64() * 1000.0);
    println!(
        "  ({:.1} ms best, {:.1} ms worst, {:.0} frames/s at {}x{})",
        times[0].as_secs_f64() * 1000.0,
        times[times.len() - 1].as_secs_f64() * 1000.0,
        1.0 / mean.as_secs_f64().max(1e-9),
        size.0,
        size.1,
    );
    println!(
        "  fresh:  {:>8.1} ms mean (the free `render`, which allocates both buffers)",
        fresh_mean.as_secs_f64() * 1000.0
    );
    println!(
        "  fill:   {:>8.1} Mray/s",
        pixels / mean.as_secs_f64() / 1e6
    );
}

fn parse(value: &str, fallback: usize) -> usize {
    value.parse().unwrap_or(fallback)
}

/// A solid cube of stone: its six faces are what is left after culling.
struct Cube {
    side: i32,
}

impl Voxels for Cube {
    fn size(&self) -> [i32; 3] {
        [self.side; 3]
    }

    fn palette_len(&self) -> usize {
        1
    }

    fn palette(&self, index: usize) -> Option<VoxelBlock> {
        (index == 0).then(|| VoxelBlock {
            name: Identifier::parse("minecraft:stone"),
            properties: String::new(),
        })
    }

    fn block(&self, x: i32, y: i32, z: i32) -> Option<usize> {
        let inside = (0..self.side).contains(&x)
            && (0..self.side).contains(&y)
            && (0..self.side).contains(&z);
        inside.then_some(0)
    }
}

fn open(root: &str) -> Box<dyn Source> {
    let path = std::path::Path::new(root);
    match ZipSource::open(path) {
        Some(source) => Box::new(source),
        None => Box::new(DirSource::new(path)),
    }
}
