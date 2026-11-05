# Build and test

CMake drives one cargo workspace and links the result into the Qt executable.
Every artefact is staged in the build directory, so a build runs in place.

## Prerequisites

- CMake ≥ 3.16, a Rust toolchain with Cargo, gettext tools (`xgettext`,
  `msgfmt`, `extractrc`) only when translations are built.
- Qt 6 (Widgets, Svg, Concurrent, optionally OpenGL/OpenGLWidgets), KF6
  (ColorScheme, ConfigWidgets, I18n, WidgetsAddons, CoreAddons, IconThemes)
  and Extra CMake Modules. Without them use the headless build.

## Everyday commands

```bash
# full build: GUI + terminal frontend + modules + translations
cmake -B build && cmake --build build

# headless: shared libraries, plugins and `dhlrc-tui` only
cmake -B build -DDH_BUILD_GUI=OFF && cmake --build build

# no gettext tools available (Termux/Android)
cmake -B build -DDH_BUILD_GUI=OFF -DDH_TRANSLATIONS=OFF && cmake --build build
```

Do not run `cargo build` on its own for a working programme: CMake passes the
runpath, the build stamp and the feature set (`dhlrc-core/desktop-notifications`,
`dhlrc-core/render`). A bare cargo binary cannot find `libregion_rs` without
`LD_LIBRARY_PATH=target/debug`.

## Options

| Option                          | Default | Effect                                                        |
| ------------------------------- | ------- | ------------------------------------------------------------- |
| `DH_BUILD_GUI`                  | ON      | build `dhlrc_qt` (needs Qt 6/KF6)                             |
| `DH_TRANSLATIONS`               | ON      | build `po/` into `locale/` (needs gettext)                    |
| `DH_DESKTOP_NOTIFICATIONS`      | ON      | core shows notifications on the desktop (`notify-rust`)       |
| `DH_CARGO_OFFLINE`              | OFF     | `cargo --offline`, for a machine with a warm crate cache      |
| `CARGO_RELEASE`                 | OFF     | build the Rust side with `--release`                          |
| `DH_UI_CHECK`                   | OFF     | compile `--ui-check` (see [uicheck.md](uicheck.md))           |
| `DH_BUILD_RENDER_CHECK`         | OFF     | compile `--preview` (see [render.md](render.md))              |
| `DH_PACKAGE_RUNTIME`            | ON\*    | bundle the MSYS2/Qt runtime on a MinGW build                  |
| `DH_BUILD_INSTALLER`            | ON\*    | build the NSIS installer when `makensis` exists               |

\* only meaningful on Windows; CMake turns them off elsewhere.

## What lands where

`build/` gets `dhlrc_qt`, `dhlrc-tui`, `libregion_rs.so`, `libdhlrc_core.so`,
`region_module/`, `load_module/` and `locale/`; the Rust objects are under
`build/cargo_build/`. `cpack`/`make install` copy the same tree with the
binaries in `bin/` (Windows keeps everything flat). Nothing relies on an
install prefix to run.

## Windows

`build-msys2.sh` re-executes itself in the MSYS2 UCRT64 environment, checks and
offers to install the needed packages, then configures and builds into
`build-ucrt64/`. Options: `-d/--debug`, `-r/--release` (default), `--no-gui`,
`--no-translations`, `--offline`, `-p/--package`, `-c/--clean`, `-B DIR`,
`-j N`, `-y`. `--package` produces a portable `.zip`, and an installer `.exe`
when NSIS is installed. CI (`.github/workflows/build-msys2.yml`) runs this on
tag pushes and manual dispatches only.

## Translations

`make_po.sh` regenerates `po/dhlrc/dhlrc.pot` from the C++, the `.ui` files and
the Rust sources, and regenerates the "cute" catalogue; `po/CMakeLists.txt`
merges each `.po` and compiles it to `locale/<lang>/LC_MESSAGES/dhlrc.mo`. The
string macros are `_()` (C++) and `i18n()` (Rust). Run `make_po.sh` after
adding a user-visible string, then fill in the `.po` files.

## Checks

- `cargo test --workspace` — unit tests and the C ABI round trip
  (`dhlrc-core/tests/ffi.rs`).
- `-DDH_UI_CHECK=ON` build, then `Xvfb :99 & DISPLAY=:99 ./build/dhlrc_qt
  --ui-check` — window behaviour ([uicheck.md](uicheck.md)).
- `-DDH_BUILD_RENDER_CHECK=ON` build, then `./build/dhlrc_qt --preview ...` —
  one rendered picture ([render.md](render.md)).
- `dhlrc-core/tests/abi_smoke.c` built by hand (see its header) — the C header
  and the shared library agree.
- `./build/dhlrc_qt --version`, `./build/dhlrc-tui --version` — both start and
  report the same build stamp.

The GUI checks need a display; Xvfb is enough. The `offscreen` platform plugin
can deadlock while the icon theme is initialised, so prefer Xvfb even headless.

## Local harnesses (`temp/`)

`temp/` is a local scratch directory, ignored by git. **If it exists in your
working copy, read `temp/HARNESSES.md` before doing interactive verification** —
it indexes the GUI/TUI harnesses (property dialog, slice/region box, block
picker, tree filter, view fix, `lead_check/`, TUI pty probes), with entry
points, exit codes, fixtures and gotchas.

Those harnesses link the real build tree with only `main.cpp` replaced, so they
drive the real window or terminal rather than mocks; prefer them for behaviour
checks that `--ui-check`/`--preview` do not cover. Under DSH run one through a
`test_run` `gui:` case (or `gui_launch`); from a plain shell use
`temp/xvfb-run.sh <command…>`. Their binaries under `build-render/` are stale by
default — rebuild that tree (`cmake --build build-render`) before trusting a
run, since the harness links its object files — and Qt runs need a writable
`XDG_CONFIG_HOME` inside the workspace to avoid the KF6 config modal described
in that file.

Never commit `temp/`. On a fresh clone it is absent: fall back to the checks
above, and recreate fixtures with the script you are about to write rather than
assuming `temp/render/*.litematic` is there.

Everything the tests need to stay true is described in
[uicheck.md](uicheck.md) and [render.md](render.md).
