# Minecraft Structure Modifier

[中文文档](README_zh.md) | [Development Document](dev-doc/README.md)

**LLMs should modify this file at the least lines and make the file human readable. (the Chinese README should be treated the same way)**

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
- [Logs](#logs)

---

## Disclaimer

Starting from version 20260930 (although not released) / [commit 3844872][commit], the project starts to use
LLMs to help development. Don't worry, the GUI part is still fully tested manually. You can also use the
older versions (although many changes were made between releases 0.3.6 and 20260820, you might need to test
the commits to choose a usable version) which were written manually.

## Temporary Problem in this Commit (Manually Report)

- Setting migration problem.
- Render window push/pull will blank sometimes.

## Report Bugs and Errors

Before reporting, you should confirm that:

- You are using the latest release (or a binary from the latest commit, if possible);
- The bugs/errors aren't fixed in newer commits;

To confirm your version, run `dhlrc_qt --version` or see `Help -> About` in the GUI. Then you can create
a new issue, and attach the version and the bug/error description.

Since we can't easily detect the bugs once at all, we made a file called [bugs](bugs.md) for a reference.

## License Problem

I'm lazy to change the license, sorry for the inconvenience.

## Introduction

You can import your structure into a `Region` structure. Then do some modification (although it's currently a
work in progress). After that you can export the structure to the type you wish.

## Dependencies

The Qt frontend needs the Rust libraries `region-rs` and `dhlrc-core`, and the
modules supported by `dhlrc-core` should also be written in Rust. It also needs
Qt 6 and KF6. On Windows, I recommend building with
[MSYS2](https://www.msys2.org/) (see `build-msys2.sh`).

## Compile

Qt GUI (requires Qt 6 and KF6):

```bash
cmake -B build && cmake --build build
```

Headless build (shared libraries and terminal frontend only; no Qt/KF6):

```bash
cmake -B build -DDH_BUILD_GUI=OFF && cmake --build build
```

Headless build without gettext (e.g. Termux/Android):

```bash
cmake -B build -DDH_BUILD_GUI=OFF -DDH_TRANSLATIONS=OFF && cmake --build build
```

Windows ([MSYS2](https://www.msys2.org/) UCRT64):

```bash
./build-msys2.sh            # release build into build-ucrt64/
./build-msys2.sh --package  # also a portable .zip (setup .exe with NSIS)
./build-msys2.sh --help     # all options
```

## Features

- NBT, Litematica and Sponge schematic (`.schem`) support.
- Modify base data of the region.
- A 3D preview that draws a region from its block models and textures, with
  turning, zooming and block picking.
- Old-school GUI, with notification centre and log view.
- Restores the previous session at start: the open tabs, the active tab, the
  window size and the sidebar width; regions that were loaded without an open
  tab are loaded again too (turn it off in Settings -> General).

## Usage

Just run `dhlrc_qt`, and the Qt backend will start unless you are using Linux tty.

A short guide is built into the programme: open `Help -> Help` for the quick
start (also in `resources/usage.md`, with the Chinese translation next to it in
`resources/usage.zh_CN.md`). The `Program Directories` entry in the left sidebar
shows the folders the programme uses — check the configuration and cache folders
there before uninstalling.

`dhlrc-tui` is an interactive terminal frontend: run it without arguments for
a mouse-capable settings page with a sidebar; `--help` lists the other
subcommands.

## Logs

The Qt frontend keeps a daily log in `<cache directory>/logs/dhlrc-YYYY-MM-DD.log`;
the cache directory is listed under `Program Directories` in the left sidebar,
and `Help -> Open Log Folder` opens the log directory directly.

Only messages at or above the configured level are written; the default is
`info`. Set the level with the `DHLRC_LOG_LEVEL` environment variable
(`debug`, `info`, `warning`, `critical` or `off`) or with
`--log-level <level>` on the command line, which takes precedence. `off`
disables the file while the terminal output stays as it was.

[commit]: https://github.com/DreamHelium/dhlrc/commit/38448728e647b9b6665f0c502d1f79c00d1d4afc
