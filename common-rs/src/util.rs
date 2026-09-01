use crate::ProgressFn;
use crate::helper_struct::HelperStruct;
use crate::i18n::i18n;
use crate::my_error::MyError;
use flate2::{Compress, Compression, FlushCompress, Status};
use std::error::Error;
use std::ffi::{CStr, CString, c_char, c_int, c_void};
use std::ptr::null_mut;
use std::time::Instant;
use sysinfo::System;

pub fn string_free(string: *mut c_char) {
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

pub fn finish_oom(system: &mut System, free_memory: u64) -> Result<(), MyError> {
    system.refresh_all();
    if system.available_memory() < free_memory {
        return Err(MyError {
            msg: i18n("Out of memory!").to_string(),
        });
    }
    Ok(())
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

pub fn real_show_progress(
    instant: &mut Instant,
    system: &mut System,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    percentage: c_int,
    msg: &str,
    text: &str,
    elapsed_millisecs: u128,
    free_memory: u64,
) -> Result<(), MyError> {
    if instant.elapsed().as_millis() >= elapsed_millisecs {
        finish_oom(system, free_memory)?;
        show_progress(progress_fn, main_klass, percentage, msg, text);
        *instant = Instant::now();
    }
    Ok(())
}

#[macro_export]
macro_rules! show_progress_macro {
    ($time : expr, $sys : expr, $progress_fn : expr, $main_klass : expr, $percentage : expr,
     $elapsed_ms : expr, $free_memory : expr, $str : expr, $cancel_flag : expr, $err_msg : expr ) => {
        if cancel_flag_is_cancelled($cancel_flag) == 1 {
            return Err(Box::new(MyError {
                msg: $err_msg.to_string(),
            }));
        }
        if $time.elapsed().as_millis() >= $elapsed_ms {
            finish_oom($sys, $free_memory)?;
            show_progress($progress_fn, $main_klass, $percentage, $str, "");
            *$time = Instant::now();
        }
    };
}

pub fn vec_try_compress_real(
    vec: *mut Vec<u8>,
    helper_struct: &HelperStruct,
    zlib: bool,
) -> Result<Vec<u8>, Box<dyn Error>> {
    let real_vec = unsafe { Box::from_raw(vec) };
    let mut compressor;
    if zlib {
        compressor = Compress::new(Compression::default(), true);
    } else {
        compressor = Compress::new_gzip(Compression::default(), 15);
    }
    let mut start = Instant::now();
    let mut pos: usize = 0;
    let vec_size = real_vec.len();
    let mut sys = System::new_all();
    let mut ret = vec![];
    loop {
        helper_struct.progress(
            &mut sys,
            &mut start,
            (pos * 100 / vec_size) as c_int,
            i18n("Uncompressing data."),
            i18n("The compressing operation is cancelled."),
        )?;

        let result;
        if pos != vec_size {
            result = compressor.compress_vec(&real_vec[pos..], &mut ret, FlushCompress::None)?;
        } else {
            result = compressor.compress_vec(&real_vec[pos..], &mut ret, FlushCompress::Finish)?;
        }
        match result {
            Status::Ok => {}
            Status::BufError => {
                ret.reserve(100);
            }
            Status::StreamEnd => {
                show_progress(
                    helper_struct.progress_fn,
                    helper_struct.main_klass,
                    100,
                    i18n("Uncompress finish!"),
                    &String::new(),
                );
                return Ok(ret);
            }
        }
        pos = compressor.total_in() as usize;
    }
}
