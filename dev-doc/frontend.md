# Qt Frontend

The desktop program is the `dhlrc_qt` target in `src/`: Qt 6 Widgets, KF6,
C++23. [architecture.md](architecture.md) covers what is built and how the
layers fit together; this document is the Qt frontend's own map — startup, the
window, the region model, jobs, settings and the pieces to reuse.

## Startup

`src/main.cpp` runs in this order:

1. `applyWindowsAppUserModelID()` and `KIconTheme::initTheme()` before
   `QApplication`; the icon theme must be initialised before the application
   object.
2. gettext domains `dhlrc` and `dhlrc-cute` in `dh::getTranslationDir()`;
   `KLocalizedString` domain; local helpers install Qt's and KF6's own
   catalogues. `CuteLanguagePack` switches to `dhlrc-cute` later and needs a
   restart.
3. Under `DH_UI_CHECK`, `--ui-check` first points `XDG_CONFIG_HOME`,
   `XDG_CACHE_HOME` and `XDG_DATA_HOME` at `DHLRC_UI_CHECK_DIR` (or a temporary
   directory), so a run never touches the user's configuration.
4. `QApplication::setApplicationName("dhlrc")`, an early
   `dh::log::initialize()` on the platform cache directory, then
   `DhCore::init(&a)`.
5. An old `config.toml` is backed up and replaced with defaults; the warning
   box is suppressed for `--ui-check`/`--preview`.
6. Logging is re-initialised against `dh::cacheDirectory()`; the level comes
   from `DHLRC_LOG_LEVEL` and `--log-level` overrides it.
7. Display name, version and the window icon `:/cn/dh/dhlrc/dhlrc.svg`;
   `QCommandLineParser` with help, version and `log-level` (`--help` and
   `--version` print and exit inside `parser.process()`). `--preview` and its
   options exist only with `DH_RENDER_CHECK`, `--ui-check` only with
   `DH_UI_CHECK` (see [build.md](build.md)).
8. `QApplication::setStyle("breeze")`; `--ui-check` exits via
   `dhRunUiCheck()`, `--preview` via `dhRunPreview()`; otherwise
   `MainWindow w; w.show(); return a.exec();`.

## Window and tabs

`MainWindow` (`src/mainwindow.cpp/.h`) is a vertical splitter: a scrollable top
strip of `KMessageWidget` rows (added with `addWidgetToTopArea()`) over a
horizontal splitter with the sidebar (`QListView` + search) and the page stack
(`QTabWidget`, movable and closable). Sidebar rows are actions: NBT File
Reader, Manage Region, Region Reader/Modifier, Settings, Program Directories,
Notifications & Log. A click or Enter runs
`openSidebarEntry()`; rows that group pages open a menu instead of a tab. The
menu bar holds Config (the KDE colour-scheme menu) and Help (Help, Open Log
Folder, About).

The static API is how pages are opened: `addWidgetToTab()`, `addKeyedTab()`,
`keyedTab()`, `showTab()`, `setPageTitle()`, `closeTab()`,
`openRegionReader()`, `openSettings(group)` and `ownWindow()`. A keyed page
keeps its key in the `dhTabKey` property, so a second request raises the
existing page instead of opening a duplicate. Double-clicking a tab tears it
off into a top-level window; closing that window discards a normal page or
docks a persistent one back. Persistent pages are `ManageRegionUI` and
`DhDirectoryUI` (`isPersistentPage()`): closing their tab hides them and
reopening reuses the same instance.

`RegionWorkspace` (`src/regionworkspace.cpp/.h`) is the tab of one region. Its
inner `DetachableTabWidget` holds, lazily, the pages `Reader` (built first),
`Modifier`, `Change History`, `3D Preview` and `Material List`; `Page`,
`currentPage()` and `showPage()` are what the session saves. The reader is the
main page: closing it closes the whole workspace, while the other pages are
dropped and rebuilt on demand (`dhlrcClosable`). `DetachableTabWidget` owns no
page lifetime; it emits `pageCloseRequested()` and the workspace decides what
closing means.

Session restore writes `session.json` next to `config.toml` when
`RestoreSession` is on: geometry, maximized state, sidebar width, tab list and
loaded regions. It saves 2 s after a tab change, every 60 s and in
`closeEvent()`. Restore reopens regions by `sourcePath`/`sourceRegion` (the
UUID is minted per load) as their files finish loading, then reorders the tabs;
a legacy `session.ini` is migrated once.

## Region model

`ManageRegionUI` (`src/manageregionui.cpp/.h`) owns the application-wide state:
`moduleBaseList` (region plugins), `loadObjectList` (object codecs) and
`regions` (`std::vector<std::shared_ptr<RegionClass>>`). The vector is guarded
by `regionsMutex`, because loading jobs append to it from worker threads
(`appendRegion()`); readers take a copy with `getRegions()` and hold the shared
pointers so a region survives removal from the list. `loadFiles()` starts one
`DhAllLoadJob` per batch; `loadModules()` calls `dhlrc_core_load_modules()`
once, wraps every core entry and drops invalid modules with a warning. The page
also has the selection toolbar, skips locked regions when removing and shows
"Locked!" on a busy row.

`RegionClass` (`src/regionclass.cpp/.h`) is the only access point to a
`libregion_rs` object. It keeps the object with a `void(*)(void*)` deleter, a
per-load UUID, the GUI-level `displayName()` (the file's own name is
`regionName()`), `sourcePath()`/`sourceRegion()` for session restore and the
change list. Its `region.h` wrappers lock the region themselves; **writers
refuse to change it while a job holds the lock**. The lock is a recursive mutex
plus a counter, visible through `locked()`/`lockedChanged()`; `AutoLocker`
holds it for a job's lifetime. Undo is per region, not global:
`recordChange(description, detail, undo)` then
`revertChange()`/`revertAllChanges()`, shown by `RegionChangesUI`. There is no
redo and no dirty flag. `locked_region()` takes the raw pointer with a lock
held for the call; `get_region()` is the old accessor that takes no lock at
all.

`ModuleBase` (`manageregionui.h`) wraps one entry of the core's module list.
`fromIndex()` reads `DhlrcModuleInfo` through
`dhlrc_core_region_module_info()`, picks `SingleModuleBase` or
`MultiModuleBase` from its `is_multi` (read from `region_is_multi()`), binds
`ModuleLibrary::region()` and calls `initialize()`, which resolves the symbols.
Unsupported operations are `asSingle()`/`asMulti()` returning null, and a
module whose required load function is missing is invalid and is dropped.
`ModuleLibrary` resolves symbols through `dhlrc_core_region_module_symbol()`
and `dhlrc_core_load_module_symbol()`; `LoadObjectBase` uses the same handle
for codecs. The plugin contract is in [plugin.md](plugin.md); the decode
pipeline is in [region_create.md](region_create.md).

The Qt side of that pipeline: the new region is appended through
`ManageRegionUI::appendRegion(region, displayName, filename[, regionKey])`,
labelled with
`dh::expandRegionNamePattern(multiRegionNamePattern, file, regionName)` and
falling back to `file - region`. `getModule(type)` picks a module by its
`region_type()` when saving; `ModuleBase::filter()` is the translated filter a
region file dialog offers.

## Background work

`DhLoadJob`/`DhAllLoadJob` (`src/dhloadjob.cpp/.h`) and
`SaveRegionJob`/`SaveMultiRegionJob`/`SaveAllRegionJob`
(`src/saveregionjob.cpp/.h`) are `KJob`/`KCompositeJob`s whose body runs on
`QtConcurrent::run` (the global thread pool; no `QThreadPool` of its own). One
composite per batch owns the per-file jobs, one `cancel_flag_new()` per file
and the `KMessageWidget` rows: one per job, plus an overall row for the batch.
Progress arrives through the `HelperStruct` `ProgressFunc` callback and updates
the row; closing a row cancels its flag. The rows live in the main window's top
strip.

The load path decompresses (`file_try_uncompress`), decodes with the matching
`LoadObjectBase`, calls the module (a multi-region plugin asks which regions to
load unless `selectAllRegionsInLoading`), records the detected
compression/encoding in `warnings()`, appends the regions and reports. The load
estimate (`DhCore::loadPrediction()`, the core's history plus the module's
estimate) is checked against the `maxLoadSeconds`/`maxLoadMemoryMB` settings
*before* decoding and refuses the file with a notification. Saving is the
mirror: the options are resolved once on the GUI thread
(`SaveAllRegionJob::resolveOutputConfig`, plugin switch or options dialog),
then `AutoLocker` holds every region being written until it finishes. Finishing
jobs report through `dh::tasks::report()`, which records the event in the
notification centre and writes one line to the log.

## Settings

A setting is added in the core's schema, not in the frontend.
`dhlrc_core_setting_count/info/choice` describe key, group, control type,
label, tooltip, choices, default and long-text flag; `DhCore::settings()`
mirrors that as `DhCore::SettingInfo`. `MainWindow` creates one
`DhSettingItem` per entry (`itemType()`/`itemValue()` map `DHLRC_SETTING_*` to
`DhSettingItem::Type`), then calls `DhSettingsDialog::addGroup()`/`addItem()`.
A key gets a control in this order:

1. a per-key creator passed to `addItem()` (used for `MemoryLimit`,
   `MultiRegionNamePattern`, `MultiRegionOrderRule` and `CacheDirectory`);
2. a per-type creator registered with `addTemplateByType()`;
3. the built-in template for the item's type (`DhBoolSettingTemplate` …
   `DhColourSettingTemplate`).

Plugin options are discovered at run time (`PluginOptionsConfig`) and added to
the `Manage` page as collapsible sections, with one `Use saved options for ...`
switch per direction and `setEnabledWhen()` greying the option rows while it is
off. `DhSettingsAssistant` runs extra work when registered and on every Apply;
`DhSetConfigAssistant` (`src/dhgameconfigui.h`) pushes the region defaults into
`libregion_rs` with `init_default_strings()`. The dialog is a separate shared
library (`src/dhsettingsdialog/`) with no KConfig dependency: it owns
`DhSettingItem`s and emits `saved()`.

Apply runs every template's `applyChange()`, then every assistant's
`applyHelp()`, then `detect()` and `saved()`. `MainWindow` handles `saved()` by
writing `DhCore::setSettingValue()` for every schema key, writing the plugin
options, then `DhCore::save()`, `refresh()` and `notifyConfigChanged()`.
Cancel/Esc put the controls back on the items without applying. The invariants
the self-check asserts are in [uicheck.md](uicheck.md).

## Cross-cutting

- Notifications: `MainWindow::showNotification()` reuses one row per `eventId`,
  mirrors the event into `NotificationCenterUI::instance()->record()` and
  hides the row after `NotificationTimeout`. `updateNotificationGate()` enables
  the core's desktop notifications unless `SuppressWhenActive` is set and the
  application is active.
- Log: `dh::log` (`src/dhlog.h`) installs a Qt message handler that writes
  `<cache>/logs/dhlrc-YYYY-MM-DD.log` and keeps the last 2000 messages in
  `dh::log::LogStore`. The Notifications & Log page shows them live (queued
  across threads) with filtering and copy. `--log-level`/`DHLRC_LOG_LEVEL` set
  the file threshold.
- Help: `DhHelpUI::showHelp(topic)` raises one window. Topics are the table in
  `src/dhhelpui.cpp` (id, `N_()` title, resource); articles are
  `resources/<name>.md` with `resources/<name>.<locale>.md` preferred when the
  catalogue locale matches (see `catalogLocale()`). `resources/dhlrc.qrc`
  compiles them in.
- Translations: each `.cpp` defines `#define _(str) gettext(str)`; KF6 strings
  use `i18n()`. `N_()` marks a string for extraction when it is translated
  later. The catalogues and `make_po.sh` are covered in [build.md](build.md).
- Region file dialogs: build the filter list from `ModuleBase::filter()`
  (already translated by the plugin) and join with `";;"`; open and save
  dialogs use the same list.
- 3D view: `DhRenderUI` (toolbar, options, download) shows a `DhRenderWidget`,
  which draws through `dhlrc_render.h` and falls back from `DhGpuWidget`
  (OpenGL) to the CPU rasteriser when no GL context works. See
  [render.md](render.md).

## Shared pieces

| File | Use it for |
| --- | --- |
| `src/utility.h` | `dh::` translation/config/cache dirs, date formatting, name patterns, choice labels. |
| `src/dhcore.h` | Core wrapper: settings, config snapshot, prediction, plugins, notifications. |
| `src/configobjectui.h` | Plugin options: `getObject(library, kind, ask, &cancelled)`, never by hand. |
| `src/generalchoosedialog.h` | Pick one/many entries, with optional load-order controls. |
| `src/dhtableview.h` | `QTableView` with external drops and `viewResized()`. |
| `src/flowlayout.h` | Wrapping toolbar layout for rows that must survive narrow windows. |
| `src/modulelibrary.h` | Symbol lookup on a core-owned plugin; never `dlopen`. |
| `src/detachabletabwidget.h` | Tab bar with per-page tear-off/dock. |
| `src/dhwidget.h` | Page/window base: closes with the window, emits `windowClosed()`. |

## Recipes

- **Add a settings key** — add it to the core schema; it appears on its group's
  page and is saved by the existing apply path. Add a creator in `MainWindow`
  only when the control needs custom behaviour.
- **Add a page/tab** — make it a `DhWidget`, open it with
  `MainWindow::addKeyedTab(widget, title, key)` (or `addWidgetToTab()` when
  there is only ever one) and give it a unique key. For restore, add a type
  string to `saveSession()`/`restoreSession()`/`sessionTabKey()`.
- **Add an undoable region operation** — write through `RegionClass`, keep the
  previous value, then call
  `recordChange(description, detail, [old](RegionClass &r) { return r.set...(old); })`
  on the GUI thread. It shows up in `RegionChangesUI` automatically.
- **Add a help article** — add `resources/<name>.md` (plus `.<locale>.md` when
  translated), list both in `resources/dhlrc.qrc`, and add a `Topic` row in
  `src/dhhelpui.cpp` with an `N_()` title.
- **Add a translated string** — wrap it in `_()` (or `i18n()`) and run
  `make_po.sh` (see [build.md](build.md)).

## Gotchas

- **ABI ownership**: the rule is in [architecture.md](architecture.md); in the
  frontend, results from `region.h` and plugin error strings go through
  `string_free()`, and the `char *` returns documented as owned in
  `dhlrc_core.h` through `dhlrc_string_free()`.
- **QObject ownership**: widgets are parented where possible; pages are
  `close()`d and then `deleteLater()`d. Torn-off windows are in no Qt parent
  chain, so their owner closes them (`RegionWorkspace::closeEvent()` calls
  `closeFloatingWindows()`; `MainWindow` takes down its floating pages in its
  destructor). Rows use `QPointer` because a job can outlive its
  `KMessageWidget`.
- **Threads**: only the GUI thread may touch pages, the change list
  (`recordChange`) and the settings dialog. Workers use the `RegionClass`
  accessors (they self-lock), `ManageRegionUI::appendRegion()` (takes the
  mutex) and `dh::log`; cross-thread config/notification delivery is queued
  (see [architecture.md](architecture.md)).
- **New files**: add `.cpp`/`.h` to `TEMP_PROJECT_SOURCES` in
  `src/CMakeLists.txt`, `.ui` files to `ui_files` there, and resources to
  `resources/dhlrc.qrc`. Settings-library files go to
  `src/dhsettingsdialog/CMakeLists.txt`. `--ui-check` sources compile only with
  `DH_UI_CHECK`, `--preview` only with `DH_BUILD_RENDER_CHECK` (see
  [build.md](build.md)).
