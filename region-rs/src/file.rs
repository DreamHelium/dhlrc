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

/* How a file turned out to be compressed. Kept in sync with the
 * `ObjectCompression` values in `src/region.h`, which both the plugin metadata
 * (`region_compression ()`) and the loader's report use. */
pub const OBJECT_COMPRESSION_ANY: c_int = 0;
pub const OBJECT_COMPRESSION_GZIP: c_int = 1;
pub const OBJECT_COMPRESSION_ZLIB: c_int = 2;
pub const OBJECT_COMPRESSION_NONE: c_int = 3;

fn file_try_uncompress_real(
    filename: *const c_char,
    helper_struct: &HelperStruct,
    compression: &mut c_int,
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
        /* Only what the read filled: `temp_data` is a fixed-size buffer, so
         * moving all of it in would pad the file with trailing zeros. */
        file_data.extend_from_slice(&temp_data[..len]);
    }

    let mut decoder: Decompress;
    let mut ret = vec![];

    if file_data.len() > 2 && file_data[0] == 0x1f && file_data[1] == 0x8b {
        /* file is gzip */
        *compression = OBJECT_COMPRESSION_GZIP;
        decoder = Decompress::new_gzip(15);
    } else if file_data.len() > 1 && file_data[0] == 0x78 {
        /* file is zlib */
        *compression = OBJECT_COMPRESSION_ZLIB;
        decoder = Decompress::new(true);
    } else {
        /* Plain bytes, not compressed at all. Whether that is what the format
         * wants is for the loader to say; this only reports what it found. */
        *compression = OBJECT_COMPRESSION_NONE;
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
                    i18n("Uncompress finished."),
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
    compression: *mut c_int,
) -> *mut Vec<u8> {
    let mut form = OBJECT_COMPRESSION_ANY;
    let result = unsafe { file_try_uncompress_real(filename, &*helper_struct, &mut form) };
    /* Reported even on failure, so the caller always sees what was found. */
    if !compression.is_null() {
        unsafe { *compression = form };
    }
    match result {
        Ok(r) => Box::into_raw(Box::new(r)),
        Err(err) => unsafe {
            *failed = 1;
            let err_msg: Vec<u8> = Vec::from(err.to_string());
            Box::into_raw(Box::new(err_msg))
        },
    }
}

#[cfg(test)]
mod tests
{
    use super::*;
    use flate2::Compression;
    use flate2::write::{GzEncoder, ZlibEncoder};
    use std::ffi::CString;
    use std::io::Write;

    fn helper() -> HelperStruct {
        HelperStruct::new(None, std::ptr::null_mut(), std::ptr::null(), u64::MAX, 0)
    }

    /// Writes `bytes` to a temporary file, reads it back through the real
    /// function and reports what compression it turned out to have.
    fn read_form (bytes: &[u8], name: &str) -> (i32, Vec<u8>) {
        let path = std::env::temp_dir ().join (format! (
            "dhlrc-compression-{}-{name}",
            std::process::id ()
        ));
        std::fs::write (&path, bytes).unwrap ();
        let real_path = CString::new (path.to_string_lossy ().as_bytes ()).unwrap ();
        let mut form = OBJECT_COMPRESSION_ANY;
        let data = file_try_uncompress_real (real_path.as_ptr (), &helper (), &mut form)
            .expect ("the file should be readable");
        std::fs::remove_file (&path).ok ();
        (form, data)
    }

    #[test]
    fn the_compression_of_a_file_is_told_apart () {
        let mut gzip = GzEncoder::new (Vec::new (), Compression::default ());
        gzip.write_all (b"hello").unwrap ();
        let gzip = gzip.finish ().unwrap ();

        let mut zlib = ZlibEncoder::new (Vec::new (), Compression::default ());
        zlib.write_all (b"hello").unwrap ();
        let zlib = zlib.finish ().unwrap ();

        assert_eq! (
            read_form (&gzip, "gzip"),
            (OBJECT_COMPRESSION_GZIP, b"hello".to_vec ())
        );
        assert_eq! (
            read_form (&zlib, "zlib"),
            (OBJECT_COMPRESSION_ZLIB, b"hello".to_vec ())
        );
        /* Neither header: the bytes are already the file. */
        assert_eq! (
            read_form (b"hello", "plain"),
            (OBJECT_COMPRESSION_NONE, b"hello".to_vec ())
        );
    }
}
