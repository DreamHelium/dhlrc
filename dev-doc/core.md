# Core (`dhlrc-core`)

The Qt-free half of dhlrc: configuration, the settings schema, plugin loading,
downloads, notifications, logging, load history, resource access and the C ABI.
The Qt frontend loads it as a shared library; the terminal frontend links the
same crate as an rlib. The crates underneath it are in
[Rust libraries](rust-libs.md); the render side's pipeline is
[Rendering](render.md).

## Modules

| Module | What it owns |
| --- | --- |
| `config` | `Config` (TOML, sections), `ConfigPaths`, `load_or_create`, `save_config`, watcher. |
| `schema` | `SETTINGS`/`SettingDef` (key, page, control, default, get/set); both frontends use it. |
| `catalog` | A version's blocks and properties, built once via `dhlrc_model::catalog`. |
| `material` | `material_json`: palette entries to items, via `handle_block` rules. |
| `modules` | `Modules::load`: both module dirs, metadata, symbols, `region_estimate`. |
| `download` | `Downloader`: manifest, index, objects, client jar; `Sink`, `Cancel`. |
| `notification` | `Level` and `Notification { event_id, level, title, text }`. |
| `desktop` | Feature-gated: `present`/`present_all` through `notify-rust`. |
| `log` | A sink (Rust or C) plus the `log_*!` macros; stderr fallback. |
| `loadtime` | `History`, `predict_for_file`, `memory_guess`; `load-times.tsv`. |
| `render` | Feature-gated: the `dhlrc_render_*` ABI (scene, camera, image, pick). |
| `ffi` | Every `dhlrc_core_*` entry point, opaque handles, free helpers. |
| `lib` | `VERSION`; re-exports `Config`, `Notification`, `dhlrc_resource`. |

## Lifecycle and the configuration file

A C frontend does, in order:

1. `dhlrc_log_set_sink(...)` before the first call, if it wants the records.
2. `dhlrc_core_open(config_dir, notify, user_data, &error)`. `config_dir == NULL`
   uses the default location. The call loads or creates `config.toml`, reports
   the open-time notifications through `notify`, and returns an opaque
   `DhlrcCore *`; on failure it returns `NULL` and an owned `*error`.
3. `dhlrc_core_load_modules`, and for drawing `dhlrc_core_resources_configure`,
   then the accessors.
4. `dhlrc_core_watch_start` if the frontend wants external edits applied;
   `dhlrc_core_watch_stop` before `dhlrc_core_free` (free also stops the watcher).

Rust frontends use `lib.rs`'s re-exports instead of the C handle (see
[Terminal frontend](tui.md)).

The file is `<config_home>/dhlrc/config.toml`, where `config_home` is
`$XDG_CONFIG_HOME`, `%APPDATA%` on Windows, else `~/.config`. `cache_dir()` is
separate (`$XDG_CACHE_HOME/dhlrc`, `~/Library/Caches/dhlrc`,
`%LOCALAPPDATA%/dhlrc`) and is resolved in the core, so it is never empty. A
file whose `version` is not `CONFIG_VERSION` (5) is **not migrated**:
`dhlrc_core_config_needs_reset` says so, and `dhlrc_core_reset_config` keeps it
as `config.toml.bak` and writes the defaults.

## The C ABI

Headers: `include/dhlrc_core.h` for everything above,
`include/dhlrc_render.h` for the `render` feature. They are hand-written and
must match the `#[repr(C)]` layouts in `ffi.rs`/`render.rs`.

| Convention | Rule |
| --- | --- |
| Naming | `dhlrc_core_*`, `dhlrc_render_*`, and the `dhlrc_*_free` helpers. |
| Handles | `DhlrcCore`/`DhlrcConfigView`/`DhlrcScene` opaque; regions are `void *` from `libregion_rs`. |
| Borrowed | A `const char *` lives until its owner is freed or rebuilt; never free it. |
| Owned | A `char *` goes to `dhlrc_string_free`; bytes to `dhlrc_core_bytes_free`. |
| Inputs | The library never takes ownership; `NULL` means default or bad argument. |
| Errors | `0`/`-1` (`1`/`0` for predicates), plus an owned `char **error` on failure. |
| Callbacks | Notify/change/download/log/progress typedefs; their strings are borrowed per call. |

Categories: lifecycle/config/watch, modules and their symbols, load prediction,
per-plugin options, the settings table (`setting_count`/`info`/`choice`/`get`/
`set`/`summary`), resources and catalog, translations, material list and item
icons, and blocking downloads (object, client jar, its path and state). The
settings table is the full list; `DhlrcConfigView` is the smaller snapshot used
by `dhlrc_core_config_view`/`dhlrc_core_apply` — everything else is reached by
key through `dhlrc_core_setting_get`/`set`. A new setting therefore does not
widen the view struct.

## Threading

- `DhlrcCore` state is `Mutex`/`AtomicBool`-guarded; no core call requires the
  main thread.
- `dhlrc_core_open`'s notify callback runs on the calling thread;
  `DhlrcChangeFunc` on the watcher thread. Queue both to the UI thread.
- Download functions block, so run them on a worker; their info/progress/cancel
  callbacks run there. `DhlrcLogFunc` may run on any thread, called with the
  registry lock released.
- A `DhlrcScene` is not synchronised: build, draw, mutate and free it on one
  thread at a time; the progress callback runs on the builder thread, and the
  pixel buffer dies at the next draw or free.

## Notifications

`Notification` carries a stable `event_id` (`configCreated`, `configRecreated`,
`configReloaded`), a `Level` and translated title/text. `dhlrc_core_open` emits
the open-time ones through `DhlrcNotifyFunc`; `dhlrc_core_watch_start` emits
`configReloaded` through `DhlrcChangeFunc` when the file really changed (its own
save is not reported). With the feature built in, the core also shows them on
the desktop, unless `dhlrc_core_set_notifications_enabled` turns that off (Qt
does so while its window is active when `[notifications] suppress_when_active`
is set). The Qt trampolines feed its notification centre; `dhlrc-tui` calls
`ConfigInit::notifications()` and `App::show_notifications`, writing the status
line and console transcript.

## Load-time history

`loadtime` keeps up to `CAPACITY` (200) measurements in
`<cache>/load-times.tsv`, one tab-separated line per load plus a header.
`History` reads/records/clears it; `predict_for_file` answers an exact hit by
path+size+mtime, otherwise extrapolates from the same format, then others, then
the built-in guess; a region module's estimate replaces only the no-history
guess. The C side asks through `dhlrc_core_load_prediction` without touching the
file.

## Features and CMake

| Feature | Pulls in | CMake switch | Effect |
| --- | --- | --- | --- |
| `desktop-notifications` | `notify-rust` | `DH_DESKTOP_NOTIFICATIONS` (ON) | Core shows them. |
| `render` | `dhlrc-render` | `DH_BUILD_GUI` (ON) | Adds `dhlrc_render_*`. |

Neither is a default feature; CMake passes the enabled ones as `--features
dhlrc-core/desktop-notifications,dhlrc-core/render`. With `desktop-notifications`
off, the core only hands notifications to the frontend. `DH_BUILD_GUI=OFF` has
no render ABI; `dhlrc-tui` draws through `dhlrc-render` directly.

## Tests and examples

`cargo test -p dhlrc-core` covers the unit tests and `tests/ffi.rs`
(open/read/watch, apply/save, plugin options, old-config reset);
`tests/abi_smoke.c` is a hand-built C smoke test, not cargo test. `examples/`
holds `print_default_config`, `watch_config`, `fetch_client`, `blocks`, plus the
feature-gated `desktop_notify` and `render_region` (the renderer's C ABI against
an in-memory region).

## Recipes

**Add a C ABI function.** In `ffi.rs` (or `render.rs`), write a
`#[unsafe(no_mangle)] pub extern "C" fn` named `dhlrc_core_*`: validate nulls,
lock, return `0`/`-1` with an owned `set_error` message, and document ownership.
Mirror the declaration in the header, keep `#[repr(C)]` layout exact, and cover
it in `tests/ffi.rs`.

**Add a config key.** Add a serde-defaulted field to the right section in
`config.rs` and its `Default`; bump `CONFIG_VERSION` only when old files cannot
be read. Add `get_*`/`set_*` and a `SettingDef` in `schema.rs`: the TUI, the Qt
pages and `dhlrc_core_setting_*` all derive from `SETTINGS`. From C use
`setting_get`/`set` rather than widening `DhlrcConfigView`;
`print_default_config` prints the TOML.

**Add a notification kind.** Pick a stable `event_id`, build `Notification::new`
with a `Level` and `i18n`/`gettext` text, and return it from the producer
(`ConfigInit::notifications` for open-time) or pass it to the frontend callback;
no header or registry changes. The Qt side routes by `event_id`, so reuse it for
the same kind of event.
