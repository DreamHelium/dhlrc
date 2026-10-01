use common_rs::config::{ConfigItem, ConfigKind, ConfigObject, config_free, config_new, ffi};
use common_rs::i18n::i18n;
use std::ffi::{c_char, c_int};

/* The exported symbols below are thin forwards into `common_rs::config::ffi`;
 * the option table is the single place where the available options are
 * described. */

pub struct InputConfig {
    pub ignore_base_data: bool,
}

impl Default for InputConfig {
    fn default() -> Self {
        Self {
            ignore_base_data: false,
        }
    }
}

impl ConfigObject for InputConfig {
    const ITEMS: &'static [ConfigItem] = &[ConfigItem {
        key: "ignore_base_data",
        kind: ConfigKind::Bool,
        label: i18n("Ignore Base Data"),
        description: i18n("Do not read the metadata stored in the file"),
        minimum: 0,
        maximum: 0,
        default: 0,
    }];

    fn set_bool(&mut self, index: usize, value: bool) -> bool {
        match index {
            0 => {
                self.ignore_base_data = value;
                true
            }
            _ => false,
        }
    }

    fn get_bool(&self, index: usize) -> Option<bool> {
        match index {
            0 => Some(self.ignore_base_data),
            _ => None,
        }
    }
}

/* What the writer lets the user choose.
 *
 * There is deliberately no `ignore_air` here. Litematic stores blocks as a
 * densely packed array whose length has to match the region volume, so leaving
 * air out would make the file inconsistent with the `Size` in its own metadata;
 * the option exists for the NBT writer because that format stores block
 * positions explicitly instead. */
pub struct OutputConfig {
    /// The litematic format revision to write into `Version`.
    pub version: i32,
}

impl Default for OutputConfig {
    fn default() -> Self {
        Self {
            version: crate::output::DEFAULT_LITEMATIC_VERSION,
        }
    }
}

impl ConfigObject for OutputConfig {
    /* The spec carries the bounds as `<min>,<max>` after the kind, so the host
     * can build a spin box that cannot accept a value the writer would reject. */
    const ITEMS: &'static [ConfigItem] = &[ConfigItem {
        key: "version",
        kind: ConfigKind::Int,
        label: i18n("Litematic Version"),
        description: i18n(
            "The format revision written into the file. Increase it only if the program you open the file with requires a newer one.",
        ),
        minimum: crate::output::MIN_LITEMATIC_VERSION,
        maximum: crate::output::MAX_LITEMATIC_VERSION,
        /* The host shows this until the user saves something else, so it has to
         * be the same value `OutputConfig::default ()` starts at. */
        default: crate::output::DEFAULT_LITEMATIC_VERSION,
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
                self.version = value;
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
pub unsafe extern "C" fn output_config_item_set_int(
    output_config: *mut OutputConfig,
    index: usize,
    value: i32,
) {
    unsafe { ffi::item_set_int(output_config, index, value) };
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
