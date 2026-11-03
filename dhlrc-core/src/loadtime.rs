/*! Measuring and predicting how long a region file takes to load.
 *
 * There is no metadata-only way into a region file: the only honest number for
 * a file is the time a real load takes. This module therefore records every
 * measured load in a small history and answers a new file by extrapolating
 * from it. A file measured before (same path, size and modification time) is
 * answered exactly, which is also the cache question the history exists for.
 *
 * The history is a tab separated file in the cache directory, one measured
 * load per line, so it can be read and edited by hand. It is a cache, not a
 * setting: deleting it only loses the predictions' confidence.
 *
 * The same code serves every frontend: the terminal command reads and records
 * the history directly, the C interface asks for a prediction without touching
 * the file. The module estimate is only used when the history has nothing to
 * say, so a measured load always wins.
 */

use common_rs::estimate::DhlrcEstimate;
use std::fs;
use std::io;
use std::path::{Path, PathBuf};
use std::time::UNIX_EPOCH;

/// The first line of the history file; a file with another first line is
/// ignored rather than misread.
pub const HEADER: &str = "dhlrc-load-times 1";

/// How many measured loads the history keeps. The oldest one is dropped, so a
/// long lived installation still starts up quickly.
pub const CAPACITY: usize = 200;

/// What a file with no history at all is predicted with: a fixed cost for
/// opening the plugins and the file, plus a per compressed megabyte rate. Both
/// are deliberately pessimistic rather than optimistic, and the prediction
/// says it is a guess.
pub const GUESS_FIXED_MILLIS: f64 = 3.0;
pub const GUESS_MILLIS_PER_MEGABYTE: f64 = 80.0;

/// The memory model behind `--estimate`: the process's own cost plus a factor
/// over the unpacked size. A region keeps the parsed block states, the palette
/// and the render models alive, so its footprint is several times the NBT it
/// came from; the factor is deliberately pessimistic. The output calls this a
/// guess because it is one.
pub const GUESS_MEMORY_FIXED_BYTES: f64 = 48.0 * 1024.0 * 1024.0;
pub const GUESS_MEMORY_BYTES_PER_UNPACKED: f64 = 6.0;

/// How far the memory estimate may be off, as a fraction of the estimate.
pub const GUESS_MEMORY_SPREAD: f64 = 0.4;

/// How long one block takes to turn into its model, in nanoseconds. Measured
/// with the terminal frontend on small regions; the Qt frontend builds more
/// objects per block, so the range below is wide on purpose.
pub const GUESS_NANOS_PER_BLOCK: f64 = 1_500.0;

/// The part of a load that does not depend on the block count (opening the
/// file, reading the palette and the module headers).
pub const GUESS_FIXED_REGION_MILLIS: f64 = 4.0;

/// How much a region's blocks take in memory, in bytes per block, on top of
/// the fixed cost below. Measured roughly against the Qt region model.
pub const GUESS_MEMORY_BYTES_PER_BLOCK: f64 = 420.0;

/// How many unpacked bytes a set block takes, at most, so a sparse region's
/// volume is not counted as fully filled when only its memory is estimated.
pub const GUESS_UNPACKED_BYTES_PER_BLOCK: f64 = 2.0;

/// The spread of the region-based time and memory estimates.
pub const GUESS_REGION_SPREAD: f64 = 0.5;

/// One measured load.
#[derive(Debug, Clone, PartialEq)]
pub struct Sample {
    /// The file as the user named it, so the same file is recognised again.
    pub path: String,
    pub size: u64,
    /// Modification time in milliseconds since the epoch; a file that changed
    /// is a different load.
    pub modified: u64,
    /// The file extension, lower case, without the dot.
    pub format: String,
    pub millis: f64,
}

/// Where a prediction comes from, strongest first.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Basis {
    /// The same file, unchanged, was measured before.
    Exact,
    /// Other loads of the same format were used.
    Format,
    /// Loads of every format were used.
    AnyFormat,
    /// No history; the built-in rate was used.
    Guess,
}

impl Basis {
    /// The number the C interface reports (`DHLRC_LOAD_BASIS_*` in the
    /// header); `Guess` is also what a module-provided estimate reports.
    pub fn to_int(self) -> i32 {
        match self {
            Basis::Exact => 0,
            Basis::Format => 1,
            Basis::AnyFormat => 2,
            Basis::Guess => 3,
        }
    }
}

/// How much the history backs a prediction.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Confidence {
    High,
    Medium,
    Low,
}

/// What the history says about one file.
#[derive(Debug, Clone, PartialEq)]
pub struct Prediction {
    pub millis: f64,
    /// The range to expect the load in; equal to `millis` for an exact hit.
    pub low: f64,
    pub high: f64,
    pub basis: Basis,
    pub confidence: Confidence,
    /// How many samples the prediction was made from.
    pub samples: usize,
}

/// What the memory model says about one file.
#[derive(Debug, Clone, PartialEq)]
pub struct MemoryGuess {
    pub bytes: f64,
    pub low: f64,
    pub high: f64,
}

/// The measured loads, and where they are kept.
#[derive(Debug, Clone)]
pub struct History {
    path: PathBuf,
    samples: Vec<Sample>,
}

impl History {
    /// Reads the history at `path`; a missing or unreadable file is an empty
    /// history, because this is a cache and must never stop the caller.
    pub fn at(path: PathBuf) -> Self {
        let samples = fs::read_to_string(&path)
            .ok()
            .map(|text| parse(&text))
            .unwrap_or_default();
        Self { path, samples }
    }

    /// The history in `cache_dir`.
    pub fn open(cache_dir: &Path) -> Self {
        Self::at(cache_dir.join("load-times.tsv"))
    }

    pub fn path(&self) -> &Path {
        &self.path
    }

    pub fn samples(&self) -> &[Sample] {
        &self.samples
    }

    /// Records `sample`, replacing an earlier measurement of the same file and
    /// dropping the oldest entry when the history is full.
    pub fn record(&mut self, sample: Sample) -> io::Result<()> {
        self.samples.retain(|old| old.path != sample.path);
        self.samples.push(sample);
        if self.samples.len() > CAPACITY {
            let excess = self.samples.len() - CAPACITY;
            self.samples.drain(..excess);
        }
        self.save()
    }

    /// Forgets every measured load.
    pub fn clear(&mut self) -> io::Result<()> {
        self.samples.clear();
        self.save()
    }

    fn save(&self) -> io::Result<()> {
        if let Some(parent) = self.path.parent() {
            fs::create_dir_all(parent)?;
        }
        let mut text = String::from(HEADER);
        text.push('\n');
        for sample in &self.samples {
            text.push_str(&format_sample(sample));
            text.push('\n');
        }
        fs::write(&self.path, text)
    }
}

/// The extension of `path`, lower case and without the dot, or `-` when the
/// file has none.
pub fn format_for(path: &Path) -> String {
    path.extension()
        .and_then(|extension| extension.to_str())
        .map(|extension| extension.to_lowercase())
        .unwrap_or_else(|| "-".to_owned())
}

/// The file's modification time in milliseconds since the epoch, or 0 when the
/// platform cannot tell.
pub fn modified_millis(metadata: &fs::Metadata) -> u64 {
    metadata
        .modified()
        .ok()
        .and_then(|time| time.duration_since(UNIX_EPOCH).ok())
        .map(|duration| duration.as_millis() as u64)
        .unwrap_or(0)
}

/// Predicts the load time of a file of `size` bytes.
///
/// An exact history entry wins. Otherwise the loads of the same format are
/// fitted with a line `time = fixed + rate * size`; when there are too few
/// distinct sizes for a line, the median rate of the format is scaled by the
/// size instead. A format with no history borrows the fit of every format; with
/// no history at all, the built-in guess is used, marked as such.
pub fn predict(samples: &[Sample], path: &Path, size: u64, modified: u64) -> Prediction {
    let name = path.to_string_lossy();
    if let Some(exact) = samples.iter().find(|sample| {
        sample.path == name.as_ref() && sample.size == size && sample.modified == modified
    }) {
        return Prediction {
            millis: exact.millis,
            low: exact.millis,
            high: exact.millis,
            basis: Basis::Exact,
            confidence: Confidence::High,
            samples: 1,
        };
    }

    let format = format_for(path);
    let same: Vec<&Sample> = samples
        .iter()
        .filter(|sample| sample.format == format)
        .collect();
    if let Some(prediction) = extrapolate(&same, size, Basis::Format) {
        return prediction;
    }
    let all: Vec<&Sample> = samples.iter().collect();
    if let Some(prediction) = extrapolate(&all, size, Basis::AnyFormat) {
        return prediction;
    }

    let millis = GUESS_FIXED_MILLIS + size as f64 / 1_000_000.0 * GUESS_MILLIS_PER_MEGABYTE;
    Prediction {
        millis,
        low: millis * 0.5,
        high: millis * 2.0,
        basis: Basis::Guess,
        confidence: Confidence::Low,
        samples: 0,
    }
}

/// Predicts a file's load time from `history`, falling back to a region
/// module's estimate when the history has nothing to say. Every frontend uses
/// this entry point; `estimate` is `None` when no module is loaded.
pub fn predict_for_file(
    history: &History,
    path: &Path,
    size: u64,
    modified: u64,
    estimate: Option<&DhlrcEstimate>,
) -> Prediction {
    predict_with_estimate(history.samples(), path, size, modified, estimate)
}

/// Predicts the load time, falling back to a region module's estimate of the
/// block count when the history has nothing to say about the file.
///
/// A measured load (or a fit from other loads) always wins: the estimate only
/// replaces the size-based guess, because a well-filled region can be a tenth
/// the compressed size of a sparse one with the same dimensions. `basis` stays
/// `Guess` for the estimate, since no load of this file was measured.
pub fn predict_with_estimate(
    samples: &[Sample],
    path: &Path,
    size: u64,
    modified: u64,
    estimate: Option<&DhlrcEstimate>,
) -> Prediction {
    let prediction = predict(samples, path, size, modified);
    if prediction.basis != Basis::Guess {
        return prediction;
    }
    estimate.and_then(guess_from_region).unwrap_or(prediction)
}

/// A load-time guess implied by a region module's block count, using the same
/// model as the terminal frontend's `--estimate`.
fn guess_from_region(estimate: &DhlrcEstimate) -> Option<Prediction> {
    if estimate.blocks <= 0 {
        return None;
    }
    let millis =
        GUESS_FIXED_REGION_MILLIS + estimate.blocks as f64 * GUESS_NANOS_PER_BLOCK / 1_000_000.0;
    Some(Prediction {
        millis,
        low: millis * (1.0 - GUESS_REGION_SPREAD),
        high: millis * (1.0 + GUESS_REGION_SPREAD),
        basis: Basis::Guess,
        confidence: Confidence::Low,
        samples: 0,
    })
}

/// The memory model behind `--estimate`: the process's own cost plus a factor
/// per live block, or per unpacked byte when no module counted blocks. A
/// module's `blocks_exact` flag decides whether the reported block count is
/// the number of existing blocks or only the region's volume, in which case
/// the unpacked data caps it. `unpacked` is `None` when it could not be read.
pub fn memory_guess(
    size: u64,
    unpacked: Option<u64>,
    estimate: Option<&DhlrcEstimate>,
) -> MemoryGuess {
    if let Some(estimate) = estimate.filter(|estimate| estimate.blocks > 0) {
        let blocks = if estimate.blocks_exact != 0 {
            estimate.blocks as f64
        } else {
            let described = unpacked.unwrap_or(size) as f64 / GUESS_UNPACKED_BYTES_PER_BLOCK;
            (estimate.blocks as f64).min(described).max(1.0)
        };
        let bytes = GUESS_MEMORY_FIXED_BYTES + blocks * GUESS_MEMORY_BYTES_PER_BLOCK;
        return MemoryGuess {
            bytes,
            low: bytes * (1.0 - GUESS_REGION_SPREAD),
            high: bytes * (1.0 + GUESS_REGION_SPREAD),
        };
    }
    let bytes = GUESS_MEMORY_FIXED_BYTES
        + GUESS_MEMORY_BYTES_PER_UNPACKED * unpacked.unwrap_or(size) as f64;
    MemoryGuess {
        bytes,
        low: bytes * (1.0 - GUESS_MEMORY_SPREAD),
        high: bytes * (1.0 + GUESS_MEMORY_SPREAD),
    }
}

/// Extrapolates from `group` to a file of `size` bytes.
fn extrapolate(group: &[&Sample], size: u64, basis: Basis) -> Option<Prediction> {
    /* A zero measured time says nothing useful about the rate; one bad line
     * must not poison the fit. */
    let usable: Vec<&&Sample> = group.iter().filter(|sample| sample.millis > 0.0).collect();
    let first = usable.first()?;

    let smallest = usable.iter().map(|sample| sample.size).min()?;
    let largest = usable.iter().map(|sample| sample.size).max()?;
    let varying = largest > smallest && (largest - smallest) as f64 > smallest as f64 * 0.1;

    let (millis, confidence) = if varying {
        /* `time = a + b * size` through least squares, clamped to a sensible
         * line: a negative fixed cost or rate would predict nonsense. */
        let n = usable.len() as f64;
        let mean_x = usable.iter().map(|s| s.size as f64).sum::<f64>() / n;
        let mean_y = usable.iter().map(|s| s.millis).sum::<f64>() / n;
        let mut covariance = 0.0;
        let mut variance = 0.0;
        for sample in &usable {
            covariance += (sample.size as f64 - mean_x) * (sample.millis - mean_y);
            variance += (sample.size as f64 - mean_x).powi(2);
        }
        if variance > 0.0 {
            let rate: f64 = (covariance / variance).max(0.0);
            let fixed: f64 = (mean_y - rate * mean_x).max(0.0);
            let predicted = fixed + rate * size as f64;
            let confidence = if usable.len() >= 3 {
                Confidence::Medium
            } else {
                Confidence::Low
            };
            (predicted, confidence)
        } else {
            let rate = first.millis / first.size.max(1) as f64;
            (rate * size as f64, Confidence::Low)
        }
    } else {
        /* Every measurement has about the same size: only the rate is known,
         * so scale the nearest measurement instead of claiming a fixed cost. */
        let nearest = usable
            .iter()
            .min_by_key(|sample| sample.size.abs_diff(size))?;
        let rate = nearest.millis / nearest.size.max(1) as f64;
        (rate * size as f64, Confidence::Low)
    };

    let spread = (millis * 0.25).max(2.0);
    Some(Prediction {
        millis,
        low: (millis - spread).max(0.0),
        high: millis + spread,
        basis,
        confidence,
        samples: usable.len(),
    })
}

/// The history line for one sample, with tabs and newlines in the path escaped.
pub fn format_sample(sample: &Sample) -> String {
    format!(
        "{}\t{}\t{}\t{:.3}\t{}",
        sample.format,
        sample.size,
        sample.modified,
        sample.millis,
        escape(&sample.path)
    )
}

fn escape(text: &str) -> String {
    let mut escaped = String::with_capacity(text.len());
    for character in text.chars() {
        match character {
            '\\' => escaped.push_str("\\\\"),
            '\t' => escaped.push_str("\\t"),
            '\n' => escaped.push_str("\\n"),
            other => escaped.push(other),
        }
    }
    escaped
}

fn unescape(text: &str) -> String {
    let mut plain = String::with_capacity(text.len());
    let mut characters = text.chars();
    while let Some(character) = characters.next() {
        if character != '\\' {
            plain.push(character);
            continue;
        }
        match characters.next() {
            Some('t') => plain.push('\t'),
            Some('n') => plain.push('\n'),
            Some('\\') => plain.push('\\'),
            Some(other) => plain.push(other),
            None => plain.push('\\'),
        }
    }
    plain
}

/// Reads a history file; lines that do not parse are skipped rather than
/// failing the whole history.
pub fn parse(text: &str) -> Vec<Sample> {
    let mut samples = Vec::new();
    let mut lines = text.lines();
    if lines.next() != Some(HEADER) {
        return samples;
    }
    for line in lines {
        let mut fields = line.splitn(5, '\t');
        let (Some(format), Some(size), Some(modified), Some(millis), Some(path)) = (
            fields.next(),
            fields.next(),
            fields.next(),
            fields.next(),
            fields.next(),
        ) else {
            continue;
        };
        let (Ok(size), Ok(modified), Ok(millis)) = (
            size.parse::<u64>(),
            modified.parse::<u64>(),
            millis.parse::<f64>(),
        ) else {
            continue;
        };
        samples.push(Sample {
            path: unescape(path),
            size,
            modified,
            format: format.to_owned(),
            millis,
        });
    }
    samples
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::SystemTime;

    fn sample(path: &str, size: u64, millis: f64) -> Sample {
        Sample {
            path: path.to_owned(),
            size,
            modified: 7,
            format: format_for(Path::new(path)),
            millis,
        }
    }

    #[test]
    fn an_unchanged_file_is_answered_from_its_own_measurement() {
        let samples = vec![sample("a.litematic", 1_000, 12.0)];
        let prediction = predict(&samples, Path::new("a.litematic"), 1_000, 7);
        assert_eq!(prediction.basis, Basis::Exact);
        assert_eq!(prediction.confidence, Confidence::High);
        assert_eq!(prediction.millis, 12.0);
    }

    #[test]
    fn a_changed_file_is_not_an_exact_hit() {
        let samples = vec![sample("a.litematic", 1_000, 12.0)];
        let prediction = predict(&samples, Path::new("a.litematic"), 1_000, 8);
        assert_ne!(prediction.basis, Basis::Exact);
    }

    #[test]
    fn two_different_sizes_give_a_line_through_them() {
        /* 10 ms at 1 KB and 30 ms at 2 KB: 10 ms fixed and 10 ms/KB. */
        let samples = vec![
            sample("a.litematic", 1_000, 20.0),
            sample("b.litematic", 2_000, 30.0),
        ];
        let prediction = predict(&samples, Path::new("c.litematic"), 3_000, 1);
        assert_eq!(prediction.basis, Basis::Format);
        assert!((prediction.millis - 40.0).abs() < 0.01, "{prediction:?}");
        assert_eq!(prediction.samples, 2);
    }

    #[test]
    fn one_measurement_scales_with_the_size() {
        let samples = vec![sample("a.litematic", 1_000, 10.0)];
        let prediction = predict(&samples, Path::new("b.litematic"), 4_000, 1);
        assert_eq!(prediction.basis, Basis::Format);
        assert_eq!(prediction.millis, 40.0);
        assert_eq!(prediction.confidence, Confidence::Low);
    }

    #[test]
    fn another_format_still_uses_the_history() {
        let samples = vec![
            sample("a.litematic", 1_000, 20.0),
            sample("b.litematic", 2_000, 30.0),
        ];
        let prediction = predict(&samples, Path::new("c.schem"), 3_000, 1);
        assert_eq!(prediction.basis, Basis::AnyFormat);
    }

    #[test]
    fn no_history_is_a_guess() {
        let prediction = predict(&[], Path::new("a.litematic"), 1_000_000, 1);
        assert!(prediction.millis > 50.0);
        assert_eq!(prediction.basis, Basis::Guess);
        assert_eq!(prediction.confidence, Confidence::Low);
        assert!(prediction.low < prediction.millis && prediction.millis < prediction.high);
    }

    #[test]
    fn a_region_estimate_replaces_the_no_history_guess() {
        let estimate = DhlrcEstimate {
            regions: 1,
            blocks: 1_431,
            uncompressed_bytes: 60_931,
            size_x: 10,
            size_y: 7,
            size_z: 52,
            blocks_exact: 1,
            ok: 1,
        };
        let prediction = predict_with_estimate(&[], Path::new("a.nbt"), 7_259, 1, Some(&estimate));
        assert_eq!(prediction.basis, Basis::Guess);
        assert_eq!(prediction.samples, 0);
        assert!((prediction.millis - 6.1465).abs() < 0.001);
        assert!(prediction.low < prediction.millis && prediction.millis < prediction.high);
    }

    #[test]
    fn measured_history_beats_a_region_estimate() {
        let estimate = DhlrcEstimate {
            regions: 1,
            blocks: 1_431,
            uncompressed_bytes: 60_931,
            size_x: 10,
            size_y: 7,
            size_z: 52,
            blocks_exact: 1,
            ok: 1,
        };
        let samples = vec![sample("a.nbt", 1_000, 1_500.0)];
        let prediction =
            predict_with_estimate(&samples, Path::new("a.nbt"), 1_000, 1, Some(&estimate));
        assert_eq!(prediction.basis, Basis::Format);
        assert_eq!(prediction.millis, 1_500.0);
    }

    #[test]
    fn the_history_round_trips_through_its_file_format() {
        let samples = vec![
            sample("with a space.litematic", 7, 1.5),
            Sample {
                path: "tab\tinside.schem".to_owned(),
                size: 9,
                modified: 3,
                format: "schem".to_owned(),
                millis: 2.25,
            },
        ];
        let text = format!(
            "{HEADER}\n{}{}",
            format_sample(&samples[0]),
            format!("\n{}", format_sample(&samples[1]))
        );
        assert_eq!(parse(&text), samples);
    }

    #[test]
    fn a_foreign_file_is_an_empty_history() {
        assert!(parse("something else\nx\ty\n").is_empty());
        assert!(parse(&format!("{HEADER}\nbroken line\n")).is_empty());
    }

    #[test]
    fn recording_replaces_the_same_path_and_keeps_the_newest_capacity() {
        let directory = std::env::temp_dir().join(format!(
            "dhlrc-loadtime-test-{}-{}",
            std::process::id(),
            SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .map(|duration| duration.as_nanos())
                .unwrap_or_default()
        ));
        let mut history = History::open(&directory);
        history.record(sample("a.litematic", 1, 1.0)).unwrap();
        history.record(sample("b.litematic", 1, 1.0)).unwrap();
        history.record(sample("a.litematic", 2, 2.0)).unwrap();
        assert_eq!(history.samples().len(), 2);
        assert_eq!(history.samples()[1].size, 2);
        for index in 0..CAPACITY + 5 {
            history
                .record(sample(&format!("{index}.litematic"), 1, 1.0))
                .unwrap();
        }
        assert_eq!(history.samples().len(), CAPACITY);
        let read_back = History::open(&directory);
        assert_eq!(read_back.samples(), history.samples());
        let _ = fs::remove_dir_all(&directory);
    }

    fn region_estimate(blocks: i64, exact: bool, unpacked: i64) -> DhlrcEstimate {
        DhlrcEstimate {
            regions: 1,
            blocks,
            uncompressed_bytes: unpacked,
            size_x: 10,
            size_y: 7,
            size_z: 52,
            blocks_exact: i32::from(exact),
            ok: 1,
        }
    }

    #[test]
    fn memory_from_a_region_uses_its_blocks_and_caps_a_sparse_volume() {
        let exact = memory_guess(
            7_000,
            Some(100_000),
            Some(&region_estimate(1_431, true, 100_000)),
        );
        assert_eq!(
            exact.bytes,
            GUESS_MEMORY_FIXED_BYTES + 1_431.0 * GUESS_MEMORY_BYTES_PER_BLOCK
        );
        assert!(exact.low < exact.bytes && exact.bytes < exact.high);

        /* An approximate block count is the region's volume: only 100 KB of
         * unpacked data cannot describe more than 50 blocks. */
        let sparse = memory_guess(7_000, Some(100), Some(&region_estimate(1_431, false, 100)));
        assert_eq!(
            sparse.bytes,
            GUESS_MEMORY_FIXED_BYTES + 50.0 * GUESS_MEMORY_BYTES_PER_BLOCK
        );
    }

    #[test]
    fn memory_without_a_module_scales_with_the_unpacked_size() {
        let small = memory_guess(1_000_000, None, None);
        let large = memory_guess(2_000_000, Some(2_000_000), None);
        assert_eq!(
            small.bytes,
            GUESS_MEMORY_FIXED_BYTES + GUESS_MEMORY_BYTES_PER_UNPACKED * 1_000_000.0
        );
        assert!(large.bytes > small.bytes);
        assert!(small.low < small.bytes && small.bytes < small.high);
    }
}
