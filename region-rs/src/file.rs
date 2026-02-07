use crate::{ProgressFn, cstr_to_str};
use common_rs::cancel_flag::cancel_flag_is_cancelled;
use common_rs::i18n::i18n;
use common_rs::my_error::MyError;
use common_rs::util::{real_show_progress, show_progress};
use flate2::{Compress, Compression, Decompress, FlushCompress, FlushDecompress, Status};
use std::error::Error;
use std::ffi::{c_char, c_int, c_void};
use std::fs::File;
use std::io::{Read, Seek, SeekFrom};
use std::sync::atomic::AtomicBool;
use std::time::Instant;
use sysinfo::System;

fn file_try_uncompress_real(
    filename: *const c_char,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    cancel_flag: *const AtomicBool,
) -> Result<Vec<u8>, Box<dyn Error>> {
    let str = cstr_to_str(filename)?;
    let mut file = File::open(str)?;
    let file_size = file.metadata()?.len();
    let mut start = Instant::now();
    let mut file_data = vec![];
    let mut sys = System::new_all();
    loop {
        let mut temp_data = vec![0; 100];
        let file_pos = file.try_clone()?.seek(SeekFrom::Current(0))?;

        real_show_progress(
            &mut start,
            &mut sys,
            progress_fn,
            main_klass,
            (file_pos * 100 / file_size) as c_int,
            i18n("Loading file."),
            "",
        )?;

        if cancel_flag_is_cancelled(cancel_flag) == 1 {
            return Err(Box::new(MyError {
                msg: i18n("The loading operation is cancelled.").to_string(),
            }));
        }

        let len = file.read(&mut temp_data)?;
        if len == 0 {
            break;
        }
        file_data.append(&mut temp_data);
    }

    let mut decoder: Decompress;
    let mut ret = vec![];

    if file_data.len() > 2 && file_data[0] == 0x1f && file_data[1] == 0x8b {
        /* file is gzip */
        decoder = Decompress::new_gzip(15);
    } else if file_data.len() > 1 && file_data[0] == 0x78 {
        /* file is zlib */
        decoder = Decompress::new(true);
    } else {
        return Ok(file_data);
    }

    let mut pos: usize = 0;

    let data_size = file_data.len();
    loop {
        real_show_progress(
            &mut start,
            &mut sys,
            progress_fn,
            main_klass,
            (pos * 100 / data_size) as c_int,
            i18n("Uncompressing data."),
            "",
        )?;

        if cancel_flag_is_cancelled(cancel_flag) == 1 {
            return Err(Box::new(MyError {
                msg: i18n("The uncompressing operation is cancelled.").to_string(),
            }));
        }

        let decompress_result =
            decoder.decompress_vec(&file_data[pos..], &mut ret, FlushDecompress::None)?;
        match decompress_result {
            Status::Ok => {}
            Status::BufError => {
                ret.reserve(100);
            }
            Status::StreamEnd => {
                show_progress(
                    progress_fn,
                    main_klass,
                    100,
                    i18n("Uncompress finish!"),
                    &String::new(),
                );
                return Ok(ret);
            }
        }
        pos = decoder.total_in() as usize;
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn file_try_uncompress(
    filename: *const c_char,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    failed: *mut c_int,
    cancel_flag: *const AtomicBool,
) -> *mut Vec<u8> {
    match file_try_uncompress_real(filename, progress_fn, main_klass, cancel_flag) {
        Ok(r) => Box::into_raw(Box::new(r)),
        Err(err) => {
            unsafe {
                *failed = 1;
            }
            let err_msg: Vec<u8> = Vec::from(err.to_string());
            Box::into_raw(Box::new(err_msg))
        }
    }
}

fn vec_try_compress_real(
    vec: *mut Vec<u8>,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    zlib: bool,
    cancel_flag: *const AtomicBool,
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
        real_show_progress(
            &mut start,
            &mut sys,
            progress_fn,
            main_klass,
            (pos * 100 / vec_size) as c_int,
            i18n("Uncompressing data."),
            "",
        )?;

        if cancel_flag_is_cancelled(cancel_flag) == 1 {
            return Err(Box::new(MyError {
                msg: i18n("The compressing operation is cancelled.").to_string(),
            }));
        }
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
                    progress_fn,
                    main_klass,
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

#[unsafe(no_mangle)]
pub extern "C" fn vec_try_compress(
    vec: *mut Vec<u8>,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    failed: *mut c_int,
    zlib: bool,
    cancel_flag: *const AtomicBool,
) -> *mut Vec<u8> {
    match vec_try_compress_real(vec, progress_fn, main_klass, zlib, cancel_flag) {
        Ok(r) => Box::into_raw(Box::new(r)),
        Err(err) => {
            unsafe {
                *failed = 1;
            }
            let err_msg: Vec<u8> = Vec::from(err.to_string());
            Box::into_raw(Box::new(err_msg))
        }
    }
}
