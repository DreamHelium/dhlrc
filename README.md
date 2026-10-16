# Minecraft Structure Modifier

[中文文档](README_zh.md) | [Development Document](dev-doc/README.md)

---

## Table of Contents

- [Disclaimer](#disclaimer)
- [Report Bugs and Errors](#report-bugs-and-errors)
- [License Problem](#license-problem)
- [Introduction](#introduction)
- [Dependencies](#dependencies)
- [Compile](#compile)
- [Features](#features)
- [Usage](#usage)

---

## Disclaimer

Starting from version 20260930 (although not released) / [commit 3844872][commit], the project starts to use
LLMs to help development. Don't worry, the GUI part is still fully tested manually. You can also use the
older versions (although many changes were made between releases 0.3.6 and 20260820, you might need to test
the commits to choose a usable version) which were written manually.

## Temporary Problem in this Commit (Manually Report)

- Settings' logical problem.
- No tests with modules.
- External NBT Reader unexpectedly exits when file error sometimes.

## Report Bugs and Errors

Before reporting, you should confirm that:

- You are using the latest release (or a binary from the latest commit, if possible);
- The bugs/errors aren't fixed in newer commits;

To confirm your version, run `dhlrc_qt --version` or see `Help -> About` in the GUI. Then you can create
a new issue, and attach the version and the bug/error description.

## License Problem

I'm lazy to change the license, sorry for the inconvenience.

## Introduction

You can import your structure into a `Region` structure. Then do some modification (although it's currently a
work in progress). After that you can export the structure to the type you wish.

## Dependencies

See `Cargo.toml` in `*-rs` directories, and `CMakeLists.txt` in `src`.

## Compile

If CMake is already installed, just compile with these commands:

```bash
cmake -B build
cmake --build build
```

For a headless build without the Qt frontend — only the shared libraries and the
terminal frontend — configure with `-DDH_BUILD_GUI=OFF`; Qt/KF6 are then not
needed. Add `-DDH_TRANSLATIONS=OFF` to skip the gettext catalogue too (which
otherwise needs `xgettext`/`msgfmt`); the programme then runs in English.

On Termux/Android there is no `libintl`, so the Rust translation layer is
disabled automatically (see `gettext-shim`): the build needs no C library and
messages stay in English. Combine with `-DDH_TRANSLATIONS=OFF` there.

On Windows with [MSYS2](https://www.msys2.org/), `build-msys2.sh` drives the
whole build from the UCRT64 environment: it checks the packages it needs (and
offers to install the missing ones), configures, builds, and — with `--package`
— assembles a redistributable archive, or an installer when NSIS is installed.

```bash
./build-msys2.sh            # release build into build-ucrt64/
./build-msys2.sh --package  # also a portable .zip and, with NSIS installed, a setup .exe
```

The installer lets the user choose the frontends (the Qt GUI and/or the terminal
one), the object codecs and region modules to install, and whether to create
Start Menu shortcuts. Selecting a region module pulls in the NBT codec it needs
(a module only works on the object a codec produced). It also registers the
AppUserModelID the core's desktop notifications are sent as, so Windows names
them after the application rather than PowerShell.

Run `./build-msys2.sh --help` for the other options (`--debug`, `--no-gui`,
`--no-translations`, `--offline`, `--clean`, `--build-dir`, `--jobs`).

## Features

- NBT, Litematica and Sponge schematic (`.schem`) support.
- Modify base data of the region.
- A 3D preview that draws a region from its block models and textures, with
  turning, zooming and block picking (see the [developer document](dev-doc/render.md)).
- Old-school GUI.

## Usage

Just run `dhlrc_qt`, and the Qt backend will start unless you are using Linux tty.

A short guide is built into the programme: open `Help -> Help` for the quick
start (also in `resources/usage.md`, with the Chinese translation next to it in
`resources/usage.zh_CN.md`). The `Program Directories` entry in the left sidebar
shows the folders the programme uses — check the configuration and cache folders
there before uninstalling.

tty support might be planned.

[commit]: https://github.com/DreamHelium/dhlrc/commit/38448728e647b9b6665f0c502d1f79c00d1d4afc
