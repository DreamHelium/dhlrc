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
