/*! C interface to the core, for frontends that are not Rust.
 *
 * Hand-written and small, so the ABI stays greppable. The matching declarations
 * live in `include/dhlrc_core.h`, which must be kept in step with the struct
 * layouts below.
 */

use crate::config::{
    Config, ConfigPaths, ConfigWatch, ConfigWatcher, OptionValue, PluginDirection, ShowOption,
    Unit, default_paths, load_or_create, save_config,
};
use crate::modules::Modules;
use crate::notification::Level;
use crate::resource::{Identifier, ResourceEnvironment, ResourceSet};
use std::ffi::{CStr, CString, c_char, c_void};
use std::path::PathBuf;
use std::ptr;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex, OnceLock};

/* Kept in sync with the `DHLRC_LEVEL_*` values in the header. */
const LEVEL_INFO: i32 = 0;
const LEVEL_WARNING: i32 = 1;
const LEVEL_ERROR: i32 = 2;

/* Kept in sync with `DHLRC_UNIT_*`. */
const UNIT_GIB: i32 = 0;
const UNIT_MIB: i32 = 1;
const UNIT_KIB: i32 = 2;
const UNIT_BYTES: i32 = 3;

/* Kept in sync with `DHLRC_SHOW_*`. */
const SHOW_PALETTE: i32 = 0;
const SHOW_NAME: i32 = 1;

/* Kept in sync with `DHLRC_KIND_*`: which symbol set an option belongs to. */
const KIND_INPUT: i32 = 0;
const KIND_OUTPUT: i32 = 1;

/// Called once per notification while opening.
pub type DhlrcNotifyFunc = extern "C" fn(
    user_data: *mut c_void,
    level: i32,
    event_id: *const c_char,
    title: *const c_char,
    text: *const c_char,
);

/// Called from the watch thread when the configuration file changed. The
/// strings describe the change (a `Notification`) and are only valid for the
/// duration of the call.
pub type DhlrcChangeFunc = extern "C" fn(
    user_data: *mut c_void,
    level: i32,
    event_id: *const c_char,
    title: *const c_char,
    text: *const c_char,
);

/// Called with a status line while a download runs; `message` is borrowed and
/// only valid for the call.
pub type DhlrcDownloadInfoFunc = extern "C" fn(user_data: *mut c_void, message: *const c_char);

/// Called with the bytes received and the total (`total` is 0 when the server
/// does not say). Runs on the download's thread.
pub type DhlrcDownloadProgressFunc =
    extern "C" fn(user_data: *mut c_void, received: u64, total: u64);

/// Polled between reads; a non-zero return stops the download.
pub type DhlrcCancelFunc = extern "C" fn(user_data: *mut c_void) -> i32;

/// A snapshot of the configuration. The layout must match `DhlrcConfigView` in
/// the header, field for field.
#[repr(C)]
pub struct DhlrcConfigView {
    pub memory_limit: i64,
    pub limit_unit: i32,
    pub elapsed_milliseconds: i64,
    pub select_all_regions_in_loading: bool,
    /// Try the other modules when the one the file extension points at fails.
    pub fail_then_retry: bool,
    pub fail_download_use_cache: bool,
    pub default_show_option: i32,
    pub override_setting: bool,
    pub base_name: *const c_char,
    pub region_name: *const c_char,
    pub multi_region_name_pattern: *const c_char,
    pub name_pattern_sample_file: *const c_char,
    pub name_pattern_sample_region: *const c_char,
    pub description: *const c_char,
    pub author: *const c_char,
    pub override_version: *const c_char,
    pub cache_directory: *const c_char,
    pub language: *const c_char,
}

/// The view plus the `CString`s its pointers point into. `view` is the first
/// field, so a `ConfigViewData *` can be handed out as a `DhlrcConfigView *`.
#[repr(C)]
struct ConfigViewData {
    view: DhlrcConfigView,
    strings: Vec<CString>,
}

impl ConfigViewData {
    fn new(config: &Config) -> Self {
        let mut strings: Vec<CString> = Vec::new();
        let view = DhlrcConfigView {
            memory_limit: i64::from(config.general.memory_limit),
            limit_unit: unit_to_int(config.general.limit_unit),
            elapsed_milliseconds: i64::from(config.general.elapsed_milliseconds),
            select_all_regions_in_loading: config.general.select_all_regions_in_loading,
            fail_then_retry: config.general.fail_then_retry,
            fail_download_use_cache: config.general.fail_download_use_cache,
            default_show_option: show_to_int(config.reader.default_show_option),
            override_setting: config.game.override_setting,
            base_name: owned(&mut strings, &config.defaults.base_name()),
            region_name: owned(&mut strings, &config.defaults.region_name()),
            multi_region_name_pattern: owned(
                &mut strings,
                &config.defaults.multi_region_name_pattern(),
            ),
            name_pattern_sample_file: owned(
                &mut strings,
                &config.defaults.name_pattern_sample_file,
            ),
            name_pattern_sample_region: owned(
                &mut strings,
                &config.defaults.name_pattern_sample_region,
            ),
            description: owned(&mut strings, &config.defaults.description),
            author: owned(&mut strings, &config.defaults.author),
            override_version: owned(&mut strings, &config.game.override_version),
            cache_directory: owned(&mut strings, &config.general.cache_dir().to_string_lossy()),
            language: owned(&mut strings, &config.general.language),
        };
        Self { view, strings }
    }
}

/// Copies `value` into a `CString`, keeps it alive in `strings` and returns a
/// pointer to its bytes, or NULL when it contains an interior NUL.
fn owned(strings: &mut Vec<CString>, value: &str) -> *const c_char {
    match CString::new(value) {
        Ok(string) => {
            let pointer = string.as_ptr();
            strings.push(string);
            pointer
        }
        Err(_) => ptr::null(),
    }
}

/// The part of the core the watch thread and the accessors share.
struct Shared {
    config: Mutex<Config>,
    /// Set false by the frontend to stop the desktop notifications, typically
    /// while its window is active.
    notifications_enabled: AtomicBool,
}

/// The handle a non-Rust frontend holds.
pub struct DhlrcCore {
    /// Cached `paths.file`, returned by `dhlrc_core_config_path`.
    path: CString,
    paths: ConfigPaths,
    shared: Arc<Shared>,
    watch: Mutex<Option<ConfigWatch>>,
    /// The plugins, loaded by `dhlrc_core_load_modules`.
    modules: Mutex<Option<Modules>>,
    /// The stacked resource sources; built on demand or by
    /// `dhlrc_core_resources_configure`.
    resources: Mutex<Option<ResourceSet>>,
    /// The blocks those resources have, built on first use and dropped when the
    /// resources are configured again.
    catalog: Mutex<Option<Arc<crate::catalog::Catalog>>>,
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_open(
    config_dir: *const c_char,
    notify: Option<DhlrcNotifyFunc>,
    notify_user_data: *mut c_void,
    error: *mut *mut c_char,
) -> *mut DhlrcCore {
    let paths = paths_for(config_dir);
    let init = match load_or_create(&paths) {
        Ok(init) => init,
        Err(err) => {
            set_error(error, &err.to_string());
            return ptr::null_mut();
        }
    };

    let notes = init.notifications();
    present_desktop(
        true,
        init.config.notifications.app_id.as_deref(),
        init.config.notifications.timeout_ms,
        &notes,
    );
    if let Some(notify) = notify {
        for note in &notes {
            // The strings only have to live for the call.
            let event = CString::new(note.event_id).unwrap_or_default();
            let title = CString::new(note.title.as_str()).unwrap_or_default();
            let text = CString::new(note.text.as_str()).unwrap_or_default();
            notify(
                notify_user_data,
                level_to_int(note.level),
                event.as_ptr(),
                title.as_ptr(),
                text.as_ptr(),
            );
        }
    }

    let path = CString::new(paths.file.to_string_lossy().to_string()).unwrap_or_default();
    let core = Box::new(DhlrcCore {
        path,
        shared: Arc::new(Shared {
            config: Mutex::new(init.config),
            notifications_enabled: AtomicBool::new(true),
        }),
        paths,
        watch: Mutex::new(None),
        modules: Mutex::new(None),
        resources: Mutex::new(None),
        catalog: Mutex::new(None),
    });
    Box::into_raw(core)
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_free(core: *mut DhlrcCore) {
    if core.is_null() {
        return;
    }
    let core = unsafe { Box::from_raw(core) };
    // Stop the watch before the rest is dropped, so no thread outlives it.
    if let Ok(mut guard) = core.watch.lock() {
        *guard = None;
    }
    drop(core);
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_config_path(core: *const DhlrcCore) -> *const c_char {
    if core.is_null() {
        return ptr::null();
    }
    unsafe { (*core).path.as_ptr() }
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_watch_start(
    core: *mut DhlrcCore,
    on_change: Option<DhlrcChangeFunc>,
    user_data: *mut c_void,
) -> i32 {
    if core.is_null() {
        return -1;
    }
    let Some(on_change) = on_change else {
        return -1;
    };
    let core = unsafe { &*core };
    stop_watch(core);

    let current = match core.shared.config.lock() {
        Ok(guard) => guard.clone(),
        Err(_) => return -1,
    };
    let shared = Arc::clone(&core.shared);
    let paths = core.paths.clone();
    // Carried as a `usize` so the closure is `Send`: the caller keeps `user_data`
    // valid until the watch is stopped.
    let user_address = user_data as usize;
    let watcher = ConfigWatcher::from_config(core.paths.clone(), current);

    match watcher.spawn(move |config| {
        let app_id = config.notifications.app_id.clone();
        let timeout_ms = config.notifications.timeout_ms;
        if let Ok(mut guard) = shared.config.lock() {
            *guard = config;
        }
        // Tell the frontend what changed; the strings only live for the call.
        let note = paths.reloaded_notification();
        let enabled = shared.notifications_enabled.load(Ordering::Relaxed);
        present_desktop(
            enabled,
            app_id.as_deref(),
            timeout_ms,
            std::slice::from_ref(&note),
        );
        let event = c_string(note.event_id);
        let title = c_string(&note.title);
        let text = c_string(&note.text);
        on_change(
            user_address as *mut c_void,
            level_to_int(note.level),
            event.as_ptr(),
            title.as_ptr(),
            text.as_ptr(),
        );
    }) {
        Ok(watch) => match core.watch.lock() {
            Ok(mut guard) => {
                *guard = Some(watch);
                0
            }
            Err(_) => -1,
        },
        Err(_) => -1,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_watch_stop(core: *mut DhlrcCore) {
    if core.is_null() {
        return;
    }
    stop_watch(unsafe { &*core });
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_config_view(core: *const DhlrcCore) -> *mut DhlrcConfigView {
    if core.is_null() {
        return ptr::null_mut();
    }
    let core = unsafe { &*core };
    let config = match core.shared.config.lock() {
        Ok(guard) => guard.clone(),
        Err(_) => return ptr::null_mut(),
    };
    // `view` is the first field, so the box pointer doubles as the view pointer.
    Box::into_raw(Box::new(ConfigViewData::new(&config))) as *mut DhlrcConfigView
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_config_view_free(view: *mut DhlrcConfigView) {
    if view.is_null() {
        return;
    }
    drop(unsafe { Box::from_raw(view as *mut ConfigViewData) });
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_string_free(string: *mut c_char) {
    if string.is_null() {
        return;
    }
    drop(unsafe { CString::from_raw(string) });
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_apply(
    core: *mut DhlrcCore,
    values: *const DhlrcConfigView,
    error: *mut *mut c_char,
) -> i32 {
    if core.is_null() || values.is_null() {
        set_error(error, "dhlrc_core_apply: null argument");
        return -1;
    }
    let core = unsafe { &*core };
    let view = unsafe { &*values };
    let base = match core.shared.config.lock() {
        Ok(guard) => guard.clone(),
        Err(_) => {
            set_error(error, "the configuration is being read");
            return -1;
        }
    };
    let config = config_from_view(view, &base);
    match core.shared.config.lock() {
        Ok(mut guard) => {
            *guard = config;
            0
        }
        Err(_) => {
            set_error(error, "the configuration is being read");
            -1
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_save(core: *const DhlrcCore, error: *mut *mut c_char) -> i32 {
    if core.is_null() {
        set_error(error, "dhlrc_core_save: null argument");
        return -1;
    }
    let core = unsafe { &*core };
    let config = match core.shared.config.lock() {
        Ok(guard) => guard.clone(),
        Err(_) => {
            set_error(error, "the configuration is being read");
            return -1;
        }
    };
    match save_config(&core.paths, &config) {
        Ok(()) => 0,
        Err(err) => {
            set_error(error, &err.to_string());
            -1
        }
    }
}

/// Turns the desktop notifications on or off. On by default; a frontend turns
/// them off while its window is active.
#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_set_notifications_enabled(core: *mut DhlrcCore, enabled: bool) {
    if core.is_null() {
        return;
    }
    let core = unsafe { &*core };
    core.shared
        .notifications_enabled
        .store(enabled, Ordering::Relaxed);
}

/// One module, as the C interface describes it. The pointers are borrowed from
/// the module the core loaded and stay valid until the modules are loaded
/// again or the core is freed; a NULL string means the module does not declare
/// that value.
#[repr(C)]
pub struct DhlrcModuleInfo {
    pub file: *const c_char,
    pub type_name: *const c_char,
    pub file_suffix: *const c_char,
    pub base_type: *const c_char,
    pub filter: *const c_char,
    pub is_multi: i32,
    pub encoding: i32,
    pub compression: i32,
}

fn optional_ptr(value: &Option<CString>) -> *const c_char {
    value.as_ref().map_or(ptr::null(), |string| string.as_ptr())
}

/// Opens every plugin next to the executable. Loading again replaces the
/// current set (and invalidates the pointers a previous `*_info` handed out).
#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_load_modules(core: *mut DhlrcCore, error: *mut *mut c_char) -> i32 {
    if core.is_null() {
        set_error(error, "dhlrc_core_load_modules: null argument");
        return -1;
    }
    let core = unsafe { &*core };
    let (region_dir, load_dir) = crate::modules::default_dirs();
    let modules = Modules::load(&region_dir, &load_dir);
    match core.modules.lock() {
        Ok(mut guard) => {
            *guard = Some(modules);
            0
        }
        Err(_) => {
            set_error(error, "the modules are being read");
            -1
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_region_module_count(core: *const DhlrcCore) -> usize {
    if core.is_null() {
        return 0;
    }
    let core = unsafe { &*core };
    match core.modules.lock() {
        Ok(guard) => guard.as_ref().map_or(0, Modules::region_count),
        Err(_) => 0,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_load_module_count(core: *const DhlrcCore) -> usize {
    if core.is_null() {
        return 0;
    }
    let core = unsafe { &*core };
    match core.modules.lock() {
        Ok(guard) => guard.as_ref().map_or(0, Modules::load_count),
        Err(_) => 0,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_region_module_info(
    core: *const DhlrcCore,
    index: usize,
    info: *mut DhlrcModuleInfo,
) -> i32 {
    if core.is_null() || info.is_null() {
        return -1;
    }
    let core = unsafe { &*core };
    let Ok(guard) = core.modules.lock() else {
        return -1;
    };
    let Some(module) = guard
        .as_ref()
        .and_then(|modules| modules.region_info(index))
    else {
        return -1;
    };
    fill_module_info(info, module);
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_load_module_info(
    core: *const DhlrcCore,
    index: usize,
    info: *mut DhlrcModuleInfo,
) -> i32 {
    if core.is_null() || info.is_null() {
        return -1;
    }
    let core = unsafe { &*core };
    let Ok(guard) = core.modules.lock() else {
        return -1;
    };
    let Some(module) = guard.as_ref().and_then(|modules| modules.load_info(index)) else {
        return -1;
    };
    fill_module_info(info, module);
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_region_module_symbol(
    core: *const DhlrcCore,
    index: usize,
    name: *const c_char,
) -> *mut c_void {
    module_symbol(core, index, name, true)
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_load_module_symbol(
    core: *const DhlrcCore,
    index: usize,
    name: *const c_char,
) -> *mut c_void {
    module_symbol(core, index, name, false)
}

fn fill_module_info(info: *mut DhlrcModuleInfo, module: &crate::modules::ModuleInfo) {
    let out = unsafe { &mut *info };
    out.file = module.file.as_ptr();
    out.type_name = optional_ptr(&module.type_name);
    out.file_suffix = optional_ptr(&module.file_suffix);
    out.base_type = optional_ptr(&module.base_type);
    out.filter = optional_ptr(&module.filter);
    out.is_multi = i32::from(module.is_multi);
    out.encoding = module.encoding;
    out.compression = module.compression;
}

fn module_symbol(
    core: *const DhlrcCore,
    index: usize,
    name: *const c_char,
    region: bool,
) -> *mut c_void {
    if core.is_null() || name.is_null() {
        return ptr::null_mut();
    }
    let core = unsafe { &*core };
    let name = unsafe { CStr::from_ptr(name) }.to_string_lossy();
    let Ok(guard) = core.modules.lock() else {
        return ptr::null_mut();
    };
    match guard.as_ref() {
        Some(modules) if region => modules.region_symbol(index, &name),
        Some(modules) => modules.load_symbol(index, &name),
        None => ptr::null_mut(),
    }
}

/// One setting, as the C interface describes it. The pointers are borrowed
/// from the static table and stay valid for the whole run; an `Enum` setting
/// has `choice_count` choices, read with `dhlrc_core_setting_choice`.
#[repr(C)]
pub struct DhlrcSettingInfo {
    pub key: *const c_char,
    pub group: *const c_char,
    pub kind: i32,
    pub label: *const c_char,
    pub tooltip: *const c_char,
    pub choice_count: usize,
    pub default_value: *const c_char,
    pub long_text: bool,
}

/// A `SettingDef` turned into NUL-terminated message ids, so a C frontend can
/// read them. Built once and kept for the run.
struct SettingRow {
    key: CString,
    group: CString,
    kind: i32,
    label: CString,
    tooltip: CString,
    choices: Vec<CString>,
    default_value: CString,
    long_text: bool,
}

/// The settings, in the order the schema lists them.
fn settings_table() -> &'static [SettingRow] {
    static TABLE: OnceLock<Vec<SettingRow>> = OnceLock::new();
    TABLE.get_or_init(|| {
        crate::schema::SETTINGS
            .iter()
            .map(|def| SettingRow {
                key: c_string(def.key),
                group: c_string(def.group),
                kind: def.kind.to_int(),
                label: c_string(def.label),
                tooltip: c_string(def.tooltip),
                choices: def.choices.iter().map(|choice| c_string(choice)).collect(),
                default_value: c_string(def.default),
                long_text: def.long_text,
            })
            .collect()
    })
}

fn c_string(value: &str) -> CString {
    CString::new(value).unwrap_or_default()
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_setting_count() -> usize {
    settings_table().len()
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_setting_info(index: usize, info: *mut DhlrcSettingInfo) -> i32 {
    if info.is_null() {
        return -1;
    }
    let Some(row) = settings_table().get(index) else {
        return -1;
    };
    let out = unsafe { &mut *info };
    out.key = row.key.as_ptr();
    out.group = row.group.as_ptr();
    out.kind = row.kind;
    out.label = row.label.as_ptr();
    out.tooltip = row.tooltip.as_ptr();
    out.choice_count = row.choices.len();
    out.default_value = row.default_value.as_ptr();
    out.long_text = row.long_text;
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_setting_choice(index: usize, choice: usize) -> *const c_char {
    settings_table()
        .get(index)
        .and_then(|row| row.choices.get(choice))
        .map_or(ptr::null(), |value| value.as_ptr())
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_setting_get(
    core: *const DhlrcCore,
    key: *const c_char,
) -> *mut c_char {
    if core.is_null() || key.is_null() {
        return ptr::null_mut();
    }
    let core = unsafe { &*core };
    let key = unsafe { CStr::from_ptr(key) }.to_string_lossy();
    let config = match core.shared.config.lock() {
        Ok(guard) => guard,
        Err(_) => return ptr::null_mut(),
    };
    match config.setting(&key) {
        Some(value) => match CString::new(value) {
            Ok(string) => string.into_raw(),
            Err(_) => ptr::null_mut(),
        },
        None => ptr::null_mut(),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_setting_set(
    core: *mut DhlrcCore,
    key: *const c_char,
    value: *const c_char,
) -> i32 {
    if core.is_null() || key.is_null() {
        return -1;
    }
    let core = unsafe { &*core };
    let key = unsafe { CStr::from_ptr(key) }
        .to_string_lossy()
        .into_owned();
    let value = from_c_required(value);
    match core.shared.config.lock() {
        Ok(mut guard) => {
            if guard.set_setting(&key, &value) {
                0
            } else {
                -1
            }
        }
        Err(_) => -1,
    }
}

/// The bytes of a file the library carries inside itself (see the `bundled`
/// module), or null when there is no such file. Borrowed and valid for the
/// whole run; the length is written to `length` when it is not null. The bytes
/// are not NUL-terminated, so callers must read `*length` of them.
#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_bundled_resource(
    name: *const c_char,
    length: *mut usize,
) -> *const c_char {
    let Some(name) = from_c(name) else {
        return ptr::null();
    };
    match crate::resource::bundled::get(&name) {
        Some(bytes) => {
            if !length.is_null() {
                unsafe { *length = bytes.len() };
            }
            bytes.as_ptr().cast::<c_char>()
        }
        None => ptr::null(),
    }
}

/// The material list of a region: every palette entry counted once, mapped to
/// the item(s) a player has to prepare, and summed per item. `region` is a
/// handle from the region module (`region_new`), borrowed for the call.
///
/// Returns an owned JSON object (free it with `dhlrc_string_free`), or NULL
/// when `region` is NULL. The shape is documented on `crate::material`.
#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_material_list(region: *const common_rs::region::Region) -> *mut c_char {
    let Some(region) = (unsafe { region.as_ref() }) else {
        return ptr::null_mut();
    };
    CString::new(crate::material::material_json(region))
        .map(CString::into_raw)
        .unwrap_or(ptr::null_mut())
}

/// The PNG bytes of an item's icon, for a frontend that shows a material list
/// or an ingredient panel. Looked up through the resource set: the item's own
/// texture, then the block texture of the same name, then the first texture the
/// block model names (which is how a stair or a door gets an icon).
///
/// `length` receives the byte count (it may be NULL). The bytes are owned; free
/// them with `dhlrc_core_bytes_free`. NULL when the resources are not configured
/// or nothing was found.
#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_item_texture(
    core: *const DhlrcCore,
    item: *const c_char,
    length: *mut usize,
) -> *mut u8 {
    let Some(core) = (unsafe { core.as_ref() }) else {
        return ptr::null_mut();
    };
    let Some(item) = from_c(item) else {
        return ptr::null_mut();
    };
    let Some(bytes) = with_resources(core, |set| item_texture(set, &item)).flatten() else {
        return ptr::null_mut();
    };
    let mut boxed = bytes.into_boxed_slice();
    let len = boxed.len();
    let pointer = boxed.as_mut_ptr();
    std::mem::forget(boxed);
    if !length.is_null() {
        unsafe { *length = len };
    }
    pointer
}

/// Frees bytes returned by `dhlrc_core_item_texture`.
#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_bytes_free(bytes: *mut u8, length: usize) {
    if bytes.is_null() {
        return;
    }
    let slice = ptr::slice_from_raw_parts_mut(bytes, length);
    drop(unsafe { Box::from_raw(slice) });
}

/// The icon bytes for `item`, trying the place a texture usually lives in
/// before falling back to the block model that names one.
fn item_texture(set: &ResourceSet, item: &str) -> Option<Vec<u8>> {
    let (namespace, path) = item.split_once(':').unwrap_or(("minecraft", item));
    for candidate in [
        format!("{namespace}:textures/item/{path}.png"),
        format!("{namespace}:textures/block/{path}.png"),
    ] {
        if let Some(bytes) = set.get_bytes(&Identifier::parse(&candidate)) {
            return Some(bytes);
        }
    }

    // A stair, a slab or a door has no texture of its own name: its block model
    // lists one under `textures`. A bed or a two-block flower is modelled as an
    // item instead, so that model is the next place to look.
    for model in [
        format!("{namespace}:models/block/{path}.json"),
        format!("{namespace}:models/item/{path}.json"),
    ] {
        if let Some(bytes) = model_texture(set, &model) {
            return Some(bytes);
        }
    }
    None
}

/// The first texture a model names, as the PNG it points at.
fn model_texture(set: &ResourceSet, model: &str) -> Option<Vec<u8>> {
    let model = set.get_json(&Identifier::parse(model))?;
    for reference in model.get("textures")?.as_object()?.values() {
        let Some(reference) = reference.as_str() else {
            continue;
        };
        let reference = if reference.contains(':') {
            reference.to_string()
        } else {
            format!("minecraft:{reference}")
        };
        // `minecraft:block/oak_planks` -> `minecraft:textures/block/oak_planks.png`
        let texture = reference.replacen(':', ":textures/", 1) + ".png";
        if let Some(bytes) = set.get_bytes(&Identifier::parse(&texture)) {
            return Some(bytes);
        }
    }
    None
}

/// Points the core at its resource files: the cache directory the downloader
/// filled, the version whose client jar to use (`version` may be NULL), and
/// `pack_count` resource packs / mod jars, highest priority first (`packs` may
/// be NULL). Replaces any previous set. Returns 0, or -1 on a bad handle.
/// The blocks the configured resources have, built on first use.
fn catalog_of(core: &DhlrcCore) -> Option<Arc<crate::catalog::Catalog>> {
    if let Ok(guard) = core.catalog.lock()
        && let Some(cached) = guard.as_ref()
    {
        return Some(Arc::clone(cached));
    }
    let built = with_resources(core, |resources| {
        let source: &dyn dhlrc_resource::Source = resources;
        crate::catalog::Catalog::build(source)
    })?;
    if let Ok(mut guard) = core.catalog.lock() {
        *guard = Some(Arc::clone(&built));
    }
    Some(built)
}

/// Looks one block up, with its properties, for the getters below.
fn block_of(
    core: *mut DhlrcCore,
    index: usize,
) -> Option<Arc<crate::catalog::Catalog>> {
    let core = unsafe { core.as_ref() }?;
    let catalog = catalog_of(core)?;
    (index < catalog.blocks.len()).then_some(catalog)
}

/// How many blocks the configured resources have, or 0 when there are none yet
/// (nothing configured, or a version whose jar is not there).
///
/// The strings the getters below return are borrowed from the core and stay
/// valid until the resources are configured again; they are not to be freed.
///
/// # Safety
///
/// `core` must be a pointer this library returned.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn dhlrc_core_block_count(core: *mut DhlrcCore) -> usize {
    unsafe { core.as_ref() }
        .and_then(catalog_of)
        .map_or(0, |catalog| catalog.blocks.len())
}

/// The id of the block at `index` (`minecraft:oak_stairs`), or null.
///
/// # Safety
///
/// `core` must be a pointer this library returned.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn dhlrc_core_block_name(
    core: *mut DhlrcCore,
    index: usize,
) -> *const c_char {
    let Some(catalog) = block_of(core, index) else {
        return ptr::null();
    };
    catalog.blocks[index].name.as_ptr()
}

/// How many properties the block at `index` takes.
///
/// # Safety
///
/// `core` must be a pointer this library returned.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn dhlrc_core_block_property_count(
    core: *mut DhlrcCore,
    index: usize,
) -> usize {
    block_of(core, index)
        .map_or(0, |catalog| catalog.blocks[index].properties.len())
}

/// The name of one property of the block at `index`, or null.
///
/// # Safety
///
/// `core` must be a pointer this library returned.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn dhlrc_core_block_property_name(
    core: *mut DhlrcCore,
    index: usize,
    property: usize,
) -> *const c_char {
    let Some(catalog) = block_of(core, index) else {
        return ptr::null();
    };
    match catalog.blocks[index].properties.get(property) {
        Some((name, _)) => name.as_ptr(),
        None => ptr::null(),
    }
}

/// How many values a property of the block at `index` takes.
///
/// # Safety
///
/// `core` must be a pointer this library returned.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn dhlrc_core_block_property_value_count(
    core: *mut DhlrcCore,
    index: usize,
    property: usize,
) -> usize {
    let Some(catalog) = block_of(core, index) else {
        return 0;
    };
    match catalog.blocks[index].properties.get(property) {
        Some((_, values)) => values.len(),
        None => 0,
    }
}

/// One value a property of the block at `index` takes, or null.
///
/// # Safety
///
/// `core` must be a pointer this library returned.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn dhlrc_core_block_property_value(
    core: *mut DhlrcCore,
    index: usize,
    property: usize,
    value: usize,
) -> *const c_char {
    let Some(catalog) = block_of(core, index) else {
        return ptr::null();
    };
    match catalog.blocks[index].properties.get(property) {
        Some((_, values)) => match values.get(value) {
            Some(text) => text.as_ptr(),
            None => ptr::null(),
        },
        None => ptr::null(),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_resources_configure(
    core: *mut DhlrcCore,
    cache_dir: *const c_char,
    version: *const c_char,
    packs: *const *const c_char,
    pack_count: usize,
) -> i32 {
    let Some(core) = (unsafe { core.as_ref() }) else {
        return -1;
    };
    let environment = ResourceEnvironment {
        cache_dir: PathBuf::from(from_c_required(cache_dir)),
        version: from_c(version).filter(|value| !value.is_empty()),
        packs: collect_paths(packs, pack_count),
    };
    match core.resources.lock() {
        Ok(mut guard) => {
            *guard = Some(environment.resource_set());
            /* A different jar is a different set of blocks. */
            if let Ok(mut catalog) = core.catalog.lock() {
                *catalog = None;
            }
            0
        }
        Err(_) => -1,
    }
}

/// Points the core at the resources of one version, resolved from a region's
/// data version: the version's client jar under the configured cache
/// directory, plus any resource packs and mod jars. This is what a frontend
/// that draws calls before building a scene — the block models and textures
/// are only in the client jar, so without it a scene has nothing to draw.
///
/// The module's own `cache_dir` comes from the configuration, so a caller only
/// names the version and the packs. Returns 0, or -1 when the data version
/// names no known version or the handle is bad.
#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_resources_configure_for_data_version(
    core: *mut DhlrcCore,
    data_version: i32,
    packs: *const *const c_char,
    pack_count: usize,
) -> i32 {
    let Some(core) = (unsafe { core.as_ref() }) else {
        return -1;
    };
    let Some(version) = crate::download::version_id(data_version) else {
        return -1;
    };
    let cache_dir = core
        .shared
        .config
        .lock()
        .ok()
        .map(|config| config.general.cache_dir())
        .unwrap_or_default();
    let environment = ResourceEnvironment {
        cache_dir,
        version: Some(version),
        packs: collect_paths(packs, pack_count),
    };
    match core.resources.lock() {
        Ok(mut guard) => {
            *guard = Some(environment.resource_set());
            if let Ok(mut catalog) = core.catalog.lock() {
                *catalog = None;
            }
            0
        }
        Err(_) => -1,
    }
}

/// The localized name of a block or item (`minecraft:oak_log`) in `locale`
/// (`zh_cn`), looked up through the resource set. Owned; free it with
/// `dhlrc_string_free`. NULL when there is no name for it.
///
/// The set is built from the configuration the first time it is needed, so a
/// frontend happy with the defaults never has to call
/// `dhlrc_core_resources_configure`.
#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_translate(
    core: *const DhlrcCore,
    locale: *const c_char,
    name: *const c_char,
) -> *mut c_char {
    let Some(core) = (unsafe { core.as_ref() }) else {
        return ptr::null_mut();
    };
    let (Some(locale), Some(name)) = (from_c(locale), from_c(name)) else {
        return ptr::null_mut();
    };
    let Some(translated) = with_resources(core, |set| {
        crate::resource::translate::translate(set, &locale, &name)
    }) else {
        return ptr::null_mut();
    };
    match translated {
        Some(text) => CString::new(text)
            .map(CString::into_raw)
            .unwrap_or(ptr::null_mut()),
        None => ptr::null_mut(),
    }
}

/// Runs `with` against the core's resource set, building the default one from
/// the configuration the first time it is needed.
fn with_resources<T>(core: &DhlrcCore, with: impl FnOnce(&ResourceSet) -> T) -> Option<T> {
    let mut guard = core.resources.lock().ok()?;
    if guard.is_none() {
        let (cache_dir, version) = core
            .shared
            .config
            .lock()
            .ok()
            .map(|config| {
                let version = config.game.override_version.trim().to_owned();
                (
                    config.general.cache_dir(),
                    (!version.is_empty()).then_some(version),
                )
            })
            .unwrap_or_default();
        *guard = Some(
            ResourceEnvironment {
                cache_dir,
                version,
                ..ResourceEnvironment::default()
            }
            .resource_set(),
        );
    }
    guard.as_ref().map(with)
}

impl DhlrcCore {
    /// Runs `with` against the core's resource set, building the default one
    /// from the configuration the first time. This is the Rust-side seam for a
    /// library that reads the same resources — the renderer does — so it does
    /// not have to rebuild the set the frontend already configured, nor go
    /// through the C ABI for something that is Rust on both sides.
    pub fn resources<T>(&self, with: impl FnOnce(&ResourceSet) -> T) -> Option<T> {
        with_resources(self, with)
    }
}

/// Reads a `const char *const *` array of `count` strings into paths.
fn collect_paths(pointer: *const *const c_char, count: usize) -> Vec<PathBuf> {
    if pointer.is_null() {
        return Vec::new();
    }
    (0..count)
        .filter_map(|index| from_c(unsafe { *pointer.add(index) }))
        .filter(|path| !path.is_empty())
        .map(PathBuf::from)
        .collect()
}

/// Fetches `object` (`minecraft/lang/zh_cn.json`) for the version with data
/// version `version`, following the manifest and the asset index into the
/// cache directory the core resolved. Blocking, so a frontend runs it off its
/// own thread; `info`, `progress` and `cancel` may be NULL and, when set, run on
/// that thread. Returns 0 when the file is in place, or -1 with an owned
/// `*error`.
#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_download_object(
    core: *mut DhlrcCore,
    version: i32,
    object: *const c_char,
    info: Option<DhlrcDownloadInfoFunc>,
    progress: Option<DhlrcDownloadProgressFunc>,
    cancel: Option<DhlrcCancelFunc>,
    user_data: *mut c_void,
    error: *mut *mut c_char,
) -> i32 {
    let Some(core) = (unsafe { core.as_ref() }) else {
        set_error(error, "dhlrc_core_download_object: null core");
        return -1;
    };
    let Some(object) = from_c(object) else {
        set_error(error, "dhlrc_core_download_object: null object");
        return -1;
    };
    let Ok(config) = core.shared.config.lock().map(|guard| guard.clone()) else {
        set_error(
            error,
            "dhlrc_core_download_object: configuration lock poisoned",
        );
        return -1;
    };

    let mut downloader = crate::download::Downloader::new(config.general.cache_dir());
    downloader.set_prefer_cache(config.general.fail_download_use_cache);

    let sink = DownloadSink {
        info,
        progress,
        user_data,
    };
    let stop = DownloadCancel { cancel, user_data };

    match downloader.download_object(version, &object, &sink, &stop) {
        Ok(_) => 0,
        Err(problem) => {
            set_error(error, &problem.to_string());
            -1
        }
    }
}

/// The cache directory the configuration names, or the platform default.
fn configured_cache_dir(core: &DhlrcCore) -> PathBuf {
    core.shared
        .config
        .lock()
        .ok()
        .map(|config| config.general.cache_dir())
        .unwrap_or_default()
}

/// Where the client jar of the version `data_version` names belongs in the
/// cache directory, as an owned string. NULL when the handle is bad or the data
/// version names no known version. The file may or may not be there; ask
/// `dhlrc_core_client_jar_state` for that.
///
/// This is the file a 3D preview reads a version's block models and textures
/// from, so a frontend can show the path to a user who has to fetch it by hand.
#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_client_jar_path(
    core: *const DhlrcCore,
    data_version: i32,
) -> *mut c_char {
    let Some(core) = (unsafe { core.as_ref() }) else {
        return ptr::null_mut();
    };
    let Some(version) = crate::download::version_id(data_version) else {
        return ptr::null_mut();
    };
    let path = crate::resource::cache::client_jar(&configured_cache_dir(core), &version);
    CString::new(path.to_string_lossy().as_bytes())
        .map(CString::into_raw)
        .unwrap_or(ptr::null_mut())
}

/// 1 when the version's client jar is in the cache, 0 when it is not, and -1
/// when the handle is bad or the data version names no known version.
#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_client_jar_state(
    core: *const DhlrcCore,
    data_version: i32,
) -> i32 {
    let Some(core) = (unsafe { core.as_ref() }) else {
        return -1;
    };
    let Some(version) = crate::download::version_id(data_version) else {
        return -1;
    };
    i32::from(crate::download::client_jar_is_there(
        &configured_cache_dir(core),
        &version,
    ))
}

/// Downloads the client jar of the version `data_version` names into the cache
/// directory's `client_jar/` folder, which is where the renderer reads a
/// version's block models and textures from. An empty error when there is
/// nothing to fetch.
///
/// Blocking, so a frontend runs it off its own thread; `info`, `progress` and
/// `cancel` behave as they do for `dhlrc_core_download_object`. Returns 0 when
/// the jar is in place, or -1 with an owned `*error`.
#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_download_client_jar(
    core: *mut DhlrcCore,
    data_version: i32,
    info: Option<DhlrcDownloadInfoFunc>,
    progress: Option<DhlrcDownloadProgressFunc>,
    cancel: Option<DhlrcCancelFunc>,
    user_data: *mut c_void,
    error: *mut *mut c_char,
) -> i32 {
    let Some(core) = (unsafe { core.as_ref() }) else {
        set_error(error, "dhlrc_core_download_client_jar: null core");
        return -1;
    };
    let Ok(config) = core.shared.config.lock().map(|guard| guard.clone()) else {
        set_error(
            error,
            "dhlrc_core_download_client_jar: configuration lock poisoned",
        );
        return -1;
    };

    let mut downloader = crate::download::Downloader::new(config.general.cache_dir());
    downloader.set_prefer_cache(config.general.fail_download_use_cache);

    let sink = DownloadSink {
        info,
        progress,
        user_data,
    };
    let stop = DownloadCancel { cancel, user_data };

    match downloader.download_client(data_version, &sink, &stop) {
        Ok(_) => 0,
        Err(problem) => {
            set_error(error, &problem.to_string());
            -1
        }
    }
}

/// Forwards a [`Sink`](crate::download::Sink) to the C callbacks (either may be
/// absent).
struct DownloadSink {
    info: Option<DhlrcDownloadInfoFunc>,
    progress: Option<DhlrcDownloadProgressFunc>,
    user_data: *mut c_void,
}

impl crate::download::Sink for DownloadSink {
    fn info(&self, message: &str) {
        if let Some(callback) = self.info {
            if let Ok(text) = CString::new(message) {
                callback(self.user_data, text.as_ptr());
            }
        }
    }

    fn progress(&self, received: u64, total: u64) {
        if let Some(callback) = self.progress {
            callback(self.user_data, received, total);
        }
    }
}

/// Forwards a [`Cancel`](crate::download::Cancel) to the C callback (absent
/// means "never").
struct DownloadCancel {
    cancel: Option<DhlrcCancelFunc>,
    user_data: *mut c_void,
}

impl crate::download::Cancel for DownloadCancel {
    fn cancelled(&self) -> bool {
        self.cancel
            .map(|callback| callback(self.user_data) != 0)
            .unwrap_or(false)
    }
}

/// A `const char *` the caller passed: NULL means unset.
fn from_c(pointer: *const c_char) -> Option<String> {
    if pointer.is_null() {
        return None;
    }
    Some(
        unsafe { CStr::from_ptr(pointer) }
            .to_string_lossy()
            .into_owned(),
    )
}

/// A `const char *` the caller passed: NULL becomes the empty string.
fn from_c_required(pointer: *const c_char) -> String {
    from_c(pointer).unwrap_or_default()
}

/// Applies a view's fields onto a copy of `base`; anything the view does not
/// carry (the plugin options) is kept.
fn config_from_view(view: &DhlrcConfigView, base: &Config) -> Config {
    let mut config = base.clone();
    config.general.memory_limit = view.memory_limit.max(0) as u32;
    config.general.limit_unit = int_to_unit(view.limit_unit);
    config.general.elapsed_milliseconds = view.elapsed_milliseconds.max(0) as u32;
    config.general.select_all_regions_in_loading = view.select_all_regions_in_loading;
    config.general.fail_then_retry = view.fail_then_retry;
    config.general.fail_download_use_cache = view.fail_download_use_cache;
    config.general.cache_directory = from_c(view.cache_directory)
        .filter(|value| !value.is_empty())
        .map(PathBuf::from);
    if let Some(language) = from_c(view.language).filter(|value| !value.is_empty()) {
        config.general.language = language;
    }
    config.defaults.base_name = from_c(view.base_name);
    config.defaults.region_name = from_c(view.region_name);
    config.defaults.multi_region_name_pattern = from_c(view.multi_region_name_pattern);
    config.defaults.name_pattern_sample_file = from_c_required(view.name_pattern_sample_file);
    config.defaults.name_pattern_sample_region = from_c_required(view.name_pattern_sample_region);
    config.defaults.description = from_c_required(view.description);
    config.defaults.author = from_c_required(view.author);
    config.game.override_setting = view.override_setting;
    config.game.override_version = from_c_required(view.override_version);
    config.reader.default_show_option = int_to_show(view.default_show_option);
    config
}

fn int_to_unit(value: i32) -> Unit {
    match value {
        UNIT_MIB => Unit::Mib,
        UNIT_KIB => Unit::Kib,
        UNIT_BYTES => Unit::Bytes,
        _ => Unit::Gib,
    }
}

fn int_to_show(value: i32) -> ShowOption {
    match value {
        SHOW_NAME => ShowOption::Name,
        _ => ShowOption::Palette,
    }
}

/// What a plugin that has never been configured does for one direction.
///
/// Reading uses the saved options without asking: a load does not know which
/// plugin will end up reading the file until it has been decoded, so asking
/// about every plugin would be noise. Writing asks, because the format was
/// picked on purpose and its options are per export. Either way the switch on
/// the plugin's page in the settings can turn the question the other way.
fn default_use_configured(kind: i32) -> bool {
    kind != KIND_OUTPUT
}

/// One plugin's options for one direction, read-only.
fn direction<'a>(config: &'a Config, type_name: &str, kind: i32) -> Option<&'a PluginDirection> {
    let entry = config.plugins.get(type_name)?;
    Some(if kind == KIND_OUTPUT {
        &entry.output
    } else {
        &entry.input
    })
}

/// One plugin's options for one direction, created on demand.
fn direction_mut<'a>(
    config: &'a mut Config,
    type_name: &str,
    kind: i32,
) -> &'a mut PluginDirection {
    let entry = config.plugins.entry(type_name.to_owned()).or_default();
    if kind == KIND_OUTPUT {
        &mut entry.output
    } else {
        &mut entry.input
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_plugin_use_configured(
    core: *const DhlrcCore,
    type_name: *const c_char,
    kind: i32,
) -> i32 {
    if core.is_null() {
        return 0;
    }
    let core = unsafe { &*core };
    let type_name = from_c_required(type_name);
    let flags = match core.shared.config.lock() {
        Ok(guard) => direction(&guard, &type_name, kind)
            .map(|dir| dir.use_configured)
            .unwrap_or_else(|| default_use_configured(kind)),
        Err(_) => default_use_configured(kind),
    };
    i32::from(flags)
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_plugin_set_use_configured(
    core: *mut DhlrcCore,
    type_name: *const c_char,
    kind: i32,
    value: bool,
) {
    if core.is_null() {
        return;
    }
    let core = unsafe { &*core };
    let type_name = from_c_required(type_name);
    if let Ok(mut guard) = core.shared.config.lock() {
        direction_mut(&mut guard, &type_name, kind).use_configured = value;
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_plugin_get_bool(
    core: *const DhlrcCore,
    type_name: *const c_char,
    kind: i32,
    key: *const c_char,
    out: *mut bool,
) -> i32 {
    if core.is_null() || out.is_null() {
        return 0;
    }
    let core = unsafe { &*core };
    let type_name = from_c_required(type_name);
    let key = from_c_required(key);
    let value = match core.shared.config.lock() {
        Ok(guard) => {
            match direction(&guard, &type_name, kind).and_then(|dir| dir.options.get(&key)) {
                Some(OptionValue::Bool(value)) => Some(*value),
                _ => None,
            }
        }
        Err(_) => None,
    };
    match value {
        Some(value) => {
            unsafe { *out = value };
            1
        }
        None => 0,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_plugin_set_bool(
    core: *mut DhlrcCore,
    type_name: *const c_char,
    kind: i32,
    key: *const c_char,
    value: bool,
) {
    if core.is_null() {
        return;
    }
    let core = unsafe { &*core };
    let type_name = from_c_required(type_name);
    let key = from_c_required(key);
    if let Ok(mut guard) = core.shared.config.lock() {
        direction_mut(&mut guard, &type_name, kind)
            .options
            .insert(key, OptionValue::Bool(value));
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_plugin_get_int(
    core: *const DhlrcCore,
    type_name: *const c_char,
    kind: i32,
    key: *const c_char,
    out: *mut i64,
) -> i32 {
    if core.is_null() || out.is_null() {
        return 0;
    }
    let core = unsafe { &*core };
    let type_name = from_c_required(type_name);
    let key = from_c_required(key);
    let value = match core.shared.config.lock() {
        Ok(guard) => {
            match direction(&guard, &type_name, kind).and_then(|dir| dir.options.get(&key)) {
                Some(OptionValue::Int(value)) => Some(*value),
                _ => None,
            }
        }
        Err(_) => None,
    };
    match value {
        Some(value) => {
            unsafe { *out = value };
            1
        }
        None => 0,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn dhlrc_core_plugin_set_int(
    core: *mut DhlrcCore,
    type_name: *const c_char,
    kind: i32,
    key: *const c_char,
    value: i64,
) {
    if core.is_null() {
        return;
    }
    let core = unsafe { &*core };
    let type_name = from_c_required(type_name);
    let key = from_c_required(key);
    if let Ok(mut guard) = core.shared.config.lock() {
        direction_mut(&mut guard, &type_name, kind)
            .options
            .insert(key, OptionValue::Int(value));
    }
}

fn stop_watch(core: &DhlrcCore) {
    if let Ok(mut guard) = core.watch.lock() {
        *guard = None;
    }
}

fn set_error(error: *mut *mut c_char, message: &str) {
    if error.is_null() {
        return;
    }
    let message = CString::new(message).unwrap_or_default();
    unsafe { *error = message.into_raw() };
}

/// The paths for `config_dir`, or the default location when it is NULL/empty.
fn paths_for(config_dir: *const c_char) -> ConfigPaths {
    let dir = if config_dir.is_null() {
        String::new()
    } else {
        unsafe { CStr::from_ptr(config_dir) }
            .to_string_lossy()
            .into_owned()
    };
    if dir.is_empty() {
        return default_paths();
    }
    ConfigPaths {
        file: PathBuf::from(dir).join("config.toml"),
    }
}

fn level_to_int(level: Level) -> i32 {
    match level {
        Level::Info => LEVEL_INFO,
        Level::Warning => LEVEL_WARNING,
        Level::Error => LEVEL_ERROR,
    }
}

/// Shows `notes` on the desktop when the `desktop-notifications` feature is
/// built in and notifications are enabled, and does nothing otherwise.
fn present_desktop(
    enabled: bool,
    app_id: Option<&str>,
    timeout_ms: u32,
    notes: &[crate::notification::Notification],
) {
    #[cfg(feature = "desktop-notifications")]
    if enabled {
        crate::desktop::present_all(notes, app_id, timeout_ms);
    }
    #[cfg(not(feature = "desktop-notifications"))]
    let _ = (enabled, app_id, timeout_ms, notes);
}

fn unit_to_int(unit: Unit) -> i32 {
    match unit {
        Unit::Gib => UNIT_GIB,
        Unit::Mib => UNIT_MIB,
        Unit::Kib => UNIT_KIB,
        Unit::Bytes => UNIT_BYTES,
    }
}

fn show_to_int(option: ShowOption) -> i32 {
    match option {
        ShowOption::Palette => SHOW_PALETTE,
        ShowOption::Name => SHOW_NAME,
    }
}

#[cfg(test)]
mod tests {
    use super::item_texture;
    use crate::resource::ResourceEnvironment;
    use std::path::PathBuf;

    /// Exercises the icon lookup against a real client jar when the machine has
    /// one, so the texture names and model fallbacks are checked against actual
    /// resource files instead of a stub. Skipped when no jar is cached.
    #[test]
    fn an_item_icon_comes_from_the_client_jar() {
        let Some(home) = std::env::var_os("HOME") else {
            return;
        };
        let cache_dir = PathBuf::from(home).join(".cache/dhlrc");
        let Some(version) = ["1.21.5", "1.21.4", "1.21"]
            .into_iter()
            .find(|version| crate::resource::cache::client_jar(&cache_dir, version).exists())
        else {
            return;
        };
        let set = ResourceEnvironment {
            cache_dir,
            version: Some(version.to_string()),
            packs: vec![],
        }
        .resource_set();
        assert!(item_texture(&set, "minecraft:stick").is_some());
        // A stair has no texture named after it; the block model names one.
        assert!(item_texture(&set, "minecraft:oak_stairs").is_some());
        // A bed is modelled as an item.
        assert!(item_texture(&set, "minecraft:red_bed").is_some());
        assert!(item_texture(&set, "minecraft:not_a_real_item").is_none());
    }
}
