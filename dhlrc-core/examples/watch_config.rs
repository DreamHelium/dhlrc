/*! Demonstrates watching the configuration file.
 *
 * Run it, then edit the printed file in another window; every change is printed
 * as the core notices it. With a numeric argument it stops after that many
 * seconds (`watch_config 5`), otherwise it runs until interrupted.
 */

use dhlrc_core::{Config, ConfigWatcher, Level, default_paths, load_or_create};
use std::time::Duration;

fn main() {
    let paths = default_paths();
    let init = match load_or_create(&paths) {
        Ok(init) => init,
        Err(error) => {
            eprintln!("could not open the configuration: {error}");
            std::process::exit(1);
        }
    };

    println!("config file: {}", paths.file.display());
    // A frontend shows these; here they are just printed.
    for note in init.notifications() {
        println!(
            "[{}] {} - {}",
            level_name(note.level),
            note.title,
            note.text
        );
    }
    println!(
        "initial elapsed_milliseconds = {}",
        init.config.general.elapsed_milliseconds
    );
    println!("edit the file to see changes.");

    // The core watches on its own; `_watch` keeps the thread alive, and dropping
    // it (at the end of `main`) stops it.
    let _watch = ConfigWatcher::new(&init)
        .spawn(|config: Config| {
            println!(
                "changed: elapsed_milliseconds = {}",
                config.general.elapsed_milliseconds
            );
        })
        .expect("could not start watching");

    match std::env::args()
        .nth(1)
        .and_then(|arg| arg.parse::<u64>().ok())
    {
        Some(seconds) => std::thread::sleep(Duration::from_secs(seconds)),
        None => loop {
            std::thread::sleep(Duration::from_secs(60));
        },
    }
}

fn level_name(level: Level) -> &'static str {
    match level {
        Level::Info => "info",
        Level::Warning => "warning",
        Level::Error => "error",
    }
}
