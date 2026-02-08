use common_rs::i18n::i18n;
use common_rs::util::string_to_ptr_fail_to_null;
use std::ffi::{c_char, c_int};
use std::ptr::null;

#[derive(Default)]
pub struct OutputConfig {
    pub ignore_air: bool,
}

#[unsafe(no_mangle)]
pub extern "C" fn output_config_new() -> *mut OutputConfig {
    Box::into_raw(Box::new(OutputConfig::default()))
}

#[unsafe(no_mangle)]
pub extern "C" fn output_config_free(output_config: *mut OutputConfig) {
    drop(unsafe { Box::from_raw(output_config) });
}

#[unsafe(no_mangle)]
pub extern "C" fn output_config_num() -> usize {
    1
}

#[unsafe(no_mangle)]
pub extern "C" fn output_config_item(index: usize) -> *const c_char {
    if index == 0 {
        return string_to_ptr_fail_to_null("ignore_air:bool");
    }
    null()
}

#[unsafe(no_mangle)]
pub extern "C" fn output_config_item_set_bool(
    output_config: *mut OutputConfig,
    index: usize,
    value: c_int,
) {
    if index == 0 && !output_config.is_null() {
        unsafe { (*output_config).ignore_air = value != 0 }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn output_config_item_get_name(index: usize) -> *const c_char {
    if index == 0 {
        return string_to_ptr_fail_to_null(i18n("Ignore Air"));
    }
    null()
}

#[unsafe(no_mangle)]
pub extern "C" fn output_config_item_get_description(index: usize) -> *const c_char {
    if index == 0 {
        return string_to_ptr_fail_to_null(i18n(
            "There will be no air block in the output structure",
        ));
    }
    null()
}
