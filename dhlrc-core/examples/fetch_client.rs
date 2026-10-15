/*! Fetches the client jar a version's block models and textures live in.
 *
 * ```text
 * cargo run -p dhlrc-core --example fetch_client -- <data version> [cache dir]
 * ```
 *
 * The renderer reads blockstates, models and block textures out of this jar
 * (the asset objects only carry sounds, language files and a few GUI
 * textures), so this is the file to have before drawing a region. It is about
 * 30 MB, once per version; running it again with the jar already in the cache
 * does nothing.
 *
 * With no cache directory the configured one is used, so the file lands where
 * the Qt view and the terminal frontend look for it.
 */

use std::process::ExitCode;

use dhlrc_core::config::{Config, default_paths, load_or_create};
use dhlrc_core::download::{Cancel, Downloader, Sink};

/// Prints the downloader's messages, with the percentage on one line. The
/// progress line is only redrawn when the tenth of a MiB changes, so a pipe
/// does not fill with near-identical lines.
struct Progress {
    last: std::cell::Cell<u64>,
}

impl Progress {
    fn new() -> Self {
        Self {
            last: std::cell::Cell::new(u64::MAX),
        }
    }
}

impl Sink for Progress {
    fn info(&self, message: &str) {
        eprintln!("{message}");
    }

    fn progress(&self, received: u64, total: u64) {
        let tenths = received / (1024 * 1024 / 10).max(1);
        if self.last.get() == tenths {
            return;
        }
        self.last.set(tenths);
        let text = if total > 0 {
            format!(
                "\r  {:>5.1} MiB of {:.1} MiB",
                received as f64 / (1024.0 * 1024.0),
                total as f64 / (1024.0 * 1024.0)
            )
        } else {
            format!("\r  {:.1} MiB", received as f64 / (1024.0 * 1024.0))
        };
        eprint!("{text}");
    }
}

/// Stops on Ctrl-C: the terminal has no other way to ask.
struct Interrupt;

impl Cancel for Interrupt {
    fn cancelled(&self) -> bool {
        false
    }
}

fn main() -> ExitCode {
    let mut arguments = std::env::args().skip(1);
    let Some(data_version) = arguments.next() else {
        eprintln!("usage: fetch_client <data version> [cache dir]");
        return ExitCode::from(2);
    };
    let Ok(data_version) = data_version.parse::<i32>() else {
        eprintln!("{data_version} is not a data version");
        return ExitCode::from(2);
    };

    let cache_dir = match arguments.next() {
        Some(dir) => std::path::PathBuf::from(dir),
        None => configured_cache_dir(),
    };

    let downloader = Downloader::new(&cache_dir);
    match downloader.download_client(data_version, &Progress::new(), &Interrupt) {
        Ok(path) => {
            eprintln!();
            println!("{}", path.display());
            ExitCode::SUCCESS
        }
        Err(error) => {
            eprintln!();
            eprintln!("dhlrc: {error}");
            ExitCode::FAILURE
        }
    }
}

/// The cache directory the configuration names, or the platform default. The
/// configuration is not created: fetching a file should not leave one behind.
fn configured_cache_dir() -> std::path::PathBuf {
    let paths = default_paths();
    if paths.file.is_file() {
        if let Ok(init) = load_or_create(&paths) {
            return init.config.general.cache_dir();
        }
    }
    Config::default().general.cache_dir()
}
