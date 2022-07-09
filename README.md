# Litematica Reader in C language

[中文文档](README_zh.md)

## Introduction

Although it was named as `litematica_reader_c`, **it's not only intended to read litematica file**, but process the material list in blocks in a litematica file region. Reading litematica file is just the first step to process, or in other words **there should be a source of material list to process, and reading a litematica file is the most convenient.**

It uses [libnbt](https://github.com/djytw/libnbt) as the library to read litematica file.

## Dependencies

- [libnbt](https://github.com/djytw/libnbt) (already in submodule of the repo)
- `zlib`或`libdeflate` (See `README.md` in `libnbt`)
- [cJSON](https://github.com/DaveGamble/cJSON) `>= 1.7.13`

## Compile

Use `git` to get this repo, then get submodule.

```bash
git submodule update --init
```

Change `cJSON` directory to `cjson` manually (Or install `cJSON-devel`/`libcjson-dev` and confirm the version is no lesser than `1.7.13`).

If version doesn't meet the needs, either compile `cJSON` and install or add `cjson/cJSON.h` and `cjson/cJSON.c` in `add_executable` in `CMakeLists.txt` and add repo root directory to `target_link_libraries`.

If there's already CMake installed, just compile using command:

```bash
cmake -B build
cmake --build build
```

If not, compile manually (not recommended).

.pro could no longer be used.

The program now may only POSIX-compatable (currently couldn't be compiled on Windows), if needed rewrite by yourself (The problem seems appear on `setlocale()` and `getline()` functions). For historical reasons it could not be rewrited now.

## Usage

### Main Program

Main program uses the new translation process, copy `lang/` in repo to the executable file's directory. Later version will throw a warning for the translation file not found (Since there's no content in the code, which will affect the normal use when it's no translation).

Function 1 (Litematica material list with recipe combination) currently doesn't use the translation process (translation there is only valid for block name), if could enter the program, enter 1 to enter this function.

### Litematica material list with recipe combination

If needed, copy `config/` directory to the executable file's directory to use black list and replace list in the directory (Instead of list in the code).

Find translation file's object value in `.minecraft/assets/indexes`, find in `objects/` (directory with the first 2 characters), copy to executable file's directory and rename to `translation.json`. Program also runs without the file.

Unpack the jar file of the corresponding version and copy directory `data/minecraft/recipes` to `recipes` in the executable file's directory to use recipe combination.

Code improvements and translation file of this part will be done in later version.

### NBT reader, modifier and litematica block reader

Please ensure there's translation file. Do as the request.
