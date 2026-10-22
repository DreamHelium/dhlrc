/*! The dhlrc terminal frontend (experimental).
 *
 * This crate is the beginning of a Qt-free frontend, so that dhlrc can run
 * where there is no GUI. Only the command-line shell exists so far:
 *
 * - argument parsing (`cli`),
 * - the `modules` command, which reports what the core can load and whether the
 *   region modules have the object codecs they need (`modules`),
 * - the `render` command, which loads a structure file headlessly and writes a
 *   rendered PNG (`render`, over `region`),
 * - shell completion (`completions`),
 * - the shared gettext setup (`i18n`).
 *
 * The interactive terminal UI itself is not written yet; running with no
 * command prints the help instead. The argument parser is written first on
 * purpose, so the eventual UI can be reached from the same subcommand tree
 * these commands live in.
 */

mod cli;
mod completions;
mod i18n;
mod modules;
mod region;
mod render;

use cli::Command;
use std::process::ExitCode;

fn main() -> ExitCode {
    i18n::init();

    let cli = cli::parse();
    match cli.command {
        Some(Command::Modules(args)) => modules::run(args),
        Some(Command::Render(args)) => render::run(args),
        Some(Command::Completions(args)) => completions::run(args),
        None => {
            /* The interactive UI will take this branch once it exists. Until
             * then, the help tells the user what is available. */
            let mut command = cli::command();
            let _ = command.print_help();
            println!();
            println!(
                "{}",
                i18n::gettext(
                    "The interactive terminal UI is not written yet; \
                     pick one of the commands above."
                )
            );
            ExitCode::SUCCESS
        }
    }
}
