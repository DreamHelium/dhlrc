/*! The command line, described once with `clap`.
 *
 * Every argument lives here rather than in `main`, so the parser, the `--help`
 * text and the completion script all come from the same description. Adding a
 * command is a variant here plus a `run` function to dispatch to.
 *
 * The texts the user reads go through `gettext` like the rest of the frontend,
 * so `--help` follows the configured language. They sit in the small functions
 * below rather than directly in the attributes because `xgettext` only finds
 * `gettext` calls in expression position; `make_po.sh` collects them from here.
 *
 * `clap` itself writes a few strings that have no translation hook (the section
 * headings, the template's "Usage:", the descriptions of `-h` and `-V`, and the
 * `help` subcommand). `localize` therefore rewrites those on the built
 * `Command`; that is why parsing goes through `parse`/`command` below instead of
 * the derived `Parser::parse`. The annotations `clap` appends to an argument's
 * help (`[default: ...]`, `[possible values: ...]`) have no hook either, so the
 * arguments that would show them hide them instead and name the values in their
 * help text.
 */

use crate::i18n::gettext;
use clap::{Arg, ArgAction, Args, CommandFactory, FromArgMatches, Parser, Subcommand, ValueEnum};
use std::path::PathBuf;

fn about_root() -> String {
    gettext("Open, inspect and modify Minecraft structure files (terminal frontend)")
}

fn about_modules() -> String {
    gettext("Analyse the modules installed next to the executable")
}

fn about_render() -> String {
    gettext("Load a structure file and write a rendered PNG")
}

fn about_completions() -> String {
    gettext("Print a shell completion script")
}

fn help_region_dir() -> String {
    gettext(
        "Directory holding the region modules \
         (default: `region_module/` next to the executable)",
    )
}

fn help_load_dir() -> String {
    gettext(
        "Directory holding the object codecs \
         (default: `load_module/` next to the executable)",
    )
}

fn help_kind() -> String {
    gettext(
        "Which modules to report. `all` lists both region modules and object \
         codecs, `region` only the region modules, and `load` only the object \
         codecs; the default is `all`",
    )
}

fn help_verbose() -> String {
    gettext("Also print the path of every library")
}

fn help_file() -> String {
    gettext("The structure file to load and draw")
}

fn help_output() -> String {
    gettext("Where to write the PNG (default: `render.png` next to the input)")
}

fn help_size() -> String {
    gettext("The picture's size in pixels (default: `960x540`)")
}

fn help_yaw() -> String {
    gettext("Degrees around the world's y axis; 0 looks from the south")
}

fn help_pitch() -> String {
    gettext("Degrees above the horizon; 90 looks straight down")
}

fn help_distance() -> String {
    gettext(
        "How far the camera stands from the region's middle, in blocks. \
         Without it the region is framed to fill the picture",
    )
}

fn help_ortho() -> String {
    gettext("Draw without perspective, so parallel lines stay parallel")
}

fn help_highlight() -> String {
    gettext(
        "Put the block outline around the block at this region coordinate, \
         the way the Qt view highlights the block the mouse is over (`X,Y,Z`)",
    )
}

fn help_background() -> String {
    gettext(
        "A flat colour (`rrggbb`, with or without a leading `#`) to put behind \
         the blocks. Without it the PNG keeps its transparent background",
    )
}

fn help_region_lib() -> String {
    gettext(
        "The `libregion_rs` library the region modules import `region_new` from. \
         The default looks next to the executable and beside the region modules",
    )
}

fn help_jar() -> String {
    gettext(
        "A client jar (or another zip laid out like one) to read blockstates, \
         models and textures from",
    )
}

fn help_resources() -> String {
    gettext(
        "The cache directory holding the client jar and the downloaded asset \
         objects. Without it the configured cache directory is used",
    )
}

fn help_version() -> String {
    gettext("The version whose client jar under the cache directory to read")
}

fn help_no_download() -> String {
    gettext(
        "Never fetch a missing client jar, even when the configuration allows it. \
         A version's block models and textures are only in that jar, so without \
         one (here or as `--jar`) the picture comes out empty",
    )
}

fn help_region() -> String {
    gettext(
        "Which region of a multi-region file to draw, by index or by name. \
         Without it the first region is used",
    )
}

fn help_shell() -> String {
    gettext(
        "The shell to emit a completion script for: `bash`, `elvish`, `fish`, \
         `powershell` or `zsh`",
    )
}

fn help_completion_output() -> String {
    gettext(
        "Install into this directory instead of the shell's own completion \
         directory. The shell's conventional file name is used inside it",
    )
}

fn help_print_script() -> String {
    gettext("Print the script to standard output instead of installing it")
}

/* The strings `clap` writes itself. */
fn usage_heading() -> String {
    gettext("Usage:")
}

fn arguments_heading() -> String {
    gettext("Arguments")
}

fn options_heading() -> String {
    gettext("Options")
}

fn commands_heading() -> String {
    gettext("Commands")
}

fn help_flag() -> String {
    gettext("Print help")
}

fn version_flag() -> String {
    gettext("Print version")
}

fn help_subcommand_about() -> String {
    gettext("Print this message or the help of the given subcommand(s)")
}

fn help_subcommand_arg() -> String {
    gettext("Print help for the subcommand(s)")
}

#[derive(Debug, Parser)]
#[command(
    name = "dhlrc-tui",
    version,
    about = about_root(),
    long_about = None
)]
pub struct Cli {
    #[command(subcommand)]
    pub command: Option<Command>,
}

#[derive(Debug, Subcommand)]
pub enum Command {
    #[command(about = about_modules())]
    Modules(ModulesArgs),
    #[command(about = about_render())]
    Render(RenderArgs),
    #[command(about = about_completions())]
    Completions(CompletionsArgs),
}

/// Which modules `modules` reports on.
#[derive(Debug, Clone, Copy, PartialEq, Eq, ValueEnum)]
pub enum Kind {
    All,
    Region,
    Load,
}

#[derive(Debug, Args)]
pub struct ModulesArgs {
    #[arg(long, value_name = "DIR", help = help_region_dir())]
    pub region_dir: Option<PathBuf>,

    #[arg(long, value_name = "DIR", help = help_load_dir())]
    pub load_dir: Option<PathBuf>,

    #[arg(
        long,
        value_enum,
        default_value = "all",
        hide_default_value = true,
        hide_possible_values = true,
        help = help_kind()
    )]
    pub kind: Kind,

    #[arg(short, long, help = help_verbose())]
    pub verbose: bool,
}

#[derive(Debug, Args)]
pub struct RenderArgs {
    #[arg(value_name = "FILE", help = help_file())]
    pub file: PathBuf,

    #[arg(short, long, value_name = "PATH", help = help_output())]
    pub output: Option<PathBuf>,

    #[arg(
        long,
        value_name = "WxH",
        default_value = "960x540",
        hide_default_value = true,
        help = help_size()
    )]
    pub size: String,

    #[arg(long, value_name = "DEG", help = help_yaw())]
    pub yaw: Option<f32>,

    #[arg(long, value_name = "DEG", help = help_pitch())]
    pub pitch: Option<f32>,

    #[arg(long, value_name = "BLOCKS", help = help_distance())]
    pub distance: Option<f32>,

    #[arg(long, help = help_ortho())]
    pub ortho: bool,

    #[arg(long, value_name = "X,Y,Z", help = help_highlight())]
    pub highlight: Option<String>,

    #[arg(long, value_name = "RRGGBB", help = help_background())]
    pub background: Option<String>,

    #[arg(long, value_name = "DIR", help = help_region_dir())]
    pub region_dir: Option<PathBuf>,

    #[arg(long, value_name = "DIR", help = help_load_dir())]
    pub load_dir: Option<PathBuf>,

    #[arg(long, value_name = "PATH", help = help_region_lib())]
    pub region_lib: Option<PathBuf>,

    #[arg(long, value_name = "PATH", help = help_jar())]
    pub jar: Option<PathBuf>,

    #[arg(long, value_name = "DIR", help = help_resources())]
    pub resources: Option<PathBuf>,

    #[arg(long, value_name = "ID", help = help_version())]
    pub version: Option<String>,

    #[arg(long, help = help_no_download())]
    pub no_download: bool,

    #[arg(long, value_name = "INDEX|NAME", help = help_region())]
    pub region: Option<String>,
}

#[derive(Debug, Args)]
pub struct CompletionsArgs {
    #[arg(value_enum, hide_possible_values = true, help = help_shell())]
    pub shell: clap_complete::Shell,

    #[arg(short, long, value_name = "DIR", help = help_completion_output())]
    pub output: Option<PathBuf>,

    #[arg(long, conflicts_with = "output", help = help_print_script())]
    pub print: bool,
}

/// Parses the command line, exiting on an error and on `--help`/`--version`.
pub fn parse() -> Cli {
    Cli::from_arg_matches(&command().get_matches()).unwrap_or_else(|error| error.exit())
}

/// The parser, with the texts `clap` writes itself translated.
pub fn command() -> clap::Command {
    let mut command = Cli::command();
    /* `clap` adds the `-h`/`-V` arguments and the `help` subcommand while it
     * builds, so build first and translate afterwards. */
    command.build();
    localize(command)
}

/// Translates the chrome `clap` builds and recurses into the subcommands.
fn localize(command: clap::Command) -> clap::Command {
    let mut command = command
        .help_template(help_template())
        .subcommand_help_heading(commands_heading());
    if command.get_name() == "help" {
        command = command.about(help_subcommand_about());
    }

    let names: Vec<String> = command
        .get_subcommands()
        .map(|subcommand| subcommand.get_name().to_string())
        .collect();
    for name in names {
        command = command.mut_subcommand(name, localize);
    }

    let ids: Vec<String> = command
        .get_arguments()
        .map(|argument| argument.get_id().as_str().to_string())
        .collect();
    for id in ids {
        command = command.mut_arg(id, localize_arg);
    }

    command
}

/// Gives every argument a translated heading, and the two flags and the `help`
/// subcommand's argument their own text.
fn localize_arg(argument: Arg) -> Arg {
    // Only the flags `clap` adds itself get their text replaced. An argument the
    // command defines itself may well be called `--version` (the render command
    // has one, taking the game version), and must keep its own help text.
    let argument = match argument.get_action() {
        ArgAction::Help => argument.help(help_flag()),
        ArgAction::Version => argument.help(version_flag()),
        _ if argument.get_id().as_str() == "subcommand" => {
            argument.help(help_subcommand_arg())
        }
        _ => argument,
    };
    let heading = if argument.is_positional() {
        arguments_heading()
    } else {
        options_heading()
    };
    argument.help_heading(heading)
}

/// `clap`'s default template with the "Usage:" heading translated. Without the
/// heading, `clap` writes the English constant itself.
fn help_template() -> String {
    format!(
        "{{before-help}}{{about-with-newline}}\n{} {{usage}}\n\n{{all-args}}{{after-help}}",
        usage_heading()
    )
}
