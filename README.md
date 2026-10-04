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

## Features

- NBT and Litematica support.
- Modify base data of the region.
- Old-school GUI.

## Usage

Just run `dhlrc_qt`, and the Qt backend will start unless you are using Linux tty.

tty support might be planned.

[commit]: https://github.com/DreamHelium/dhlrc/commit/38448728e647b9b6665f0c502d1f79c00d1d4afc
