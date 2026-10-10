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
use crate::notification::Level;
use std::ffi::{CStr, CString, c_char, c_void};
use std::path::PathBuf;
use std::ptr;
use std::sync::{Arc, Mutex};

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

/// Called from the watch thread when the configuration file changed.
pub type DhlrcChangeFunc = extern "C" fn(user_data: *mut c_void);

/// A snapshot of the configuration. The layout must match `DhlrcConfigView` in
/// the header, field for field.
#[repr(C)]
pub struct DhlrcConfigView {
    pub memory_limit: i64,
    pub limit_unit: i32,
    pub elapsed_milliseconds: i64,
    pub select_all_regions_in_loading: bool,
    pub loading_file_by_extension: bool,
    pub fail_then_retry: bool,
    pub strict_nbt_encoding: bool,
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
            loading_file_by_extension: config.general.loading_file_by_extension,
            fail_then_retry: config.general.fail_then_retry,
            strict_nbt_encoding: config.general.strict_nbt_encoding,
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
            cache_directory: match &config.general.cache_directory {
                Some(path) => owned(&mut strings, &path.to_string_lossy()),
                None => ptr::null(),
            },
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
}

/// The handle a non-Rust frontend holds.
pub struct DhlrcCore {
    /// Cached `paths.file`, returned by `dhlrc_core_config_path`.
    path: CString,
    paths: ConfigPaths,
    shared: Arc<Shared>,
    watch: Mutex<Option<ConfigWatch>>,
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

    if let Some(notify) = notify {
        for note in init.notifications() {
            // The strings only have to live for the call.
            let event = CString::new(note.event_id).unwrap_or_default();
            let title = CString::new(note.title).unwrap_or_default();
            let text = CString::new(note.text).unwrap_or_default();
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
        }),
        paths,
        watch: Mutex::new(None),
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
    // Carried as a `usize` so the closure is `Send`: the caller keeps `user_data`
    // valid until the watch is stopped.
    let user_address = user_data as usize;
    let watcher = ConfigWatcher::from_config(core.paths.clone(), current);

    match watcher.spawn(move |config| {
        if let Ok(mut guard) = shared.config.lock() {
            *guard = config;
        }
        on_change(user_address as *mut c_void);
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
    config.general.loading_file_by_extension = view.loading_file_by_extension;
    config.general.fail_then_retry = view.fail_then_retry;
    config.general.strict_nbt_encoding = view.strict_nbt_encoding;
    config.general.fail_download_use_cache = view.fail_download_use_cache;
    config.general.cache_directory = from_c(view.cache_directory)
        .filter(|value| !value.is_empty())
        .map(PathBuf::from);
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
            .unwrap_or(false),
        Err(_) => false,
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
