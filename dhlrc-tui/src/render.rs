/*! The `render` command.
 *
 * It loads a structure file headlessly and writes what it holds as a PNG, using
 * the same software renderer the Qt view draws through. Nothing here decides
 * what a block looks like: the region is read by `region`, the models and
 * textures by `dhlrc_render`, and this only settles the camera, the background
 * and where the file goes.
 *
 * The picture is written with an alpha channel, so the default background is
 * nothing at all; `--background rrggbb` puts a flat colour behind the blocks
 * for a viewer that shows transparency as a checkerboard.
 */

use crate::cli::RenderArgs;
use crate::i18n::gettext;
use crate::region;
use dhlrc_core::config::{Config, default_paths, load_or_create};
use dhlrc_core::download::{Downloader, Never, Sink};
use dhlrc_render::camera::Camera;
use dhlrc_render::{Highlight, Image, build_scene, render};
use dhlrc_resource::{ResourceEnvironment, ResourceSet, ZipSource};
use formatx::formatx;
use std::path::PathBuf;
use std::process::ExitCode;

pub fn run(args: RenderArgs) -> ExitCode {
    match draw(&args) {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("dhlrc-tui: {error}");
            ExitCode::FAILURE
        }
    }
}

/// Everything the command does, so `run` only has to report the outcome.
fn draw(args: &RenderArgs) -> Result<(), String> {
    let (width, height) = parse_size(&args.size)?;
    let background = match &args.background {
        Some(value) => Some(parse_color(value)?),
        None => None,
    };

    /* The region is loaded first: the data version it carries names the version
     * whose client jar holds its block models and textures, which is what says
     * whether a resource has to be fetched before anything can be drawn. */
    let (region_dir, load_dir) = default_dirs(args);
    let voxels = region::Region::load(
        &args.file,
        &region::Options {
            region_dir: &region_dir,
            load_dir: &load_dir,
            region_lib: args.region_lib.as_deref(),
            region: args.region.as_deref(),
        },
    )
    .map_err(|error| format!("{}: {error}", args.file.display()))?;

    let set = resource_set(args, voxels.data_version())?;

    let mut scene = build_scene(&set, &voxels);
    if let Some(cell) = &args.highlight {
        let cell = highlight_cell(cell)?;
        /* The box the block is drawn in, so a slab comes out half a cell; the
         * whole cell is the answer when the block drew nothing. */
        let boxed = scene.block_box(cell).unwrap_or(Highlight {
            min: [cell[0] as f32, cell[1] as f32, cell[2] as f32],
            max: [
                cell[0] as f32 + 1.0,
                cell[1] as f32 + 1.0,
                cell[2] as f32 + 1.0,
            ],
        });
        scene.set_highlight(Some(boxed));
    }
    for skipped in &scene.skipped {
        eprintln!("dhlrc-tui: {skipped}");
    }

    let aspect = width as f32 / height.max(1) as f32;
    let mut camera = Camera::new(scene.center, scene.radius * 2.0);
    if let Some(yaw) = args.yaw {
        camera.yaw = yaw;
    }
    if let Some(pitch) = args.pitch {
        camera.pitch = pitch;
    }
    camera.orthographic = args.ortho;
    /* Framing the box gives a distance that fits; an explicit one says the
     * caller knows better and is restored afterwards. */
    camera.fit(scene.min, scene.max, aspect);
    if let Some(distance) = args.distance {
        camera.distance = distance;
    }

    let image = render(&scene, &camera, width, height);
    let image = match background {
        Some(color) => over(image, color),
        None => image,
    };

    if scene.quads.is_empty() {
        eprintln!(
            "{}",
            gettext("Nothing was drawn: check --jar/--resources and the region's palette.")
        );
    }

    let output = output_path(args);
    if let Some(parent) = output
        .parent()
        .filter(|parent| !parent.as_os_str().is_empty())
    {
        std::fs::create_dir_all(parent)
            .map_err(|error| format!("could not create {}: {error}", parent.display()))?;
    }
    image
        .write_png(&output)
        .map_err(|error| format!("could not write {}: {error}", output.display()))?;

    println!(
        "{}",
        formatx!(
            gettext("{}x{}: {} quads, {} textures, {} skipped palette entries -> {}"),
            width,
            height,
            scene.quads.len(),
            scene.texture_count(),
            scene.skipped.len(),
            output.display().to_string()
        )
        .unwrap_or_default()
    );
    Ok(())
}

/// The two module directories, the command's options over the executable's own.
fn default_dirs(args: &RenderArgs) -> (PathBuf, PathBuf) {
    let (region, load) = dhlrc_core::modules::default_dirs();
    (
        args.region_dir.clone().unwrap_or(region),
        args.load_dir.clone().unwrap_or(load),
    )
}

/// The models and textures to read, in the order a lookup tries them: the
/// `--jar`, then the cache's client jar and asset objects.
///
/// A version's block models and textures are only in its client jar, so when
/// one is missing and the configuration allows it, it is fetched here — once
/// per version, about 30 MB. `--no-download` and the `DownloadClientJar`
/// setting both turn that off; then the version's jar has to be put in the
/// cache by hand (or passed as `--jar`), and a picture of a file that needs it
/// comes out empty.
fn resource_set(args: &RenderArgs, data_version: u32) -> Result<ResourceSet, String> {
    let (cache_dir, allow_download) = configured(&args.resources);
    /* `--version` names the version outright; without it the region's data
     * version does, through the same table the downloader uses. */
    let version = args
        .version
        .clone()
        .or_else(|| dhlrc_core::download::version_id(data_version as i32));

    if args.jar.is_none() {
        if let Some(version) = &version {
            if !dhlrc_core::download::client_jar_is_there(&cache_dir, version) {
                if allow_download && !args.no_download {
                    fetch_client_jar(version, &cache_dir)?;
                } else {
                    eprintln!(
                        "dhlrc-tui: {}",
                        formatx!(
                            gettext(
                                "the client jar of {} is not in {}: pass --jar, or let it be downloaded (the DownloadClientJar setting)",
                            ),
                            version.clone(),
                            cache_dir.display().to_string()
                        )
                        .unwrap_or_default()
                    );
                }
            }
        }
    }

    let mut environment = ResourceEnvironment {
        cache_dir,
        version,
        packs: Vec::new(),
    };
    if let Some(jar) = &args.jar {
        /* A `ZipSource` that will not open is skipped silently, so the path is
         * checked here where the message can say which one it was. */
        if !jar.is_file() {
            return Err(format!("{} is not a file", jar.display()));
        }
        if ZipSource::open(jar.clone()).is_none() {
            return Err(format!("{} is not a zip archive", jar.display()));
        }
        environment.packs.push(jar.clone());
    }

    let set = environment.resource_set();
    if set.is_empty() {
        return Err(gettext(
            "no resources to read models and textures from; pass --jar or --resources",
        ));
    }
    Ok(set)
}

/// Fetches the version's client jar, reporting progress on standard error. The
/// picture cannot be drawn without it, so a failure here fails the command.
fn fetch_client_jar(version: &str, cache_dir: &std::path::Path) -> Result<(), String> {
    eprintln!(
        "{}",
        formatx!(
            gettext("downloading the client jar of {} (about 30 MB)"),
            version.to_owned()
        )
        .unwrap_or_default()
    );
    let downloader = Downloader::new(cache_dir);
    match downloader.download_client_version(version, &Progress, &Never) {
        Ok(path) => {
            eprintln!("{}", formatx!(gettext("downloaded {}"), path.display().to_string()).unwrap_or_default());
            Ok(())
        }
        Err(error) => Err(formatx!(gettext("could not download the client jar: {}"), error.to_string()).unwrap_or_default()),
    }
}

/// Prints the download's own messages and one redrawn progress line.
struct Progress;

impl Sink for Progress {
    fn info(&self, message: &str) {
        eprintln!("{message}");
    }

    fn progress(&self, received: u64, total: u64) {
        if total > 0 {
            eprint!("\r  {:.0}%", received as f64 * 100.0 / total as f64);
        }
    }
}

/// The two things the configuration says about resources: where the cache is,
/// and whether a missing client jar may be fetched by itself.
///
/// The configuration is not created here: a headless render should not leave a
/// file behind just because it was asked for a picture.
fn configured(cache_dir: &Option<PathBuf>) -> (PathBuf, bool) {
    let mut directory = Config::default().general.cache_dir();
    let mut download = Config::default().render.download_client_jar;
    let paths = default_paths();
    if paths.file.is_file() {
        if let Ok(init) = load_or_create(&paths) {
            directory = init.config.general.cache_dir();
            download = init.config.render.download_client_jar;
        }
    }
    (cache_dir.clone().unwrap_or(directory), download)
}

/// The PNG to write: the option, or `render.png` beside the input.
fn output_path(args: &RenderArgs) -> PathBuf {
    match &args.output {
        Some(path) => path.clone(),
        None => args
            .file
            .parent()
            .map(|parent| parent.join("render.png"))
            .unwrap_or_else(|| PathBuf::from("render.png")),
    }
}

/// `X,Y,Z`, as `--highlight` spells it.
fn highlight_cell(text: &str) -> Result<[i32; 3], String> {
    let parts: Vec<&str> = text.split(',').map(str::trim).collect();
    if parts.len() != 3 {
        return Err(format!("{text} is not a block coordinate; write it as X,Y,Z"));
    }
    let mut cell = [0i32; 3];
    for (index, part) in parts.iter().enumerate() {
        cell[index] = part
            .parse()
            .map_err(|_| format!("{part} is not a whole number"))?;
    }
    Ok(cell)
}

/// `WxH`, as `--size` spells it.
fn parse_size(value: &str) -> Result<(u32, u32), String> {
    let (width, height) = value
        .split_once(['x', 'X'])
        .ok_or_else(|| format!("{value} is not a size; write it as WxH"))?;
    let width = width
        .parse::<u32>()
        .map_err(|_| format!("{value} is not a size; write it as WxH"))?;
    let height = height
        .parse::<u32>()
        .map_err(|_| format!("{value} is not a size; write it as WxH"))?;
    if width == 0 || height == 0 {
        return Err(format!("{value} is not a size; neither side may be 0"));
    }
    Ok((width, height))
}

/// `rrggbb`, with or without a leading `#`.
fn parse_color(value: &str) -> Result<[u8; 3], String> {
    let digits = value.strip_prefix('#').unwrap_or(value);
    if digits.len() != 6 || !digits.chars().all(|c| c.is_ascii_hexdigit()) {
        return Err(format!("{value} is not a colour; write it as rrggbb"));
    }
    let number = u32::from_str_radix(digits, 16)
        .map_err(|_| format!("{value} is not a colour; write it as rrggbb"))?;
    Ok([(number >> 16) as u8, (number >> 8) as u8, number as u8])
}

/// Puts the transparent picture over one flat colour, so a viewer that shows
/// transparency as a checkerboard does not hide dark blocks.
fn over(image: Image, color: [u8; 3]) -> Image {
    let mut out = image;
    for pixel in out.pixels.chunks_mut(4) {
        let alpha = pixel[3] as f32 / 255.0;
        for channel in 0..3 {
            pixel[channel] = (pixel[channel] as f32 * alpha + color[channel] as f32 * (1.0 - alpha))
                .round() as u8;
        }
        pixel[3] = 255;
    }
    out
}

#[cfg(test)]
mod tests {
    use super::{output_path, parse_color, parse_size};
    use crate::cli::RenderArgs;
    use std::path::{Path, PathBuf};

    fn args(file: &str, output: Option<&str>) -> RenderArgs {
        RenderArgs {
            file: PathBuf::from(file),
            output: output.map(PathBuf::from),
            size: "960x540".to_owned(),
            yaw: None,
            pitch: None,
            distance: None,
            ortho: false,
            highlight: None,
            background: None,
            region_dir: None,
            load_dir: None,
            region_lib: None,
            jar: None,
            resources: None,
            version: None,
            no_download: false,
            region: None,
        }
    }

    #[test]
    fn a_size_is_two_numbers_around_an_x() {
        assert_eq!(parse_size("960x540"), Ok((960, 540)));
        assert_eq!(parse_size("64X64"), Ok((64, 64)));
        assert!(parse_size("960").is_err());
        assert!(parse_size("0x540").is_err());
        assert!(parse_size("wide x tall").is_err());
    }

    #[test]
    fn a_colour_is_six_hex_digits() {
        assert_eq!(parse_color("1a2b3c"), Ok([0x1a, 0x2b, 0x3c]));
        assert_eq!(parse_color("#ffffff"), Ok([255, 255, 255]));
        assert!(parse_color("fff").is_err());
        assert!(parse_color("gggggg").is_err());
    }

    #[test]
    fn the_default_output_sits_beside_the_input() {
        assert_eq!(
            output_path(&args("temp/house.litematic", None)),
            Path::new("temp/render.png")
        );
        assert_eq!(
            output_path(&args("house.litematic", None)),
            Path::new("render.png")
        );
        assert_eq!(
            output_path(&args("temp/house.litematic", Some("out.png"))),
            Path::new("out.png")
        );
    }
}
