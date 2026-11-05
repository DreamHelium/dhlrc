# Terminal frontend (`dhlrc-tui`)

The Qt-free frontend. It links `dhlrc-core` as an rlib (not the C ABI) and reuses
the same config, schema, load history and module loader as the GUI. Command
reference: [dhlrc-tui/README.md](../dhlrc-tui/README.md).

## Commands (`src/cli.rs`)

`clap` derive describes the whole command line; `main.rs` dispatches each
variant to its module.

| Command | What it does |
| --- | --- |
| `modules` | Lists region modules and object codecs; flags a missing codec. |
| `settings` | Interactive settings page; also what no command does on a terminal. |
| `render` | Loads a structure file through `region.rs` and writes a PNG. |
| `load-time` | Measures loads into the history or predicts from it (see README). |
| `completions` | Generates/installs a shell completion script from the clap command. |

`--region-dir`/`--load-dir`/`--region-lib` locate the modules; `render` adds the
resource and camera options (see the README).

## `src/ui/`

`term.rs` is the only terminal I/O: raw mode, alternate screen, hidden cursor,
mouse capture, the event loop and painting a `Screen`. `mod.rs` is the page model
and drawing (`App`, `Page`, key/mouse events, hit-test `Rect`s, the settings
editor); `command.rs` is the `:` command line (parser, completion, history,
in-memory regions, transcript); `markdown.rs` styles one line for status and
tooltips.

Layout: row 0 is the header, the left sidebar lists the five pages, the body is
the page, and the last three rows are status, detail/tooltip or editor, and the
function bar (F10 quits); below 70×18 it complains. Settings draws a group list
and its rows; entry pages scroll plain lines, and `Page::Modules` uses
`modules::report()`'s text. Clickable `Rect`s are recorded while drawing, so the
hit test never repeats it.

## Shared with the core

- `dhlrc_core::config` (`default_paths`, `load_or_create`, `save_config`,
  `ConfigInit`) and `dhlrc_core::schema::SETTINGS`: the page is built from that
  table, so one edit updates both frontends; `r` reloads, `s` saves, and an old
  config goes through `backup_and_reset`.
- `dhlrc_core::loadtime` (`History`, `predict_for_file`, `memory_guess`) — the
  same `load-times.tsv`.
- `dhlrc_core::modules::{Modules, default_dirs}` and `dhlrc_core::log` — same
  discovery, same sink API.

Drawing does **not** go through the core's render ABI: `render.rs` links
`dhlrc-render` and `dhlrc-resource` and calls `build_scene`/`render`.

## i18n and the log file

`i18n.rs` sets the locale, binds the `dhlrc` domain to `locale/` next to the
executable (the same catalogue as Qt) and re-exports `gettext`. `logfile.rs`
appends one UTC line per record to
`<cache_dir>/logs/dhlrc-tui-YYYYMMDD.log` (level, target, escaped message), a
new file at midnight, flushed per line; the core falls back to stderr if it
cannot open it.

## Build and staging

CMake's `dhlrc_cargo_build` builds the workspace and copies
`${CARGO_TYPE}/dhlrc-tui` into `${CMAKE_BINARY_DIR}`, beside `region_module/`,
`load_module/`, `locale/` and (with the GUI) `libdhlrc_core`;
`install(PROGRAMS …)` puts it in the install dir (`bin/`, or `.` on Windows).
Module dirs come from `current_exe()`, so the staged binary needs no flags.

## Recipes

**Add a subcommand.** Add a variant and `Args` struct in `cli.rs` (every
user-visible string in a `gettext` helper, as the others do), implement
`mod::run(args) -> ExitCode`, and dispatch it in `main.rs`; the parser, `--help`
and completions follow.

**Add a settings page row.** Add the `SettingDef` (and `get`/`set`) to
`dhlrc-core/src/schema.rs`; this page rebuilds groups and rows from `SETTINGS`,
and Qt does the same through the C ABI. A new group needs only a
`settings_summary` arm, not TUI code.

## The gotcha

The region modules import `region_new` from `libregion_rs`, and a bare
`cargo build` binary sits in `target/debug/`, away from them. Run it with
`LD_LIBRARY_PATH=build ./target/debug/dhlrc-tui …` and `--region-dir
build/region_module`, `--load-dir build/load_module` (or `--region-lib
build/libregion_rs.so`); a CMake-staged binary has all of that beside it.
