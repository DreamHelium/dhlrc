/*! The command line, described once with `clap`.
 *
 * Every argument lives here rather than in `main`, so the parser, the `--help`
 * text and the completion script all come from the same description. Adding a
 * command is a variant here plus a `run` function to dispatch to.
 */

use clap::{Args, Parser, Subcommand, ValueEnum};
use std::path::PathBuf;

#[derive(Debug, Parser)]
#[command(
    name = "dhlrc-tui",
    version,
    about = "Open, inspect and modify Minecraft structure files (terminal frontend)",
    long_about = None
)]
pub struct Cli {
    #[command(subcommand)]
    pub command: Option<Command>,
}

#[derive(Debug, Subcommand)]
pub enum Command {
    /// Analyse the modules installed next to the executable.
    Modules(ModulesArgs),
    /// Print a shell completion script.
    Completions(CompletionsArgs),
}

/// Which modules `modules` reports on.
#[derive(Debug, Clone, Copy, PartialEq, Eq, ValueEnum)]
pub enum Kind {
    /// Both region modules and object codecs.
    All,
    /// Only the region modules (`region_module/`).
    Region,
    /// Only the object codecs (`load_module/`).
    Load,
}

#[derive(Debug, Args)]
pub struct ModulesArgs {
    /// Directory holding the region modules (default: `region_module/` next to
    /// the executable).
    #[arg(long, value_name = "DIR")]
    pub region_dir: Option<PathBuf>,

    /// Directory holding the object codecs (default: `load_module/` next to the
    /// executable).
    #[arg(long, value_name = "DIR")]
    pub load_dir: Option<PathBuf>,

    /// Which modules to report.
    #[arg(long, value_enum, default_value = "all")]
    pub kind: Kind,

    /// Also print the path of every library.
    #[arg(short, long)]
    pub verbose: bool,
}

#[derive(Debug, Args)]
pub struct CompletionsArgs {
    /// The shell to emit a completion script for.
    #[arg(value_enum)]
    pub shell: clap_complete::Shell,

    /// Install into this directory instead of the shell's own completion
    /// directory. The shell's conventional file name is used inside it.
    #[arg(short, long, value_name = "DIR")]
    pub output: Option<PathBuf>,

    /// Print the script to standard output instead of installing it.
    #[arg(long, conflicts_with = "output")]
    pub print: bool,
}
