# Architecture

dhlrc opens, edits and exports Minecraft structure/region files. One shared
core and two frontends (Qt Widgets and a terminal) sit on top of a plugin
system that supplies the file formats. Everything is C++23 + Qt 6/KF6 in
`src/`, Rust elsewhere.

## What gets built

| Artefact            | From                            | Loaded by                             |
| ------------------- | ------------------------------- | ------------------------------------- |
| `libregion_rs`      | `region-rs`                     | `dhlrc_qt`, `dhlrc-tui`, every module |
| `libdhlrc_core`     | `dhlrc-core`                    | `dhlrc_qt` (C ABI)                    |
| `dhlrc_qt`          | `src/`, `src/dhsettingsdialog/` | the user                              |
| `dhlrc-tui`         | `dhlrc-tui` (rlib link)         | the user                              |
| `region_module/*`   | `region-*-rs`                   | the core, at startup                  |
| `load_module/*`     | `nbt-component`                 | the core, at startup                  |
| `locale/`           | `po/`                           | both frontends (gettext)              |

CMake builds the cargo workspace and stages every artefact in the build
directory, which is why the programme runs from there with no environment
setup. See [Build and test](build.md).

## Component map

- **Frontend (Qt)** — `src/`: windows, tabs, dialogs, jobs, settings UI.
- **Terminal** — `dhlrc-tui/`: commands plus an interactive settings/modules UI.
- **Core** — `dhlrc-core/`: config, schema, plugin host, notifications, logging,
  download, estimates, render ABI.
- **Resources** — `dhlrc-resource/`: packs, client jar, asset store (id → bytes).
- **Geometry** — `dhlrc-model/`: blockstates → models → baked quads.
- **Rasteriser** — `dhlrc-render/`: quads + textures → pixels, picking.
- **Region model** — `region-rs/`: the `Region` structure and its C ABI
  (`src/region.h`).
- **Region plugins** — `region-*-rs/`: import/export one file format.
- **Object codecs** — `nbt-component/`: file bytes → an object a plugin reads.
- **Plugin kit** — `common-rs/`, `zurinbt-common/`: FFI config objects, i18n,
  region helpers, estimates.

The dependency rule is one-way: frontends → core → (resource, model, render);
plugins → region-rs + common-rs; **no Qt in Rust, no Rust in the Qt sources
beyond the two C headers**.

## How a file becomes a region

1. The user picks a file; the extension selects a candidate module.
2. `dhlrc-core` owns the `dlopen`s; `ModuleLibrary`/`ModuleBase` resolve the
   plugin's symbols through it.
3. The file is read and decompressed (`file_try_uncompress` in `src/region.h`),
   then decoded by the object codec its format names.
4. The plugin turns the object into one or more `Region`s from `libregion_rs`.
   The Qt side wraps the opaque pointer in `RegionClass`; `ManageRegionUI`
   keeps the global list, selection and locking, and each region keeps its own
   undoable change list (there is no redo or global dirty flag).

Full detail: [region_create.md](region_create.md), [plugin.md](plugin.md),
[load_module.md](load_module.md). The Qt side of this is in
[frontend.md](frontend.md).

## How a region becomes pixels

`dhlrc-core`'s `render` feature builds a scene (quads, textures, transforms)
from a `Region` through `dhlrc-model`/`dhlrc-render` and exposes it through
`dhlrc_render.h`. The Qt view either uploads the quads to OpenGL or asks for a
CPU image; picking, highlights and overlays all go back through the same ABI,
so the frontends hold no geometry of their own. See [render.md](render.md).

## Process model

- The GUI runs one thread. Loading and saving run as `KJob`s on the global
  thread pool; they report progress through the core's estimate and are
  cancellable.
- The core watches `config.toml` on its own thread and delivers changes and
  notifications through C callbacks; the Qt wrapper (`dhcore.h`) turns those
  into queued signals, so a callback never touches a widget directly.
- String ownership across the C ABI is strict: a borrowed `const char *` lives
  as long as the object it came from; a `char *` the caller keeps is released
  with `dhlrc_string_free`; region objects are freed with `region_free`; module
  libraries stay loaded for the process's life.

## Where to read next

| You want to…                               | Read                                                 |
| ------------------------------------------ | ---------------------------------------------------- |
| change the window, tabs, dialogs, jobs     | [frontend.md](frontend.md)                           |
| change the config, schema, notifications   | [core.md](core.md)                                   |
| touch resource packs, models, the renderer | [rust-libs.md](rust-libs.md), [render.md](render.md) |
| work on the terminal frontend              | [tui.md](tui.md)                                     |
| add a region format                        | [plugin.md](plugin.md)                               |
| add an object codec                        | [load_module.md](load_module.md)                     |
| build, package, run the checks             | [build.md](build.md)                                 |
| write code that matches the project        | [conventions.md](conventions.md)                     |

## Rules that keep it working

- Frontends never `dlopen`; only the core loads plugins, so paths, symbol
  binding and error reporting live in one place.
- One settings description (`schema`) drives the Qt dialog and the terminal
  page; a key added there reaches both frontends.
- Anything a second call site would need belongs in `src/utility.h`,
  `src/dhcore.h`, `dhlrc-core` or `common-rs`, not copied into another file.
  [reuse.md](reuse.md) is the catalogue.
