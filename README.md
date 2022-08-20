# Litematica Reader in C language

[中文文档](README_zh.md)

## Introduction

Although it was named as `litematica_reader_c`, **it's not only intended to read litematica file**, but process the material list in blocks in a litematica file region. Reading litematica file is just the first step to process, or in other words **there should be a source of material list to process, and reading a litematica file is the most convenient.**

It uses [libnbt](https://github.com/djytw/libnbt) as the library to read litematica file.

## Dependencies

- [libnbt](https://github.com/djytw/libnbt) (already in submodule of the repo)
- `zlib` or `libdeflate` (See `README.md` in `libnbt`)
- [cJSON](https://github.com/DaveGamble/cJSON) `>= 1.7.13`
- `gettext` (It's a part of `glibc` so you might not need to install it in GNU/Linux but other systems might need)(Optional -- if you need translation)

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

The implement now determines whether `getline()` could be used, if not then use the implement written by myself (In `dh_string_getline()`). In environment that don't provide `LC_MESSAGES`, it is replaced with `LC_ALL`. But due to a small problem I couldn't compile it on Windows (The msys environment in my computer may have some problems and couldn't link cjson).

## Usage

### Main Program

**Update: The translation is using `gettext` anyway, you could use the program even without translation and it will detect whether you can use it.**

Function 1 (Litematica material list with recipe combination) currently doesn't use the translation process (translation there is only valid for block name), if could enter the program, enter 1 to enter this function.

### Litematica material list with recipe combination

If needed, copy `config/` directory to the executable file's directory to use black list and replace list in the directory (Instead of list in the code).

Find translation file's object value in `.minecraft/assets/indexes`, find in `objects/` (directory with the first 2 characters), copy to executable file's directory and rename to `translation.json`. Program also runs without the file.

Unpack the jar file of the corresponding version and copy directory `data/minecraft/recipes` to `recipes` in the executable file's directory to use recipe combination.

Code improvements and translation file of this part will be done in later version.

### NBT reader, modifier and litematica block reader

Please ensure there's translation file. Do as the request.
