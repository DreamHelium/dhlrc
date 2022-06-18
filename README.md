# Litematica Reader in C language

## Introduction

Although it was named as `litematica_reader_c`, **it's not only intended to read litematica file**, but process the material list in blocks in a litematica file region. Reading litematica file is just the first step to process, or in other words **there should be a source of material list to process, and reading a litematica file is the most convenient.**

It uses [libnbt](https://github.com/djytw/libnbt) as the library to read litematica file.

## TODO

- [ ] Add config.json
- [ ] Read litematica-generated csv
- [ ] Simple NBT reader
- [ ] Better support for reading litematica file (Currently it would still be incomplete and can't read item in containers)
- [ ] Read region details (Ability to read exact block in a region is done in `litematica_region.h` and could be easily used but currently don't write in `example.c`, reading material list uses this ability.)
- [ ] Ncurses or qt-based interface
- [ ] Edit litematica file Utilities (Changing version etc.)

## Provided functions in code (partly)

 * Get litematica region information (provided in `litematica_region.h`)
   * litematica region numbers and names;
   * block nums in one region's BlockStatePalette;
   * get index of block and block id in BlockStatePalette.
   * create material list (use three linked list types in `dhlrc_list.h`)
 * Useful list types (provided in `dhlrc_list.h`)
   * ItemList (In this program's early implement it was "Block" and was designed to use as a struct array)
      - Full function implements for create, add item, change number,sort (by numbers), delete item, scan repeated item;
      - unused "placed" and "available" member for upcoming litematica-generated csv file support (when adding item it would set to 0);
      - Combining two lists (you should sort by yourself if needed).
      - **Return 0 if no error occurs, if it's ScanRepeat, return 1 if there's the same item in the list.**
      - ItemList to "litematica-generated csv"-like csv file.
  * BlackList and RepeatList
    - Directly initialize
      - Read file `config/ignored_blocks.json` and `config/block_items.json`, if failed, use the list provided in code instead.
      (These two files are from another project [litematica-tools](https://github.com/Kikugie/litematica-tools))
    - Scan or replace item.
 * Get recipe and combine to original list (**Experimental**, in `recipes_util.h`)
   - Read recipes (see instructions below).
   - A dummy implement to combine recipe.
   - Read vanilla translation file (`translation.json`) to translate item string. (If fails, return NULL.)

## Usage

If no release package is provided, then you should compile it yourself.

libnbt needs zlib or libdeflate, see libnbt's project README for more details.

This project also use [cJSON](https://github.com/DaveGamble/cJSON), if you are in Linux, it may be provided in software repos as `cJSON-devel` or other name, otherwise refer to the project's website.

If there's qt creator in your computer, simply imports this project.

Or you can directly compile by 

```bash
gcc -DLOAD_RECIPES -lz -lcjson -lm example.c dhlrc_list.c litematica_region.c recipe_util.c file_util.c -o program_name 
```

if you're in Linux. If in other system, try to use the compiler that fits that system.

**This example is also dummy, it process all regions and use dummy ways to combine recipe, you can rewrite it as you wish.**

**To use the functions in `recipe_util.h`, unpack minecraft client jar and move files in `data/minecraft/recipes` to the directory of the compiled program, put it in `recipes` directory.**

**To use the translation, find translation file index in `.minecraft/assets/indexes/version.json`, and then find in objects, copy to program directory and rename to `translation.json`.**
