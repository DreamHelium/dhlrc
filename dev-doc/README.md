# Development Document

**If you are not a developer, you can ignore this directory.**

This is for people — and language models — changing the code. Read
[Architecture](architecture.md) first: it maps every target, says how a file
becomes a region and a region becomes pixels, and points at the page for the
part you are about to touch.

Two kinds of loadable library sit next to the executable: region plugins in
`region_module/` and object codecs in `load_module/`. Both are dynamic
libraries (`.so` / `.dll` / `.dylib`); dhlrc discovers them at startup and binds
their symbols by name.

## Development

- [Architecture](architecture.md) — targets, component map, data flow, process model
- [Build and test](build.md) — options, staging, Windows, checks, and the
  local `temp/` harnesses (read `temp/HARNESSES.md` when it exists)
- [Conventions](conventions.md) — formatting, naming, i18n, logging, ownership, adding files
- [Reuse map](reuse.md) — where shared code lives; check it before writing a helper
- [Qt frontend](frontend.md) — window, tabs, region model, background jobs, settings
- [Core](core.md) — `dhlrc-core` modules, C ABI, configuration and schema
- [Rust libraries](rust-libs.md) — resource, model, render, `common-rs`, `region-rs`, codecs
- [Terminal frontend](tui.md) — commands, pages, and what it shares with the Qt frontend

## Formats and plugins

- [Plugin reference](plugin.md) — every symbol a region plugin may or must export, plus the option sets
- [Load module](load_module.md) — the object codecs that decode a file before a region plugin sees it
- [Create a region](region_create.md) — how a file is loaded and turned into a region
- [Rendering](render.md) — how a block becomes pixels, through models and the C ABI
- [Sponge schematic](schem.md) — the `.schem` format, its versions and entities
- [Interface self-check](uicheck.md) — the `--ui-check` mode: what it asserts about the window, and how to add a check

## User-facing help

The help window (`src/dhhelpui.cpp`) shows one article at a time, picked from a
table of topics. An article is a markdown resource compiled into the binary:
`resources/<name>.md`, with `resources/<name>.<locale>.md` (for example
`usage.zh_CN.md`) shown instead when the message catalogue is being read for
that locale, and the plain file as the fallback. A new article needs both files
in `resources/dhlrc.qrc` and its topic title marked with `N_ ()`, so `xgettext`
extracts it. `resources/usage.md` is the quick start for users;
`region_create.md` is the same kind of article for developers.
