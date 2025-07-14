# Litematica Reader in C language

[中文文档](README_zh.md)
[pic](funcshow_en.png)

**Note: The most part of the code will be transformed to C++, so the project might be dhlrcpp in the future.**

## Introduction

Although it was named as `litematica_reader_c`, **it's not only intended to read litematica file**, but process the
material list in blocks in a litematica file region. Reading litematica file is just the first step to process, or in
other words **there should be a source of material list to process, and reading a litematica file is the most
convenient.**

It uses [libnbt](https://github.com/djytw/libnbt) as the library to read litematica file.

Since it's a general-purpose project, the `assets` and `data` file should be provided by user. Luckily, there's a simple
wizard in `utility` in Qt module which helps you to do this.

## Dependencies

- [libnbt](https://github.com/djytw/libnbt) (already in submodule of the repo)
- `zlib`
- [cJSON](https://github.com/DaveGamble/cJSON) `>= 1.7.13`
- `gettext` (It's a part of `glibc` so you might not need to install it in GNU/Linux but other systems might need)(
  Optional -- if you need translation)
- `glib2`
- `minizip-ng`
- [dhutil](https://github.com/DreamHelium/dhutil) (already in submodule of the repo)

## Compile

Use `git` to get this repo, then get submodule.

```bash
git submodule update --init
```

If there's already CMake installed, just compile using command:

```bash
cmake -B build
cmake --build build
```

If not, install one.

## Features

- NBT management (NBT `interface` support and NBT Reader).
  Note: if you want to modify the content, you might need to use the `get_current_nbt` method and modify by yourself.
- Partly support of `NBT Struct` and `Litematic` using general `Region`.
  Note: support easily read block state and palette, generate block list (~Although it's called item list~), generate
  `NBT Struct` from `Region`.
- Partly recipe support

## Usage

Just run `dhlrc.bin`, and the Qt backend will start unless you are using Linux tty.

If you just want to convert file, just run `dhlrc.bin conv --help` and do with the instruction.