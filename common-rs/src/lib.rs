use std::ffi::{c_char, c_int, c_void};

pub mod cancel_flag;
pub mod i18n;
pub mod my_error;
pub mod region;
pub mod tree_value;
pub mod util;

pub type ProgressFn = Option<
    extern "C" fn(
        main_klass: *mut c_void,
        progress: c_int,
        format: *const c_char,
        text: *const c_char,
    ),
>;
