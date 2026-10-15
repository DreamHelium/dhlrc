/*! Draws a small hand-made region through the C ABI the Qt view uses.
 *
 * ```text
 * cargo run -p dhlrc-core --features render --example render_region -- \
 *     <client.jar|assets dir> <out.png>
 * ```
 *
 * This is the smoke test for `dhlrc_render_scene_new` and friends without a
 * GUI and without a region file: the `Region` is built here, so a mismatch
 * between what the plugins produce and what the ABI reads (the block array's
 * order, a palette entry's properties) shows up as a wrong picture rather than
 * as a crash in a frontend.
 */

use std::ffi::{CString, c_void};
use std::ptr;

use common_rs::region::{BaseData, Palette, Region};
use dhlrc_core::ffi::{dhlrc_core_free, dhlrc_core_open, dhlrc_core_resources_configure};
use dhlrc_core::render::{
    DhlrcCamera, dhlrc_render_fit, dhlrc_render_image, dhlrc_render_scene_free,
    dhlrc_render_scene_new, dhlrc_render_scene_quads, dhlrc_render_scene_skipped,
};
use time::UtcDateTime;

/// A palette entry from a block id and its properties.
fn palette(id: &str, properties: &[(&str, &str)]) -> Palette {
    Palette {
        id_name: id.to_owned(),
        property: properties
            .iter()
            .map(|(name, value)| ((*name).to_owned(), (*value).to_owned()))
            .collect(),
    }
}

/// A region holding `blocks` at the given positions, one air block around the
/// edges so the outside faces are drawn.
fn region(blocks: &[((i32, i32, i32), usize)], palette_array: Vec<Palette>) -> Region {
    let size = (7, 4, 7);
    let mut block_array = vec![0; (size.0 * size.1 * size.2) as usize];
    /* Entry 0 is air, so anything unset draws nothing. */
    for ((x, y, z), entry) in blocks {
        let index = (size.0 * size.2 * y + size.0 * z + x) as usize;
        block_array[index] = *entry as u32;
    }
    Region {
        data_version: 4189,
        base_data: BaseData {
            create_time: UtcDateTime::from_unix_timestamp(0).unwrap(),
            modify_time: UtcDateTime::from_unix_timestamp(0).unwrap(),
            description: String::new(),
            author: String::new(),
            name: "smoke".to_owned(),
            region_name: "smoke".to_owned(),
        },
        region_size: size,
        region_offset: (0, 0, 0),
        block_array,
        block_entity_array: Vec::new(),
        entity_array: Vec::new(),
        palette_array,
    }
}

fn main() {
    let mut arguments = std::env::args().skip(1);
    let (Some(root), Some(output)) = (arguments.next(), arguments.next()) else {
        eprintln!("usage: render_region <client.jar|assets dir> <out.png>");
        std::process::exit(2);
    };

    /* A floor of stone with a few blocks that between them need the tint, the
     * cutout, the translucency and the blockstate the properties pick. */
    let palette_array = vec![
        palette("minecraft:air", &[]),
        palette("minecraft:stone", &[]),
        palette("minecraft:grass_block", &[("snowy", "false")]),
        palette("minecraft:oak_leaves", &[]),
        palette("minecraft:glass", &[]),
        palette(
            "minecraft:oak_stairs",
            &[
                ("facing", "east"),
                ("half", "bottom"),
                ("shape", "straight"),
            ],
        ),
        palette("minecraft:short_grass", &[]),
    ];

    let mut blocks = Vec::new();
    for x in 0..7 {
        for z in 0..7 {
            blocks.push(((x, 1, z), 1));
        }
    }
    for (position, entry) in [
        ((1, 2, 1), 2),
        ((3, 2, 1), 3),
        ((5, 2, 1), 4),
        ((1, 2, 3), 5),
        ((3, 2, 3), 6),
    ] {
        blocks.push((position, entry));
    }
    let region = region(&blocks, palette_array);

    let config = std::env::temp_dir().join(format!("dhlrc-render-region-{}", std::process::id()));
    let config_c = CString::new(config.to_string_lossy().as_bytes()).unwrap();
    let mut error: *mut std::ffi::c_char = ptr::null_mut();
    let core = dhlrc_core_open(config_c.as_ptr(), None, ptr::null_mut(), &mut error);
    if core.is_null() {
        eprintln!("could not open the core");
        std::process::exit(1);
    }

    let cache = CString::new("").unwrap();
    let root = CString::new(root.as_str()).unwrap();
    let packs = [root.as_ptr()];
    dhlrc_core_resources_configure(
        core,
        cache.as_ptr(),
        ptr::null(),
        packs.as_ptr(),
        packs.len(),
    );

    let width = 900u32;
    let height = 600u32;
    let scene = unsafe {
        dhlrc_render_scene_new(core as *mut c_void, &region as *const Region as *const c_void)
    };
    if scene.is_null() {
        eprintln!("the scene could not be built");
        std::process::exit(1);
    }

    let mut camera = DhlrcCamera {
        target: [3.5, 1.5, 3.5],
        yaw: 45.0,
        pitch: 30.0,
        distance: 16.0,
        fov: 45.0,
        orthographic: 0,
    };
    unsafe { dhlrc_render_fit(scene, &mut camera, width, height) };
    let pixels = unsafe { dhlrc_render_image(scene, &camera, width, height) };
    if pixels.is_null() {
        eprintln!("nothing was drawn");
        std::process::exit(1);
    }

    let image = dhlrc_render::Image {
        width,
        height,
        pixels: unsafe { std::slice::from_raw_parts(pixels, (width * height * 4) as usize) }
            .to_vec(),
    };
    if let Err(error) = image.write_png(&output) {
        eprintln!("could not write {output}: {error}");
        std::process::exit(1);
    }

    println!(
        "{} quads, {} palette entries skipped -> {output}",
        unsafe { dhlrc_render_scene_quads(scene) },
        unsafe { dhlrc_render_scene_skipped(scene) }
    );

    /* The same call a click in the Qt view makes: the middle of the picture
     * should land on the block standing there. */
    let mut block = [0i32; 3];
    let hit = unsafe {
        dhlrc_core::render::dhlrc_render_pick(
            scene,
            &camera,
            width,
            height,
            width as f64 / 2.0,
            height as f64 / 2.0,
            block.as_mut_ptr(),
        )
    };
    if hit == 1 {
        println!("the middle of the picture is ({}, {}, {})", block[0], block[1], block[2]);
    } else {
        println!("the middle of the picture is empty");
    }

    unsafe {
        dhlrc_render_scene_free(scene);
        dhlrc_core_free(core);
    }
    std::fs::remove_dir_all(&config).ok();
}
