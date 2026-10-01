use common_rs::config::{ConfigItem, ConfigKind, ConfigObject, config_free, config_new, ffi};
use common_rs::i18n::i18n;
use std::ffi::{c_char, c_int};

/* The exported symbols below are thin forwards into `common_rs::config::ffi`;
 * the option table is the single place where the available options are
 * described. */

pub struct OutputConfig {
    pub ignore_air: bool,
}

impl Default for OutputConfig {
    fn default() -> Self {
        Self { ignore_air: false }
    }
}

impl ConfigObject for OutputConfig {
    const ITEMS: &'static [ConfigItem] = &[ConfigItem {
        key: "ignore_air",
        kind: ConfigKind::Bool,
        label: i18n("Ignore Air"),
        description: i18n("There will be no air block in the output structure"),
    }];

    fn set_bool(&mut self, index: usize, value: bool) -> bool {
        match index {
            0 => {
                self.ignore_air = value;
                true
            }
            _ => false,
        }
    }

    fn get_bool(&self, index: usize) -> Option<bool> {
        match index {
            0 => Some(self.ignore_air),
            _ => None,
        }
    }
}

/* Reading a `.nbt` structure offers no options yet; the empty table keeps the
 * host from offering a page with nothing on it. */
pub struct InputConfig;

impl Default for InputConfig {
    fn default() -> Self {
        Self
    }
}

impl ConfigObject for InputConfig {
    const ITEMS: &'static [ConfigItem] = &[];

    fn set_bool(&mut self, _index: usize, _value: bool) -> bool {
        false
    }

    fn get_bool(&self, _index: usize) -> Option<bool> {
        None
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

/* The remaining `input_config_*` entry points are intentionally absent: the
 * option table is empty, so the host never asks for an entry. */

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
