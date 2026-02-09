use crate::ProgressFn;
use crate::i18n::i18n;
use crate::my_error::MyError;
use std::error::Error;
use std::ffi::{CStr, CString, c_char, c_int, c_void};
use std::ptr::null_mut;
use std::time::Instant;
use sysinfo::System;

static mut FREE_MEMORY: usize = 500 * 1024 * 1024;
static mut ELAPSED_MILLISECS: u64 = 500;

#[unsafe(no_mangle)]
pub extern "C" fn string_free(string: *mut c_char) {
    if string.is_null() {
        return;
    }
    drop(unsafe { CString::from_raw(string) });
}
pub fn show_progress(
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    progress: c_int,
    message: &str,
    text: &str,
) {
    let msg = string_to_ptr_fail_to_null(message);
    let mut real_text: *mut c_char = null_mut();
    if !text.is_empty() {
        real_text = string_to_ptr_fail_to_null(text);
    }
    if !progress_fn.is_none() {
        if progress_fn.unwrap() as usize != 0 {
            progress_fn.unwrap()(main_klass, progress, msg, real_text);
        }
    }

    string_free(msg);
    string_free(real_text);
}

pub fn finish_oom(system: &mut System) -> Result<(), MyError> {
    system.refresh_all();
    if unsafe { system.available_memory() < FREE_MEMORY as u64 } {
        println!("{}", system.free_memory());
        return Err(MyError {
            msg: i18n("Out of memory!").to_string(),
        });
    }
    Ok(())
}

#[unsafe(no_mangle)]
pub extern "C" fn get_system_info_object() -> *mut System {
    Box::into_raw(Box::new(System::new_all()))
}

#[unsafe(no_mangle)]
pub extern "C" fn get_free_memory(system: *mut System) -> u64 {
    let sys = unsafe { &mut *system };
    sys.refresh_memory();
    sys.available_memory()
}

#[unsafe(no_mangle)]
pub extern "C" fn system_info_object_free(system: *mut System) {
    drop(unsafe { Box::from_raw(system) })
}

#[unsafe(no_mangle)]
pub extern "C" fn reset_available_memory(memory: usize) {
    unsafe { FREE_MEMORY = memory };
}

#[unsafe(no_mangle)]
pub extern "C" fn reset_elapsed_millisecs(millisecond: u64) {
    unsafe { ELAPSED_MILLISECS = millisecond };
}

#[unsafe(no_mangle)]
pub extern "C" fn get_limit_available_memory() -> usize {
    unsafe { FREE_MEMORY }
}

pub fn string_to_ptr_fail_to_null(string: &str) -> *mut c_char {
    let str = CString::new(string);
    match str {
        Ok(real_str) => real_str.into_raw(),
        Err(_err) => null_mut(),
    }
}

pub fn cstr_to_str(string: *const c_char) -> Result<String, Box<dyn Error>> {
    if string.is_null() {
        return Err(Box::from(MyError {
            /* NOTE: can be translated */
            msg: i18n("Null pointer detected").to_string(),
        }));
    }
    let str = unsafe { CStr::from_ptr(string) };
    let ref_str = str.to_str()?;
    Ok(ref_str.to_string())
}

#[unsafe(no_mangle)]
pub extern "C" fn real_show_progress(
    instant: &mut Instant,
    system: &mut System,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    percentage: c_int,
    msg: &str,
    text: &str,
) -> Result<(), MyError> {
    if instant.elapsed().as_millis() >= unsafe { ELAPSED_MILLISECS as u128 } {
        finish_oom(system)?;
        show_progress(progress_fn, main_klass, percentage, msg, text);
        *instant = Instant::now();
    }
    Ok(())
}
