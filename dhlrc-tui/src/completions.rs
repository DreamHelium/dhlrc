/*! The `completions` command.
 *
 * `clap_complete` generates the script from the same `clap` description the
 * parser uses, so the completion can never drift from the real options.
 *
 * By default the script is installed into the directory the shell loads
 * completions from for the current user, so `dhlrc-tui completions bash` is
 * enough. `--print` writes it to standard output instead (for packaging, or for
 * a shell with no such directory), and `--output <DIR>` installs it somewhere
 * else.
 *
 * The script is built in memory first and written afterwards: `generate`
 * panics when its writer fails, and a script piped into `head` or a pager has a
 * closed pipe, which is normal rather than an error.
 */

use crate::cli::{Cli, CompletionsArgs};
use crate::i18n::gettext;
use clap::CommandFactory;
use clap_complete::{Shell, generate};
use formatx::formatx;
use std::env;
use std::fs;
use std::io::{self, Write};
use std::path::{Path, PathBuf};
use std::process::ExitCode;

pub fn run(args: CompletionsArgs) -> ExitCode {
    let mut command = Cli::command();
    let name = command.get_name().to_string();

    let mut script = Vec::new();
    generate(args.shell, &mut command, name.clone(), &mut script);

    if args.print {
        return write_stdout(&script);
    }

    let dir = match args.output {
        Some(dir) => dir,
        None => match default_dir(args.shell) {
            Some(dir) => dir,
            None => {
                /* No directory this shell loads from; print it so the user can
                 * put it where it belongs. */
                eprintln!(
                    "{}",
                    gettext(
                        "This shell has no standard completion directory, so the script was \
                         printed instead."
                    )
                );
                return write_stdout(&script);
            }
        },
    };

    let file = dir.join(completion_file_name(args.shell, &name));
    if let Err(error) = install(&file, &script) {
        eprintln!(
            "{}: {}: {error}",
            gettext("Could not write the completion script"),
            file.display()
        );
        return ExitCode::FAILURE;
    }

    println!(
        "{}",
        formatx!(gettext("Wrote {}."), file.display().to_string()).unwrap_or_default()
    );
    /* zsh only loads a completion file that sits in `$fpath`, and its per-user
     * directory is not in the default one. */
    if args.shell == Shell::Zsh {
        println!(
            "{}",
            formatx!(
                gettext("If completions do not load, add {} to your fpath."),
                dir.display().to_string()
            )
            .unwrap_or_default()
        );
    }

    ExitCode::SUCCESS
}

/// The directory the shell loads completions from for the current user, or
/// `None` when it has none (PowerShell dot-sources its scripts instead).
fn default_dir(shell: Shell) -> Option<PathBuf> {
    match shell {
        Shell::Bash => data_home().map(|dir| dir.join("bash-completion/completions")),
        Shell::Zsh => data_home().map(|dir| dir.join("zsh/site-functions")),
        Shell::Fish => config_home().map(|dir| dir.join("fish/completions")),
        Shell::Elvish => config_home().map(|dir| dir.join("elvish/lib")),
        _ => None,
    }
}

/// The file name a shell expects in its completion directory. Notably bash
/// wants the bare command name, not the `name.bash` clap_complete produces.
fn completion_file_name(shell: Shell, name: &str) -> String {
    match shell {
        Shell::Bash => name.to_string(),
        Shell::Zsh => format!("_{name}"),
        Shell::Fish => format!("{name}.fish"),
        Shell::Elvish => format!("{name}.elv"),
        Shell::PowerShell => format!("_{name}.ps1"),
        _ => name.to_string(),
    }
}

/// `$XDG_DATA_HOME`, or `~/.local/share`.
fn data_home() -> Option<PathBuf> {
    xdg_home("XDG_DATA_HOME", ".local/share")
}

/// `$XDG_CONFIG_HOME`, or `~/.config`.
fn config_home() -> Option<PathBuf> {
    xdg_home("XDG_CONFIG_HOME", ".config")
}

/// An XDG base directory: the environment variable when it holds an absolute
/// path (a relative one is invalid and ignored, per the spec), otherwise the
/// fallback under the home directory.
fn xdg_home(variable: &str, fallback: &str) -> Option<PathBuf> {
    match env::var_os(variable) {
        Some(dir) if !dir.is_empty() => {
            let path = PathBuf::from(dir);
            path.is_absolute().then_some(path)
        }
        _ => home().map(|home| home.join(fallback)),
    }
}

fn home() -> Option<PathBuf> {
    #[cfg(windows)]
    let variable = "USERPROFILE";
    #[cfg(not(windows))]
    let variable = "HOME";
    env::var_os(variable)
        .map(PathBuf::from)
        .filter(|path| !path.as_os_str().is_empty())
}

/// Creates `file`'s directory and writes `script` to it.
fn install(file: &Path, script: &[u8]) -> io::Result<()> {
    if let Some(parent) = file.parent() {
        fs::create_dir_all(parent)?;
    }
    fs::write(file, script)
}

fn write_stdout(script: &[u8]) -> ExitCode {
    let stdout = io::stdout();
    let mut stdout = stdout.lock();
    match stdout.write_all(script) {
        Ok(()) => ExitCode::SUCCESS,
        /* A closed pipe (`| head`) is not a failure. */
        Err(error) if error.kind() == io::ErrorKind::BrokenPipe => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!(
                "{}: {}: {error}",
                gettext("Could not write the completion script"),
                gettext("standard output")
            );
            ExitCode::FAILURE
        }
    }
}

#[cfg(test)]
mod tests {
    use super::completion_file_name;
    use clap_complete::Shell;

    #[test]
    fn every_shell_gets_the_file_name_it_expects() {
        /* bash-completion wants the bare command name, not `name.bash`. */
        assert_eq!(completion_file_name(Shell::Bash, "dhlrc-tui"), "dhlrc-tui");
        assert_eq!(completion_file_name(Shell::Zsh, "dhlrc-tui"), "_dhlrc-tui");
        assert_eq!(
            completion_file_name(Shell::Fish, "dhlrc-tui"),
            "dhlrc-tui.fish"
        );
        assert_eq!(
            completion_file_name(Shell::Elvish, "dhlrc-tui"),
            "dhlrc-tui.elv"
        );
        assert_eq!(
            completion_file_name(Shell::PowerShell, "dhlrc-tui"),
            "_dhlrc-tui.ps1"
        );
    }
}
