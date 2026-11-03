# dhlrc-tui

An experimental terminal frontend for dhlrc, built on the Qt-free `dhlrc-core`.
It exists so dhlrc can eventually run where there is no GUI.

**Status:** the command line exists and the terminal UI is interactive. The
settings page edits the configuration; the Modules page lists what is
installed. Render, Regions and Help are entry points for now: they say what
the page will do and which command line already does it.

## Commands

### `modules`

Reports what the core can load and, when both lists are shown, points out the
region modules whose object codec is not installed.

```console
$ dhlrc-tui modules
Region modules (2)
  /path/to/build/region_module
  #  type       suffix     codec  regions  encoding
  0  litematic  litematic  NBT    multi    big-endian
  1  nbt        nbt        NBT    single   big-endian

Object codecs (1)
  /path/to/build/load_module
  #  codec
  0  NBT
```

| Option                       | Meaning                                                         |
| ---------------------------- | --------------------------------------------------------------- |
| `--region-dir <DIR>`         | Region modules; default is `region_module/` next to the binary. |
| `--load-dir <DIR>`           | Object codecs; default is `load_module/` next to the binary.    |
| `--kind <all\|region\|load>` | Which list to report.                                           |
| `-v`, `--verbose`            | Also print the path of every library.                           |

The defaults sit next to the _binary_. The CMake build stages the binary in its
build directory (`build/dhlrc-tui`), beside `region_module/`, `load_module/` and
`locale/`, so running it there needs no arguments:

```console
$ ./build/dhlrc-tui modules
```

A plain `cargo build` leaves the binary in `target/debug/`, where those
directories are not; point `--region-dir` and `--load-dir` at a build's
directories then. The region modules also depend on `libregion_rs.so`, which the
CMake build makes findable with a runpath; a bare cargo binary needs it on the
loader path (`LD_LIBRARY_PATH=build`).

### `settings`

An interactive page for the saved configuration, drawn with the same setting
schema the Qt frontend uses. Running `dhlrc-tui` with no command opens it when
standard input and output are a terminal; a pipe or a script gets the help
instead.

```console
$ dhlrc-tui settings
```

The screen has a header with the configuration file's path, a sidebar with the
pages on the left, the list of setting groups next to it and the settings of the
chosen group on the right; the window is followed when the terminal is resized.
The bottom line explains the keys of the current page. Changes are written to
the configuration file as soon as they are made (`s` writes it again
explicitly, `r` reloads it from disk).

| Input                          | Effect                                                     |
| ------------------------------ | ---------------------------------------------------------- |
| `↑` `↓`, `Page Up` `Page Down` | Move in the focused pane; `Home`/`End` jump on the page.   |
| `Tab`                          | Cycle the sidebar, the group list and the settings.        |
| `←` `→`                        | Step through the choices of a list setting.                |
| `Enter`, `Space`               | Edit a value, flip a switch, or open the row that is selected. |
| `Esc`                          | Cancel an editor; `q` (outside an editor) quits.           |
| `Backspace`                    | Delete in the editor.                                      |

The mouse works where the terminal reports it (most terminal emulators and
multiplexers, including touch input mapped to a pointer):

| Input                          | Effect                                                     |
| ------------------------------ | ---------------------------------------------------------- |
| Click a page in the sidebar    | Switch to that page.                                       |
| Click a group                  | Switch to it.                                              |
| Click a row                    | Select it; click it again (or double-click) to edit or flip. |
| Wheel                          | Scroll the pane under the pointer: one group per notch on the left, three rows per notch on the right. |

A terminal without mouse reporting still works with the keyboard. Values that
cannot be typed (colours, paths) are edited as text, and invalid text keeps the
editor open with an error in the status line instead of writing the file.

### Pages, the sidebar and the function bar

The pages are listed both in the left sidebar and in the bottom function bar
(`F1`–`F5`, with `F10` right after them to quit). Entries in either place are
clickable, so the pages can be driven with a mouse or with a touch screen that
reports a pointer; `F1`–`F5` and `F10` keep working from the keyboard, and
clicking a page in the sidebar only switches to it (it does not need the
keyboard focus first).

| Page           | What it shows                                                     |
| -------------- | ----------------------------------------------------------------- |
| Settings       | The configuration editor described above.                         |
| Modules        | The same report as `dhlrc-tui modules` (region modules and codecs). |
| Render         | Entry point for rendering a file in this window; the command line works today. |
| Regions        | Entry point for the interactive region editor.                    |
| Help           | The keys and the mouse gestures, and the meaning of the pages.    |

The entry pages scroll with `↑` `↓`, `Page Up`/`Page Down`, `Home`/`End` and the
wheel, so a long module list stays readable. The status line of every page
explains its keys, and the bar hides the hint while a value is being edited.

### `load-time`

Measures how long files take to load, and predicts it for a file from the loads
that were measured before. A file that was measured before (same path, size and
modification time) is answered exactly — which is also the cache question the
history exists for; a file that was never loaded is extrapolated from the size
through the history.

`--estimate` asks the region module that claims the file's suffix instead and
bases the estimate on what the region actually holds: its dimensions, block
count and decompressed size, which the format records before the block data.
That is much better than the file size, because compression lets a sparse
region be a tenth the size of a filled one with the same dimensions. The
module reads the headers through its own parser, not through a second
implementation in this command.

The estimate is an optional plugin capability (`region_estimate`; see
[dev-doc/plugin.md](../dev-doc/plugin.md)). A module built before it existed, a
file suffix no module claims, or a format that cannot tell still work: the
command falls back to the decompressed size (and then the compressed size),
exactly as the prediction does. The plugin that answers is chosen the same way
the load chooses it, so a file is estimated by the format that would read it.

```console
$ dhlrc-tui load-time big.litematic small.schem
big.litematic   61.2 MB  litematic  predicted 4.89 s  (no history)  measured 5.31 s  error +8%
small.schem     2.1 MB   schem      predicted 171 ms  (from earlier loads of this format)  3 samples  measured 158 ms  error -8%

$ dhlrc-tui load-time --predict big.litematic      # no load, no new measurement
big.litematic   61.2 MB  litematic  predicted 4.89 s  (measured before)  usual range 4.64 s to 5.14 s

$ dhlrc-tui load-time --estimate station.litematic
station.litematic  4.6 KB  litematic
    2 regions, 7280 blocks in their volumes
    likely load 7.5 ms to 22 ms (from the block count)
    unpacked 26.5 KB    memory about 50.9 MB (between 25.5 MB and 76.4 MB, a guess)
```

Without `--predict` every file is actually loaded through the same modules the
Qt frontend uses, so the measurement is the real number and the history learns
it. `--json` prints the same results as one JSON array; `--no-record` loads
without adding to the history; `--clear` forgets every measurement.

| Option                | Meaning                                                    |
| --------------------- | ---------------------------------------------------------- |
| `--predict`           | Only predict; do not load or record.                       |
| `--estimate`          | Ask the matching region module for an estimate, no load; falls back to the file size. |
| `--json`              | Print JSON instead of the table.                           |
| `--no-record`         | Load, but do not add the measurement to the history.       |
| `--clear`             | Forget every measured load.                                |
| `--history <PATH>`    | Use another history file (a test, or a read-only cache).    |
| `--region-dir <DIR>`  | Region modules; default is next to the binary.             |
| `--load-dir <DIR>`    | Object codecs; default is next to the binary.              |
| `--region <INDEX\|NAME>` | Which region of a multi-region file to load.            |

The history is `load-times.tsv` in the cache directory (`~/.cache/dhlrc/` by
default), one measured load per line: format, size, modification time,
milliseconds and path, tab separated with tabs and newlines in paths escaped.
It is a cache, not a setting: deleting it only loses confidence. Predictions
name their basis (`measured before`, `from earlier loads of this format`, `from
earlier loads of other formats`, or `no history`), so a guess is never mistaken
for a measurement.

### `completions`

Installs a completion script into the directory the shell loads completions
from for the current user, generated from the same description the parser uses.

```console
$ dhlrc-tui completions bash   # ~/.local/share/bash-completion/completions/dhlrc-tui
$ dhlrc-tui completions zsh    # ~/.local/share/zsh/site-functions/_dhlrc-tui
$ dhlrc-tui completions fish   # ~/.config/fish/completions/dhlrc-tui.fish
```

`$XDG_DATA_HOME` and `$XDG_CONFIG_HOME` are honoured. `--output <DIR>` installs
into another directory, and `--print` writes the script to standard output
instead — handy for packaging, and the fallback for PowerShell, which has no
such directory.

zsh only reads completions from `$fpath`, so its per-user directory has to be
added to it; the command prints a reminder naming that directory.

## Translation

The messages go through gettext under the project-wide `dhlrc` domain, so they
share the Qt frontend's catalogue. The catalogue is looked up in `locale/` next
to the executable, which the CMake build already satisfies for
`build/dhlrc-tui`. Set `LANG` to check them:

```console
$ LANG=zh_CN.UTF-8 ./build/dhlrc-tui modules
```

`--help` is translated the same way. `src/cli.rs` puts every user-visible string
in a small helper function that calls `gettext`, because `xgettext` only finds
`gettext` calls in expression position; `make_po.sh` collects them from the Rust
sources like the rest. The strings `clap` writes itself (the section headings,
`Usage:`, the `-h`/`-V` descriptions and the `help` subcommand) are rewritten on
the command built by `cli::command()`.
