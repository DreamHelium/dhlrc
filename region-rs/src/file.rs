use crate::cstr_to_str;
use common_rs::helper_struct::HelperStruct;
use common_rs::i18n::i18n;
use common_rs::util::show_progress;
use flate2::{Decompress, FlushDecompress, Status};
use std::error::Error;
use std::ffi::{c_char, c_int};
use std::fs::File;
use std::io::{Read, Seek, SeekFrom};
use std::time::Instant;
use sysinfo::System;

fn file_try_uncompress_real(
    filename: *const c_char,
    helper_struct: &HelperStruct,
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

        helper_struct.progress(
            &mut sys,
            &mut start,
            (file_pos * 100 / file_size) as c_int,
            i18n("Loading file."),
            i18n("The loading operation is cancelled."),
        )?;

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
        helper_struct.progress(
            &mut sys,
            &mut start,
            (pos * 100 / data_size) as c_int,
            i18n("Uncompressing data."),
            i18n("The uncompressing operation is cancelled."),
        )?;

        let decompress_result =
            decoder.decompress_vec(&file_data[pos..], &mut ret, FlushDecompress::None)?;
        match decompress_result {
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
        pos = decoder.total_in() as usize;
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn file_try_uncompress(
    filename: *const c_char,
    helper_struct: *mut HelperStruct,
    failed: *mut c_int,
) -> *mut Vec<u8> {
    unsafe {
        match file_try_uncompress_real(filename, &*helper_struct) {
            Ok(r) => Box::into_raw(Box::new(r)),
            Err(err) => {
                *failed = 1;
                let err_msg: Vec<u8> = Vec::from(err.to_string());
                Box::into_raw(Box::new(err_msg))
            }
        }
    }
}
