# Development Document

If you are not a developer, you can ignore this directory.

This is mainly for the plugin side: the `region_module` part (currently `region-nbt-rs` and `region-litematic-rs`)
and the `load_module` part (currently `nbt-component`).

Both kinds of library are **dynamic** libraries (`.so` / `.dll` / `.dylib`). Region plugins go into `region_module/`
and object codecs into `load_module/`, next to the executable; dhlrc discovers them at startup and binds their
symbols by name.

# Category

- [Plugin reference](plugin.md) — every symbol a region plugin may or must export, plus the option sets
- [Load module](load_module.md) — the object codecs that decode a file before a region plugin sees it
- [Create a region](region_create.md) — how a file is loaded and turned into a region
