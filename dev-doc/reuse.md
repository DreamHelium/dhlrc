# Reuse map

Look here before writing a helper. If what you need is not listed, add it to the
most general home that already exists rather than to your own translation unit:
`src/utility.h` → `src/dhcore.h` → `dhlrc-core` → `common-rs`.

## C++ frontend (`src/`)

| Need                                | Use                                                                 |
| ----------------------------------- | ------------------------------------------------------------------- |
| config/cache/translation directory  | `dh::configDirectory ()`, `dh::cacheDirectory ()`, `dh::getTranslationDir ()` |
| locale date for a list row          | `dh::formatDateTime ()`, `dh::getDateTimeFromTimeStamp ()`          |
| region name from a pattern          | `dh::expandRegionNamePattern ()` and the `*Placeholder` constants   |
| pick region(s) from a dialog        | `dh::getRegion ()` + `GeneralChooseDialog`; labels from `dh::getRegionChoiceLabels ()` |
| core values, settings, prediction   | `DhCore::instance ()`; widgets do not call `dhlrc_core_*` directly  |
| plugin symbol lookup                | `ModuleLibrary` (never `dlopen` from the frontend)                  |
| plugin metadata / binding           | `ModuleBase::fromIndex ()`, `SingleModuleBase`, `MultiModuleBase`   |
| undoable region edit                | `RegionClass::recordChange ()` / `revertChange ()` + `AutoLocker`   |
| plugin option widgets               | `ConfigObjectUI::getObject ()`                                      |
| block property names and values     | `dh::blockPropertyValues (core, index)`                             |
| block-state name/data C arrays      | `dh::propertyPointers (...)` (storage stays in the caller)          |
| drag-and-drop table                 | `DhTableView`                                                       |
| wrapping button rows                | `FlowLayout`                                                        |
| closable / tear-off tabs            | `DetachableTabWidget`                                               |
| page or window base                 | `DhWidget`                                                          |
| job completion message              | `dh::tasks::report ()`, `MainWindow::showNotification ()`           |
| logging                             | `qDebug ()`/`qWarning ()`/`qCritical ()`; viewer via `dh::log::LogStore` |
| translated string                   | `_()` now, `N_()` when it is translated later                        |
| shared downloads (jar / object)     | `DhDownloadManager` (dedup by tag); `DhDownloader` is the worker     |

## Rust

| Need                                | Use                                                                 |
| ----------------------------------- | ------------------------------------------------------------------- |
| configuration                       | `dhlrc_core::config` (`Config`, `default_paths`, `load_or_create`, `ConfigWatcher`) |
| a new setting                       | add a `SettingDef` to `dhlrc_core::schema::SETTINGS`; both frontends pick it up |
| plugin host                         | `dhlrc_core::modules` (`Modules`, `ModuleInfo`, `default_dirs`)      |
| notifications                       | `dhlrc_core::notification::{Level, Notification}` (produce, never draw) |
| logging                             | `dhlrc_core::log` macros and sink                                   |
| load-time history / prediction      | `dhlrc_core::loadtime`                                              |
| downloads                           | `dhlrc_core::download` (`Downloader`, tags, cancel)                 |
| block catalogue / material list     | `dhlrc_core::catalog`, `dhlrc_core::material`                       |
| resource files (jar, pack, store)   | `dhlrc_resource` (`ResourceSet`, `Identifier`, `cache`, `handle_block`) |
| blockstates and baked geometry      | `dhlrc_model` (`blockstate`, `resolve`, `mesh`)                     |
| rasterising and picking             | `dhlrc_render` (`Scene`, `Camera`, `Raster`, `Texture`)             |
| region data                         | `region_rs::Region` is the only region type; C ABI in `src/region.h` |
| plugin FFI kit                      | `common_rs::{config::ffi, i18n, util, region, estimate, helper_struct}` |
| NBT tags and encoding names         | `zurinbt_common`                                                    |
| gettext on Android                  | `gettext_shim` (excluded elsewhere)                                 |

## Removed — do not reintroduce

- `src-qml/` — an abandoned Qt Quick prototype; its CMake referred to deleted
  targets and it could not configure. Qt Widgets is the only GUI.
- `src/paletteaddui.*`, `src/dhcheckboxgroup.*` — never in the build and not
  compilable; palette editing lives in `PaletteListUI`.
- `DH_ENABLE_DEBUG` / `DH_DEBUG_IN_IDE` — the sidebar's Debug page and the
  `dhdebugwidget.*` it needed do not exist; enabling it could not compile.
- `resources/dhlrc.xml` + `resources/CMakeLists.txt` — a glib resource build
  nobody added; `resources/dhlrc.qrc` is the live path.
- Commented-out build lines and dead declarations: delete them when you find
  them instead of keeping them "for reference" — git has the history.

## Where new code goes

| Adding…                                  | Home                                                    |
| ---------------------------------------- | ------------------------------------------------------- |
| a helper two C++ files could use          | `src/utility.h` / `.cpp` (namespace `dh`)               |
| something the Qt side needs from the core | `dhlrc-core` + `ffi.rs` + `include/dhlrc_core.h`, wrapped in `src/dhcore.h` |
| a setting both frontends show             | `dhlrc_core::schema`                                    |
| behaviour every plugin needs              | `common-rs`                                             |
| a file format                             | a new `region-*` crate (see [plugin.md](plugin.md))     |
| an object codec                           | a crate like `nbt-component` (see [load_module.md](load_module.md)) |
| resource access (jar/pack/store)          | `dhlrc-resource`                                        |
| geometry or drawing                       | `dhlrc-model` / `dhlrc-render` ([rust-libs.md](rust-libs.md)) |

When a helper moves or a file disappears, update the page that mentions it —
[frontend.md](frontend.md), [core.md](core.md), [rust-libs.md](rust-libs.md) or
this map.
