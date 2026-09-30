# Development Document

If you are not a developer, you can ignore this directory.

This is mainly for the `region_module` part (currently `region-nbt-rs` and `region-litematic-rs`).

You need to compile your library as a **dynamic** library (`.so` / `.dll` / `.dylib`) and drop it into the
`region_module/` directory next to the executable; dhlrc discovers it at startup and binds its symbols by name.

# Category

- [Plugin reference](plugin.md) — every symbol a plugin may or must export
- [Create a region](region_create.md) — how a file is loaded and turned into a region
