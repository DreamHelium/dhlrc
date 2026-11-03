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
mod modules;
mod region;
mod render;
mod ui;

use cli::Command;
use std::io::IsTerminal;
use std::process::ExitCode;

/// Opens the interactive settings page and reports why it could not when the
/// terminal refuses to cooperate.
fn interactive() -> ExitCode {
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
    let mut app = ui::App::new(init);
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
        Some(Command::Settings) => interactive(),
        None => {
            if std::io::stdin().is_terminal() && std::io::stdout().is_terminal() {
                interactive()
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
