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
    /// Load a structure file and write a rendered PNG.
    Render(RenderArgs),
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

/// What `render` draws and how.
#[derive(Debug, Args)]
pub struct RenderArgs {
    /// The structure file to load and draw.
    #[arg(value_name = "FILE")]
    pub file: PathBuf,

    /// Where to write the PNG (default: `render.png` next to the input).
    #[arg(short, long, value_name = "PATH")]
    pub output: Option<PathBuf>,

    /// The picture's size in pixels.
    #[arg(long, value_name = "WxH", default_value = "960x540")]
    pub size: String,

    /// Degrees around the world's y axis; 0 looks from the south.
    #[arg(long, value_name = "DEG")]
    pub yaw: Option<f32>,

    /// Degrees above the horizon; 90 looks straight down.
    #[arg(long, value_name = "DEG")]
    pub pitch: Option<f32>,

    /// How far the camera stands from the region's middle, in blocks. Without
    /// it the region is framed to fill the picture.
    #[arg(long, value_name = "BLOCKS")]
    pub distance: Option<f32>,

    /// Draw without perspective, so parallel lines stay parallel.
    #[arg(long)]
    pub ortho: bool,

    /// Put the block outline around the block at this region coordinate, the
    /// way the Qt view highlights the block the mouse is over (`X,Y,Z`).
    #[arg(long, value_name = "X,Y,Z")]
    pub highlight: Option<String>,

    /// A flat colour (`rrggbb`, with or without a leading `#`) to put behind
    /// the blocks. Without it the PNG keeps its transparent background.
    #[arg(long, value_name = "RRGGBB")]
    pub background: Option<String>,

    /// Directory holding the region modules (default: `region_module/` next to
    /// the executable).
    #[arg(long, value_name = "DIR")]
    pub region_dir: Option<PathBuf>,

    /// Directory holding the object codecs (default: `load_module/` next to the
    /// executable).
    #[arg(long, value_name = "DIR")]
    pub load_dir: Option<PathBuf>,

    /// The `libregion_rs` library the region modules import `region_new` from.
    /// The default looks next to the executable and beside the region modules.
    #[arg(long, value_name = "PATH")]
    pub region_lib: Option<PathBuf>,

    /// A client jar (or another zip laid out like one) to read blockstates,
    /// models and textures from.
    #[arg(long, value_name = "PATH")]
    pub jar: Option<PathBuf>,

    /// The cache directory holding the client jar and the downloaded asset
    /// objects. Without it the configured cache directory is used.
    #[arg(long, value_name = "DIR")]
    pub resources: Option<PathBuf>,

    /// The version whose client jar under the cache directory to read.
    #[arg(long, value_name = "ID")]
    pub version: Option<String>,

    /// Never fetch a missing client jar, even when the configuration allows it.
    /// A version's block models and textures are only in that jar, so without
    /// one (here or as `--jar`) the picture comes out empty.
    #[arg(long)]
    pub no_download: bool,

    /// Which region of a multi-region file to draw, by index or by name.
    /// Without it the first region is used.
    #[arg(long, value_name = "INDEX|NAME")]
    pub region: Option<String>,
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
