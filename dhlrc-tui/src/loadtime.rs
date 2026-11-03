/*! Measuring and predicting how long a region file takes to load.
 *
 * There is no metadata-only way into a region file: the only honest number for
 * a file is the time a real load takes. This command therefore records every
 * measured load in a small history and answers a new file by extrapolating
 * from it. The history format and the prediction maths live in
 * `dhlrc_core::loadtime`, so the GUI and the terminal frontend read and write
 * the same file; this module adds the measured-load loop and the header-based
 * estimate.
 *
 * The history is a tab separated file in the cache directory, one measured
 * load per line, so it can be read and edited by hand. It is a cache, not a
 * setting: deleting it only loses the predictions' confidence.
 */

use crate::cli::LoadTimeArgs;
use crate::i18n::gettext;
use crate::region;
use dhlrc_core::config::{default_paths, load_or_create, Config};
use dhlrc_core::loadtime::{
    format_for, memory_guess, modified_millis, Basis, Confidence, History, Sample,
    GUESS_FIXED_MILLIS, GUESS_FIXED_REGION_MILLIS, GUESS_MILLIS_PER_MEGABYTE,
    GUESS_NANOS_PER_BLOCK, GUESS_REGION_SPREAD,
};
use dhlrc_core::modules::Modules;
use flate2::read::GzDecoder;
use formatx::formatx;
use std::fs;
use std::io::Read;
use std::path::{Path, PathBuf};
use std::time::Instant;

/// The size of a region, in blocks, and how many blocks it holds. This is what
/// a region module's optional estimate is turned into; the modules own the
/// format knowledge, this command only uses the numbers.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct RegionSize {
    /// The extent of the region along each axis.
    pub extent: [i32; 3],
    /// Blocks that are set, or the volume when the format only gives that.
    pub blocks: u64,
    /// Whether `blocks` is the number of set blocks (`true`) or the volume of
    /// the region (`false`).
    pub blocks_exact: bool,
    /// Regions in a multi-region file; 1 for the single-region formats.
    pub regions: usize,
}

impl RegionSize {
    /// The bounding-box volume, which is what a fully filled region would hold.
    pub fn volume(&self) -> u64 {
        self.extent
            .iter()
            .map(|value| (*value).max(0) as u64)
            .product()
    }

    /// A human readable form, `10 x 7 x 52`.
    pub fn extent_text(&self) -> String {
        format!(
            "{} x {} x {}",
            self.extent[0], self.extent[1], self.extent[2]
        )
    }

    /// The module-provided estimate, with the "unknown" markers (`-1`) folded
    /// into the shape this command uses. `None` when the module knew no blocks
    /// at all, which is the same as no estimate.
    fn from_module(estimate: dhlrc_core::ffi::DhlrcEstimate) -> Option<Self> {
        if estimate.blocks <= 0 {
            return None;
        }
        Some(Self {
            extent: [estimate.size_x, estimate.size_y, estimate.size_z],
            blocks: estimate.blocks as u64,
            blocks_exact: estimate.blocks_exact != 0,
            regions: estimate.regions.max(1) as usize,
        })
    }
}

/// What `--estimate` can say about a file without loading it.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Estimate {
    /// The size after decompression, when the file could be streamed.
    pub unpacked: Option<u64>,
    /// The region's extent and block count, when its headers could be read.
    pub region: Option<RegionSize>,
    /// A load time implied by the block count (not by a measurement).
    pub millis_low: f64,
    pub millis_high: f64,
    pub memory_bytes: f64,
    pub memory_low: f64,
    pub memory_high: f64,
}

/// Stream the file through its gzip decoder and count the bytes, without
/// parsing anything. This is the cheap half of a load (the file is decompressed
/// anyway) and its result is proportional to the block count, so it is a much
/// better base for an estimate than the compressed size. `None` when the file
/// is not gzip or cannot be read.
pub fn unpacked_size(path: &Path) -> Option<u64> {
    let file = fs::File::open(path).ok()?;
    let mut decoder = GzDecoder::new(std::io::BufReader::new(file));
    let mut buffer = [0u8; 64 * 1024];
    let mut total = 0u64;
    loop {
        match decoder.read(&mut buffer) {
            Ok(0) => return Some(total),
            Ok(count) => total += count as u64,
            Err(_) => return None,
        }
    }
}

/// Estimates a file's load time and memory from its headers.
///
/// The block count is the base when the format gives it: compression makes the
/// byte size a poor stand-in (a well-filled region can be a tenth the size of a
/// sparse one with the same dimensions). When the headers cannot be read the
/// unpacked size is the fallback, and the compressed size the last resort.
pub fn estimate(path: &Path, size: u64, modules: Option<&Modules>) -> Estimate {
    /* The format knowledge belongs to the region modules; this command asks
     * the one that claims the file's suffix. A module built before the
     * optional symbol existed, or no matching module at all, answers `None`
     * and the file size stays the basis, exactly as before. */
    let module_estimate = modules.and_then(|modules| modules.region_estimate(path));
    let region = module_estimate.and_then(RegionSize::from_module);
    /* The module counted the bytes it decompressed, so a full pass over the
     * file is not needed; without a module the streamed gzip pass is cheap
     * next to a load and gives the fallback basis the sparse formats need. */
    let unpacked = module_estimate
        .and_then(|estimate| {
            (estimate.uncompressed_bytes >= 0).then_some(estimate.uncompressed_bytes as u64)
        })
        .or_else(|| unpacked_size(path));
    estimate_with(size, unpacked, region)
}

/// The estimate itself, taking the parts a test can hand in directly.
fn estimate_with(size: u64, unpacked: Option<u64>, region: Option<RegionSize>) -> Estimate {
    if let Some(region) = region {
        /* A load walks every block of the region it decodes, so the time is
         * based on `blocks` as the format reports it (set blocks when known,
         * the volume otherwise). The memory only holds the blocks that exist,
         * so a sparse region's volume is capped by what the unpacked data
         * could describe; the core owns that model so the C interface reports
         * the same numbers. */
        let millis =
            GUESS_FIXED_REGION_MILLIS + region.blocks as f64 * GUESS_NANOS_PER_BLOCK / 1_000_000.0;
        let module_estimate = dhlrc_core::ffi::DhlrcEstimate {
            regions: region.regions as i64,
            blocks: region.blocks as i64,
            uncompressed_bytes: unpacked.map(|bytes| bytes as i64).unwrap_or(-1),
            size_x: region.extent[0],
            size_y: region.extent[1],
            size_z: region.extent[2],
            blocks_exact: i32::from(region.blocks_exact),
            ok: 1,
        };
        let memory = memory_guess(size, unpacked, Some(&module_estimate));
        return Estimate {
            unpacked,
            region: Some(region),
            millis_low: millis * (1.0 - GUESS_REGION_SPREAD),
            millis_high: millis * (1.0 + GUESS_REGION_SPREAD),
            memory_bytes: memory.bytes,
            memory_low: memory.low,
            memory_high: memory.high,
        };
    }
    let basis = unpacked.unwrap_or(size) as f64;
    let memory = memory_guess(size, unpacked, None);
    Estimate {
        unpacked,
        region: None,
        millis_low: GUESS_FIXED_MILLIS + basis / 1_000_000.0 * GUESS_MILLIS_PER_MEGABYTE * 0.5,
        millis_high: GUESS_FIXED_MILLIS + basis / 1_000_000.0 * GUESS_MILLIS_PER_MEGABYTE * 2.0,
        memory_bytes: memory.bytes,
        memory_low: memory.low,
        memory_high: memory.high,
    }
}

/// The cache directory the history lives in, taken from the configuration the
/// same way the render command does; a missing or old configuration just means
/// the defaults.
fn cache_dir() -> PathBuf {
    let mut directory = Config::default().general.cache_dir();
    let paths = default_paths();
    if paths.file.is_file() {
        if let Ok(init) = load_or_create(&paths) {
            if !init.needs_reset {
                directory = init.config.general.cache_dir();
            }
        }
    }
    directory
}

/// The region modules and object codecs, or the ones beside the executable.
fn module_dirs(args: &LoadTimeArgs) -> (PathBuf, PathBuf) {
    let (region, load) = dhlrc_core::modules::default_dirs();
    (
        args.region_dir.clone().unwrap_or(region),
        args.load_dir.clone().unwrap_or(load),
    )
}

/// Human readable byte count for the table.
fn bytes_text(bytes: u64) -> String {
    const KILO: f64 = 1024.0;
    let bytes = bytes as f64;
    if bytes >= KILO * KILO {
        format!("{:.1} MB", bytes / KILO / KILO)
    } else if bytes >= KILO {
        format!("{:.1} KB", bytes / KILO)
    } else {
        format!("{bytes:.0} B")
    }
}

fn millis_text(millis: f64) -> String {
    if millis >= 1000.0 {
        format!("{:.2} s", millis / 1000.0)
    } else if millis >= 10.0 {
        format!("{millis:.0} ms")
    } else {
        format!("{millis:.1} ms")
    }
}

/// Appends a JSON string, escaping what JSON requires.
fn json_string(text: &str) -> String {
    let mut escaped = String::with_capacity(text.len() + 2);
    escaped.push('"');
    for character in text.chars() {
        match character {
            '"' => escaped.push_str("\\\""),
            '\\' => escaped.push_str("\\\\"),
            '\n' => escaped.push_str("\\n"),
            '\r' => escaped.push_str("\\r"),
            '\t' => escaped.push_str("\\t"),
            character if (character as u32) < 0x20 => {
                escaped.push_str(&format!("\\u{:04x}", character as u32))
            }
            other => escaped.push(other),
        }
    }
    escaped.push('"');
    escaped
}

fn confidence_text(confidence: Confidence) -> &'static str {
    match confidence {
        Confidence::High => "high",
        Confidence::Medium => "medium",
        Confidence::Low => "low",
    }
}

fn basis_id(basis: Basis) -> &'static str {
    match basis {
        Basis::Exact => "exact",
        Basis::Format => "format",
        Basis::AnyFormat => "any-format",
        Basis::Guess => "guess",
    }
}

/// A short translated phrase naming where the number comes from.
fn basis_text(basis: Basis) -> String {
    match basis {
        Basis::Exact => gettext("measured before"),
        Basis::Format => gettext("from earlier loads of this format"),
        Basis::AnyFormat => gettext("from earlier loads of other formats"),
        Basis::Guess => gettext("no history"),
    }
}

pub fn run(args: &LoadTimeArgs) -> Result<(), String> {
    let mut history = match &args.history {
        Some(path) => History::at(path.clone()),
        None => History::open(&cache_dir()),
    };

    if args.clear {
        history
            .clear()
            .map_err(|error| format!("{}: {error}", history.path().display()))?;
        println!(
            "{}",
            formatx!(
                gettext("forgot every measured load in {}"),
                history.path().display().to_string()
            )
            .unwrap_or_default()
        );
        return Ok(());
    }

    if args.files.is_empty() {
        return Err(gettext("give at least one FILE, or --clear"));
    }

    let (region_dir, load_dir) = module_dirs(args);
    /* Estimating reads the file through the same region modules the load
     * would use, so the numbers come from the format, not from this command. */
    let modules = if args.estimate || args.predict {
        Some(Modules::load(&region_dir, &load_dir))
    } else {
        None
    };
    let mut failures = 0usize;
    let mut entries = Vec::new();

    for file in &args.files {
        let metadata = match fs::metadata(file) {
            Ok(metadata) => metadata,
            Err(error) => {
                eprintln!("{}", format!("{}: {error}", file.display()));
                failures += 1;
                continue;
            }
        };
        let size = metadata.len();
        let modified = modified_millis(&metadata);
        let format = format_for(file);
        let prediction =
            dhlrc_core::loadtime::predict_for_file(&history, file, size, modified, None);
        let estimation = if args.estimate || args.predict {
            Some(estimate(file, size, modules.as_ref()))
        } else {
            None
        };

        let actual = if args.predict || args.estimate {
            None
        } else {
            let start = Instant::now();
            let loaded = region::Region::load(
                file,
                &region::Options {
                    region_dir: &region_dir,
                    load_dir: &load_dir,
                    region_lib: args.region_lib.as_deref(),
                    region: args.region.as_deref(),
                },
            );
            let millis = start.elapsed().as_secs_f64() * 1000.0;
            match loaded {
                Ok(_) => {
                    if !args.no_record {
                        let sample = Sample {
                            path: file.to_string_lossy().into_owned(),
                            size,
                            modified,
                            format: format.clone(),
                            millis,
                        };
                        if let Err(error) = history.record(sample) {
                            eprintln!("{}", format!("{}: {error}", history.path().display()));
                        }
                    }
                    Some(millis)
                }
                Err(error) => {
                    eprintln!("{}", format!("{}: {error}", file.display()));
                    failures += 1;
                    None
                }
            }
        };

        entries.push((file.clone(), size, format, prediction, actual, estimation));
    }

    if args.json {
        let mut objects = Vec::new();
        for (file, size, format, prediction, actual, estimation) in &entries {
            let actual = match actual {
                Some(millis) => format!(
                    ",\"actual_ms\":{millis:.3},\"error_percent\":{:.1}",
                    (millis - prediction.millis) / prediction.millis * 100.0
                ),
                None => String::new(),
            };
            let estimate_json = match estimation {
                Some(estimation) => format!(
                    ",\"unpacked_bytes\":{},\"estimated_millis\":{:.3},                     \"estimated_millis_low\":{:.3},\"estimated_millis_high\":{:.3},                     \"blocks\":{},\"estimated_memory_bytes\":{:.0}",
                    estimation
                        .unpacked
                        .map(|bytes| bytes.to_string())
                        .unwrap_or_else(|| "null".to_owned()),
                    (estimation.millis_low + estimation.millis_high) / 2.0,
                    estimation.millis_low,
                    estimation.millis_high,
                    estimation
                        .region
                        .map(|region| region.blocks.to_string())
                        .unwrap_or_else(|| "null".to_owned()),
                    estimation.memory_bytes,
                ),
                None => String::new(),
            };
            objects.push(format!(
                "{{\"file\":{},\"format\":{},\"bytes\":{size},\
                 \"predicted_ms\":{:.3},\"low_ms\":{:.3},\"high_ms\":{:.3},\
                 \"basis\":\"{}\",\"confidence\":\"{}\",\"samples\":{}{income}{actual}}}",
                json_string(&file.to_string_lossy()),
                json_string(format),
                prediction.millis,
                prediction.low,
                prediction.high,
                basis_id(prediction.basis),
                confidence_text(prediction.confidence),
                prediction.samples,
                income = estimate_json,
            ));
        }
        println!("[{}]", objects.join(","));
    } else {
        for (file, size, format, prediction, actual, estimation) in &entries {
            /* An estimate is about what the headers say, not about the size
             * history, so it replaces the measured-load headline. */
            let from_region = args.estimate
                && estimation
                    .and_then(|estimation| estimation.region)
                    .is_some();
            let mut line = if from_region {
                format!("{}  {}  {}", file.display(), bytes_text(*size), format)
            } else {
                format!(
                    "{}  {}  {}  {}",
                    file.display(),
                    bytes_text(*size),
                    format,
                    formatx!(gettext("predicted {}"), millis_text(prediction.millis))
                        .unwrap_or_default()
                )
            };
            if !from_region {
                line.push_str(&format!("  ({})", basis_text(prediction.basis)));
            }
            if !from_region && prediction.samples > 0 {
                let text = if prediction.samples == 1 {
                    gettext("{} sample")
                } else {
                    gettext("{} samples")
                };
                line.push_str(&format!(
                    "  {}",
                    formatx!(text, prediction.samples.to_string()).unwrap_or_default()
                ));
            }
            match actual {
                Some(millis) if !from_region => {
                    let error = (millis - prediction.millis) / prediction.millis * 100.0;
                    line.push_str(&format!(
                        "  {}  {}",
                        formatx!(gettext("measured {}"), millis_text(*millis)).unwrap_or_default(),
                        formatx!(gettext("error {:+.0}%"), error).unwrap_or_default()
                    ));
                }
                None if prediction.basis != Basis::Exact && !from_region => {
                    line.push_str(&format!(
                        "  {}",
                        formatx!(
                            gettext("usual range {} to {}"),
                            millis_text(prediction.low),
                            millis_text(prediction.high)
                        )
                        .unwrap_or_default()
                    ))
                }
                Some(_) | None => {}
            }
            println!("{line}");
            if let Some(estimation) = estimation {
                if let Some(region) = estimation.region {
                    let line = if region.regions > 1 {
                        formatx!(
                            gettext("{} regions, {} blocks in their volumes"),
                            region.regions.to_string(),
                            region.blocks.to_string()
                        )
                        .unwrap_or_default()
                    } else if region.blocks_exact {
                        let filled = if region.volume() > 0 {
                            region.blocks as f64 / region.volume() as f64 * 100.0
                        } else {
                            0.0
                        };
                        formatx!(
                            gettext("region {}: {} blocks, {} filled"),
                            region.extent_text(),
                            region.blocks.to_string(),
                            format!("{filled:.0}%")
                        )
                        .unwrap_or_default()
                    } else {
                        formatx!(
                            gettext("region {}: volume of {} blocks"),
                            region.extent_text(),
                            region.blocks.to_string()
                        )
                        .unwrap_or_default()
                    };
                    println!("    {line}");
                    println!(
                        "    {}",
                        formatx!(
                            gettext("likely load {} to {} (from the block count)"),
                            millis_text(estimation.millis_low),
                            millis_text(estimation.millis_high)
                        )
                        .unwrap_or_default()
                    );
                }
                let unpacked = match estimation.unpacked {
                    Some(bytes) => {
                        formatx!(gettext("unpacked {}"), bytes_text(bytes)).unwrap_or_default()
                    }
                    None => gettext("unpacked size could not be read"),
                };
                println!(
                    "    {}    {}",
                    unpacked,
                    formatx!(
                        gettext("memory about {} (between {} and {}, a guess)"),
                        bytes_text(estimation.memory_bytes.round() as u64),
                        bytes_text(estimation.memory_low.round() as u64),
                        bytes_text(estimation.memory_high.round() as u64)
                    )
                    .unwrap_or_default()
                );
            }
        }
    }

    if failures > 0 {
        Err(formatx!(
            gettext("{} file(s) could not be loaded"),
            failures.to_string()
        )
        .unwrap_or_default())
    } else {
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;

    #[test]
    fn a_gzip_file_reports_its_unpacked_size() {
        let directory = std::env::temp_dir().join(format!("dhlrc-unpacked-{}", std::process::id()));
        let _ = fs::create_dir_all(&directory);
        let path = directory.join("probe.nbt");
        let payload = vec![7u8; 300_000];
        {
            let file = fs::File::create(&path).unwrap();
            let mut encoder = flate2::write::GzEncoder::new(file, flate2::Compression::fast());
            encoder.write_all(&payload).unwrap();
            encoder.finish().unwrap();
        }
        assert_eq!(unpacked_size(&path), Some(payload.len() as u64));
        assert_eq!(unpacked_size(&directory.join("missing.nbt")), None);
        let _ = fs::remove_dir_all(&directory);
    }

    #[test]
    fn a_module_estimate_replaces_the_size_guess() {
        let region = RegionSize {
            extent: [10, 7, 52],
            blocks: 1431,
            blocks_exact: true,
            regions: 1,
        };
        let estimation = estimate_with(500, Some(400_000), Some(region));
        let region = estimation.region.expect("the module estimate was lost");
        assert_eq!(region.extent, [10, 7, 52]);
        assert_eq!(region.blocks, 1431);
        assert!(region.blocks_exact);
        assert_eq!(region.regions, 1);
        assert!(estimation.millis_high > estimation.millis_low);
        assert!(estimation.memory_bytes >= dhlrc_core::loadtime::GUESS_MEMORY_FIXED_BYTES);
    }

    #[test]
    fn a_file_no_module_claims_falls_back_to_its_size() {
        let estimation = estimate_with(500, None, None);
        assert!(estimation.region.is_none());
        assert!(estimation.millis_low > 0.0);
        assert!(estimation.memory_bytes >= dhlrc_core::loadtime::GUESS_MEMORY_FIXED_BYTES);
    }

    #[test]
    fn a_module_estimate_is_turned_into_a_region_size() {
        let region = RegionSize::from_module(dhlrc_core::ffi::DhlrcEstimate {
            regions: 3,
            blocks: 27,
            uncompressed_bytes: 4096,
            size_x: 3,
            size_y: 3,
            size_z: 3,
            blocks_exact: 0,
            ok: 1,
        })
        .expect("a positive block count is an estimate");
        assert_eq!(region.regions, 3);
        assert_eq!(region.blocks, 27);
        assert!(!region.blocks_exact);
        assert_eq!(region.extent_text(), "3 x 3 x 3");
        assert!(RegionSize::from_module(dhlrc_core::ffi::DhlrcEstimate {
            blocks: -1,
            ..dhlrc_core::ffi::DhlrcEstimate::unknown()
        })
        .is_none());
    }

    #[test]
    fn the_memory_estimate_scales_with_the_unpacked_size() {
        let small = estimate_with(1_000, Some(1_000_000), None);
        let large = estimate_with(1_000, Some(10_000_000), None);
        assert!(large.memory_bytes > small.memory_bytes);
        assert!(small.memory_bytes >= dhlrc_core::loadtime::GUESS_MEMORY_FIXED_BYTES);
        assert!(small.memory_low < small.memory_bytes);
        assert!(small.memory_high > small.memory_bytes);
    }

    #[test]
    fn byte_and_time_texts_stay_readable() {
        assert_eq!(bytes_text(999), "999 B");
        assert_eq!(bytes_text(2_048), "2.0 KB");
        assert_eq!(bytes_text(3 * 1024 * 1024), "3.0 MB");
        assert_eq!(millis_text(4.25), "4.2 ms");
        assert_eq!(millis_text(42.4), "42 ms");
        assert_eq!(millis_text(1_500.0), "1.50 s");
    }
}
