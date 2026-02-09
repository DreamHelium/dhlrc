# Minecraft Structure Modifier

[中文文档](README_zh.md) | [Development Document](dev-doc/README.md)

## License Problem

Since `crabnbt` is licensed under GPL, I had to use GPL license. Sorry for the inconvenience.

## Introduction

You can import your structure into a `Region` structure. Then do some modification (Although it's currently working in
progress). After that you can export the structure to the type you wish.

## Dependencies

See `Cargo.toml` in `*-rs` directories, and `CMakeLists.txt` in `src`.

## Compile

If there's already CMake installed, just compile using command:

```bash
cmake -B build
cmake --build build
```

You might need to copy `region_rs` library from `region-rs/target/debug(or release)` into root directory and build
directory. If it's allowed, using a soft link.

## Features

todo.

## Usage

Just run `dhlrc_qt`, and the Qt backend will start unless you are using Linux tty.