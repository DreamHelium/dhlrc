# Development Document

**If you are not a developer, you can ignore this directory.**

This is mainly for the plugin side: the `region_module` part (currently `region-nbt-rs`, `region-litematic-rs` and `region-schem-rs`)
and the `load_module` part (currently `nbt-component`).

Both kinds of library are **dynamic** libraries (`.so` / `.dll` / `.dylib`). Region plugins go into `region_module/`
and object codecs into `load_module/`, next to the executable; dhlrc discovers them at startup and binds their
symbols by name.

# Category

- [Plugin reference](plugin.md) — every symbol a region plugin may or must export, plus the option sets
- [Load module](load_module.md) — the object codecs that decode a file before a region plugin sees it
- [Create a region](region_create.md) — how a file is loaded and turned into a region
- [Rendering](render.md) — how a block becomes pixels: the model, the renderer, and the C ABI the Qt view draws through
- [Sponge schematic](schem.md) — the `.schem` format, its three versions, and how its entities are handled

# User-facing help

The help window (`src/dhhelpui.cpp`) shows one article at a time, picked from a table of topics. An article is a
markdown resource compiled into the binary: `resources/<name>.md`, with `resources/<name>.<locale>.md` (for
example `usage.zh_CN.md`) shown instead when the message catalogue is being read for that locale, and the plain
file as the fallback. A new article needs both files in `resources/dhlrc.qrc` and its topic title marked with
`N_ ()`, so `xgettext` extracts it. `resources/usage.md` is the quick start for users; `region_create.md` is the
same kind of article for developers.
