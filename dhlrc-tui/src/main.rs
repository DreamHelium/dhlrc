/*! The dhlrc terminal frontend (experimental).
 *
 * This crate is the beginning of a Qt-free frontend, so that dhlrc can run
 * where there is no GUI. Only the command-line shell exists so far:
 *
 * - argument parsing (`cli`),
 * - the interactive settings page (`ui`), which runs on the terminal directly
 *   with the mouse or a touchscreen, reached by `settings` or by running with
 *   no command on a terminal,
 * - the `modules` command, which reports what the core can load and whether the
 *   region modules have the object codecs they need (`modules`),
 * - the `render` command, which loads a structure file headlessly and writes a
 *   rendered PNG (`render`, over `region`),
 * - the `loadtime` command, which measures real loads and predicts them from
 *   the measured history (`loadtime`),
 * - shell completion (`completions`),
 * - the shared gettext setup (`i18n`).
 *
 * Running with no command on a terminal opens the interactive UI; without a
 * terminal it prints the help instead.
 */

mod cli;
mod completions;
mod i18n;
mod loadtime;
mod logfile;
mod modules;
mod region;
mod render;
mod ui;

use cli::Command;
use std::io::IsTerminal;
use std::process::ExitCode;

/// The level the log file keeps: the command-line option first, then
/// `DHLRC_LOG_LEVEL` (the same variable `dhlrc_qt` uses), then info.
fn log_level(cli: &cli::Cli) -> dhlrc_core::log::Level {
    cli.log_level
        .as_deref()
        .and_then(dhlrc_core::log::Level::from_name)
        .or_else(|| {
            std::env::var("DHLRC_LOG_LEVEL")
                .ok()
                .and_then(|name| dhlrc_core::log::Level::from_name(&name))
        })
        .unwrap_or(dhlrc_core::log::Level::Info)
}

/// Opens the interactive settings page and reports why it could not when the
/// terminal refuses to cooperate.
fn interactive(level: dhlrc_core::log::Level) -> ExitCode {
    let init = match dhlrc_core::load_or_create(&dhlrc_core::default_paths()) {
        Ok(init) => init,
        Err(error) => {
            eprintln!(
                "{}: {error}",
                i18n::gettext("Could not open the configuration")
            );
            return ExitCode::FAILURE;
        }
    };
    /* The configured cache directory may differ from the default the first
     * records used; from here on the log lands where `dhlrc_qt` would put it. */
    let _ = logfile::install(&init.config.general.cache_dir(), level);
    let notifications = init.notifications();
    let mut app = ui::App::new(init);
    /* The same notifications the Qt frontend shows, with the same wording. */
    app.show_notifications(notifications);
    match ui::term::run(&mut app) {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!(
                "{}: {error}",
                i18n::gettext("The terminal UI could not run")
            );
            ExitCode::FAILURE
        }
    }
}

fn main() -> ExitCode {
    i18n::init();

    let cli = cli::parse();
    let level = log_level(&cli);
    if let Err(error) = logfile::install(&dhlrc_core::Config::default().general.cache_dir(), level) {
        eprintln!("{}: {error}", i18n::gettext("Could not open the log file"));
    }
    dhlrc_core::log_info!("dhlrc-tui {} started", env!("DHLRC_BUILD_STAMP"));
    match cli.command {
        Some(Command::Modules(args)) => modules::run(args),
        Some(Command::Render(args)) => render::run(args),
        Some(Command::LoadTime(args)) => match loadtime::run(&args) {
            Ok(()) => ExitCode::SUCCESS,
            Err(error) => {
                eprintln!("{error}");
                ExitCode::FAILURE
            }
        },
        Some(Command::Completions(args)) => completions::run(args),
        Some(Command::Settings) => interactive(level),
        None => {
            if std::io::stdin().is_terminal() && std::io::stdout().is_terminal() {
                interactive(level)
            } else {
                /* Not a terminal (a pipe, a script): the page needs a real
                 * terminal to read keys and place the cursor, so the help is
                 * the useful answer here. */
                let mut command = cli::command();
                let _ = command.print_help();
                println!();
                println!(
                    "{}",
                    i18n::gettext(
                        "The interactive UI needs a terminal; \
                         pick one of the commands above."
                    )
                );
                ExitCode::SUCCESS
            }
        }
    }
}
