# Litematica Reader in C language

[中文文档](README_zh.md)
[pic](funcshow_en.png)

**Note: The most part of the code will be transformed to C++, so the project might be dhlrcpp in the future.**

## Introduction

Although it was named as `litematica_reader_c`, **it's not only intended to read litematica file**, but process the material list in blocks in a litematica file region. Reading litematica file is just the first step to process, or in other words **there should be a source of material list to process, and reading a litematica file is the most convenient.**

It uses [libnbt](https://github.com/djytw/libnbt) as the library to read litematica file.

## Dependencies

- [libnbt](https://github.com/djytw/libnbt) (already in submodule of the repo)
- `zlib`
- [cJSON](https://github.com/DaveGamble/cJSON) `>= 1.7.13`
- `gettext` (It's a part of `glibc` so you might not need to install it in GNU/Linux but other systems might need)(Optional -- if you need translation)
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

## Usage

Just run `dhlrc.bin`, and the Qt backend will start unless you are using Linux tty.
If you just want to convert file, just run `dhlrc.bin conv --help` and do with the instruction.

## For developer

The program uses `launcher` to load module, if you want to add new module for the program, just implement the 5 functions below and compile it as library (or executable?), and put it under `module`:

```c
    extern const char* module_name();
    extern DhStrArray* module_name_array();
    extern const char* module_description();
    extern const char* help_description();
    extern int start_point(int argc, char** argv, const char* prpath);
```

**WARNING: Add `extern "C"` in C++ project! Otherwise it couldn't be recognized!**