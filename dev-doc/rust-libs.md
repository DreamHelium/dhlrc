# Rust libraries

The crates below the frontends. [Core](core.md) covers `dhlrc-core` (rlib +
cdylib); [Rendering](render.md) covers how a block becomes pixels. Each heading
gives the crate type; the loading table says who opens it.

## dhlrc-resource (rlib)

`Identifier` names a resource, a `Source` answers it, and a `ResourceSet` stacks
sources and memoises every hit and miss.

| Module | What it does |
| --- | --- |
| `identifier` | `namespace:path`; no colon means `minecraft`. |
| `archive` | `ZipSource`: a client jar, pack or mod jar, opened once and locked. |
| `cache` | The layout: `client_jar/`, `resource_pack/`, `index_json/`, `asset_json/`, `object/`. |
| `bundled` | Files compiled in: `data_version.csv`, `handle_block.json`. |
| `object` | Assets resolved through the version's asset index to hash-named files. |
| `environment` | `ResourceEnvironment`: cache dir + version + packs to a `ResourceSet`. |
| `set` | The ordered stack, JSON memo, and `list()` with earlier-source shadowing. |
| `translate` | Language files: `block.<namespace>.<path>`, then `item.<namespace>.<path>`. |
| `handle_block` | Block-to-items rules: rows, patterns, units, pair items. |

`ResourceEnvironment::resource_set()` stacks resource packs and mod jars (highest
priority first), the version's client jar, then the downloaded object store (the
only place non-English language files are; the jar has only `en_us`). Absent
sources are skipped, and the first source with an id wins.

## dhlrc-model (rlib)

`blockstate` reads `variants`/`multipart`; `model` reads boxes, faces, texture
variables and `parent`; `resolve` folds the parent chain and follows `#variable`
textures; `mesh` bakes each face to four corners with a UV, applying
variant/element rotation and `uvlock`; `catalog` lists a version's blocks. Baking
is `quads()`: geometry only, in 0..1 block and UV space. Textures and tint are in
`dhlrc-render`; the rules are in [Rendering](render.md).

## dhlrc-render (rlib + cdylib)

Textures, tint, camera, depth-buffered rasterisation and PNG output — everything
that needs the image rather than the geometry. It is a software renderer on
purpose, so the Qt view, the terminal and the tests draw the same picture. The
pipeline, shading and costs are in [Rendering](render.md).

## common-rs (rlib)

`config` (the `*_config_*` ABI, `ConfigObject`, `ConfigItem`), `i18n` (an
identity `const fn` that marks a message for `xgettext`), `region` (`Region`,
`Palette`, `BlockEntity`, `BaseData` — what `region-rs` hands back), `util`
(`string_free`, `string_to_ptr_fail_to_null`, compression, progress), `estimate`
(`DhlrcEstimate`, `estimate_region_file`), `helper_struct` (`HelperStruct`:
progress/cancel/OOM) and `my_error`. The core uses `estimate`; a frontend-only
helper stays in that frontend.

## region-rs (cdylib)

The region **object** and its C ABI. The C++ declarations are in `src/region.h`;
`region-rs` exports:

- `lib.rs`: `region_new`/`region_free`, `region_set_*`, `string_free`,
  `vec_free`/`vec_to_cstr`, and the NBT-tree walkers (`nbt_root_*`,
  `nbt_compound_*`, `nbt_tag_*`, `nbt_list_*`).
- `cancel_flag.rs`: `cancel_flag_new`, `cancel_flag_is_cancelled`,
  `cancel_flag_cancel`.
- `file.rs`: `file_try_uncompress` and `OBJECT_COMPRESSION_*` detection.
- `region_getter.rs`: palette, block, block-entity, entity, biome, metadata.
- `default.rs`: defaults a new region starts with; `helper_struct.rs`:
  `helper_struct_new`/`_free`.

It is process-wide because the plugins do not link it as a crate: they import
`region_new` with `#[link(name = "region_rs")]`, and Qt links the `region_rs`
target. The TUI's loader opens `libregion_rs` with `RTLD_GLOBAL` **before** any
plugin, so every plugin uses the one copy already in the process, and a
plugin-allocated string is freed by the same allocator under either frontend.

## nbt-component (cdylib) and zurinbt-common (rlib)

`nbt-component` is the object codec: decompressed bytes to a `zuri_nbt` tree,
nothing more. Its symbols and `ObjectLoadOptions` (`strict`, `encoding`,
`detected`) are in [Load module](load_module.md). `zurinbt-common` is the
plain-Rust half the region plugins share (`get_palette_from_nbt_tag`,
`gettext_text`, the `get_type_from_*` macros); it exports no C symbols.

## gettext-shim (rlib)

Package `dhlrc-gettext-shim`, library name `gettextrs`. Android has no libintl,
so the target-specific dependency there points at this identity implementation:
`gettext` returns the message unchanged and bind/domain/setlocale are no-ops.
Every crate still writes `use gettextrs::…`.

## Dependency graph

```
dhlrc-tui --+-- dhlrc-core --+-- common-rs -- zuri_nbt, flate2, sysinfo
            |                +-- dhlrc-resource -- zip, serde_json
            |                +-- dhlrc-model -- dhlrc-resource
            |                +-- dhlrc-render (feature) -- model, resource, png
            +-- dhlrc-render
            +-- dhlrc-resource

Qt --+-- dhlrc_core (cdylib)      plugins -- common-rs, zurinbt-common, region_rs
     +-- region_rs (cdylib)       codecs  -- common-rs, zuri_nbt
```

## Loaded at run time

| Artifact | Where | Who loads it |
| --- | --- | --- |
| `libdhlrc_core` | next to `dhlrc_qt` | Qt only; the TUI links the rlib. |
| `libregion_rs` | next to the binaries | Qt links it, the TUI `dlopen`s it, plugins import. |
| `nbt_component` | `load_module/` | `Modules::load`. |
| `region_*_rs` | `region_module/` | The same module loader. |
| `libdhlrc_render` | nowhere | Declared `lib + cdylib`; nothing stages it. |

`libdhlrc_core` is loaded by `dhlrc_qt` alone, so a headless build neither stages
nor installs it. `libdhlrc_render` is reached through the core feature or the
rlib, never as a shared library.

## Recipes

**Add a resource source.** Implement `dhlrc_resource::Source` (`get`, and `list`
only if it can enumerate), give it a `cache` subdirectory if the downloader
writes it, and push it at the right priority in
`ResourceEnvironment::resource_set()`. Models, textures, translation and the
catalog then see it without changing.

**Add a shared helper.** Plugin crates need it → `common-rs` (no C types unless
it is an ABI helper); only the core → `dhlrc-core`; only the TUI → `dhlrc-tui`.
Never put model or render code in `common-rs`.
