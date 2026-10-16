use crate::schem::MAX_VERSION;
use common_rs::config::{ConfigItem, ConfigKind, ConfigObject, config_free, config_new, ffi};
use common_rs::i18n::i18n;
use std::ffi::{c_char, c_int};

/* The exported symbols below are thin forwards into `common_rs::config::ffi`;
 * the option tables are the single place where the available options are
 * described. */

/// Reading options.
pub struct InputConfig {
    pub ignore_base_data: bool,
    /// Whether an entity that cannot be used fails the load. Off, so such an
    /// entry is skipped and the rest of the file is still read.
    pub refuse_broken_entities: bool,
}

impl Default for InputConfig {
    fn default() -> Self {
        Self {
            ignore_base_data: false,
            /* Off by default: a file WorldEdit wrote has no entities at all,
             * and one broken entry should not cost the user the whole
             * schematic. Off is also what an unchecked box in the host's
             * options dialog means, so the default holds on every path. */
            refuse_broken_entities: false,
        }
    }
}

impl ConfigObject for InputConfig {
    const ITEMS: &'static [ConfigItem] = &[
        ConfigItem {
            key: "ignore_base_data",
            kind: ConfigKind::Bool,
            label: i18n("Ignore Base Data"),
            description: i18n(
                "Keep the region's own name, author, description and time instead of taking them from the file",
            ),
            minimum: 0,
            maximum: 0,
            default: 0,
        },
        ConfigItem {
            key: "refuse_broken_entities",
            kind: ConfigKind::Bool,
            label: i18n("Refuse Incomplete Entities"),
            description: i18n(
                "An entity or block entity without a position or an id fails the load instead of being skipped. Off by default: WorldEdit does not store entities, so the files it writes have none, and one broken entry should not cost the whole schematic.",
            ),
            minimum: 0,
            maximum: 0,
            default: 0,
        },
    ];

    fn set_bool(&mut self, index: usize, value: bool) -> bool {
        match index {
            0 => {
                self.ignore_base_data = value;
                true
            }
            1 => {
                self.refuse_broken_entities = value;
                true
            }
            _ => false,
        }
    }

    fn get_bool(&self, index: usize) -> Option<bool> {
        match index {
            0 => Some(self.ignore_base_data),
            1 => Some(self.refuse_broken_entities),
            _ => None,
        }
    }
}

/// Writing options.
pub struct OutputConfig {
    /// The Sponge specification version to write.
    pub version: i32,
}

impl Default for OutputConfig {
    fn default() -> Self {
        Self { version: 0 }
    }
}

impl ConfigObject for OutputConfig {
    const ITEMS: &'static [ConfigItem] = &[ConfigItem {
        key: "version",
        kind: ConfigKind::Int,
        label: i18n("Sponge Format Version"),
        description: i18n(
            "Which version to write: 0 keeps the version the file was read from (2 when that is unknown), 1 is the legacy layout, 2 is what WorldEdit 7 writes and 3 is the current one. Writing 2 from a version 3 file packs its per-block biomes into one per column.",
        ),
        minimum: 0,
        maximum: MAX_VERSION,
        default: 0,
    }];

    fn set_bool(&mut self, _index: usize, _value: bool) -> bool {
        false
    }

    fn get_bool(&self, _index: usize) -> Option<bool> {
        None
    }

    fn set_int(&mut self, index: usize, value: i32) -> bool {
        match index {
            0 => {
                /* 0 means "whatever the file was"; 1 to 3 pick a version. */
                self.version = value.clamp(0, MAX_VERSION);
                true
            }
            _ => false,
        }
    }

    fn get_int(&self, index: usize) -> Option<i32> {
        match index {
            0 => Some(self.version),
            _ => None,
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn input_config_new() -> *mut InputConfig {
    config_new::<InputConfig>()
}

/// # Safety
/// `input_config` must come from `input_config_new`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn input_config_free(input_config: *mut InputConfig) {
    unsafe { config_free(input_config) };
}

#[unsafe(no_mangle)]
pub extern "C" fn input_config_num() -> usize {
    ffi::num::<InputConfig>()
}

#[unsafe(no_mangle)]
pub extern "C" fn input_config_item(index: usize) -> *const c_char {
    ffi::item::<InputConfig>(index)
}

#[unsafe(no_mangle)]
pub extern "C" fn input_config_item_get_name(index: usize) -> *const c_char {
    ffi::item_get_name::<InputConfig>(index)
}

#[unsafe(no_mangle)]
pub extern "C" fn input_config_item_get_description(index: usize) -> *const c_char {
    ffi::item_get_description::<InputConfig>(index)
}

/// # Safety
/// `input_config` must come from `input_config_new`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn input_config_item_set_bool(
    input_config: *mut InputConfig,
    index: usize,
    value: c_int,
) {
    unsafe { ffi::item_set_bool(input_config, index, value) };
}

/// # Safety
/// `input_config` must come from `input_config_new`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn input_config_item_get_bool(
    input_config: *const InputConfig,
    index: usize,
) -> c_int {
    unsafe { ffi::item_get_bool(input_config, index) }
}

/// # Safety
/// `input_config` must come from `input_config_new`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn input_config_item_set_int(
    input_config: *mut InputConfig,
    index: usize,
    value: c_int,
) {
    unsafe { ffi::item_set_int(input_config, index, value) };
}

/// # Safety
/// `input_config` must come from `input_config_new`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn input_config_item_get_int(
    input_config: *const InputConfig,
    index: usize,
) -> c_int {
    unsafe { ffi::item_get_int(input_config, index) }
}

#[unsafe(no_mangle)]
pub extern "C" fn output_config_new() -> *mut OutputConfig {
    config_new::<OutputConfig>()
}

/// # Safety
/// `output_config` must come from `output_config_new`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn output_config_free(output_config: *mut OutputConfig) {
    unsafe { config_free(output_config) };
}

#[unsafe(no_mangle)]
pub extern "C" fn output_config_num() -> usize {
    ffi::num::<OutputConfig>()
}

#[unsafe(no_mangle)]
pub extern "C" fn output_config_item(index: usize) -> *const c_char {
    ffi::item::<OutputConfig>(index)
}

#[unsafe(no_mangle)]
pub extern "C" fn output_config_item_get_name(index: usize) -> *const c_char {
    ffi::item_get_name::<OutputConfig>(index)
}

#[unsafe(no_mangle)]
pub extern "C" fn output_config_item_get_description(index: usize) -> *const c_char {
    ffi::item_get_description::<OutputConfig>(index)
}

/// # Safety
/// `output_config` must come from `output_config_new`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn output_config_item_set_bool(
    output_config: *mut OutputConfig,
    index: usize,
    value: c_int,
) {
    unsafe { ffi::item_set_bool(output_config, index, value) };
}

/// # Safety
/// `output_config` must come from `output_config_new`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn output_config_item_get_bool(
    output_config: *const OutputConfig,
    index: usize,
) -> c_int {
    unsafe { ffi::item_get_bool(output_config, index) }
}

/// # Safety
/// `output_config` must come from `output_config_new`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn output_config_item_set_int(
    output_config: *mut OutputConfig,
    index: usize,
    value: c_int,
) {
    unsafe { ffi::item_set_int(output_config, index, value) };
}

/// # Safety
/// `output_config` must come from `output_config_new`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn output_config_item_get_int(
    output_config: *const OutputConfig,
    index: usize,
) -> c_int {
    unsafe { ffi::item_get_int(output_config, index) }
}
