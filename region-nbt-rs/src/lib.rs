use crab_nbt::Nbt;
use flate2::read::GzDecoder;
use std::collections::HashMap;
use std::error::Error;
use std::ffi::{CStr, CString, c_char, c_int, c_void};
use std::fmt::{Display, Formatter};
use std::fs::File;
use std::io::{Cursor, Read, Seek};
use std::ptr::{null, null_mut};
use std::string::String;
use std::time::Instant;

type ProgressFn = extern "C" fn(
    main_klass: *mut c_void,
    progress: c_int,
    format: *const c_char,
    text: *const c_char,
);

pub enum TreeValue {
    Byte(i8),
    Short(i16),
    Int(i32),
    Long(i64),
    Float(f32),
    Double(f64),
    String(String),
    ByteArray(Vec<i8>),
    IntArray(Vec<i32>),
    LongArray(Vec<i64>),
    List(Vec<TreeValue>),
    Compound(HashMap<String, TreeValue>),
}

#[link(name = "region_rs")]
unsafe extern "C" {
    fn region_new() -> *mut c_void;
    fn region_free(region: *mut c_void);
    fn region_set_time(region: *mut c_void, create_time: i64, modify_time: i64) -> *const c_char;
    fn string_free(string: *mut c_char);
    fn region_get_create_timestamp(region: *mut c_void) -> i64;
    fn region_get_modify_timestamp(region: *mut c_void) -> i64;
    fn region_get_size(region: *mut c_void);
    fn region_get_name(region: *mut c_void) -> *const c_char;
    fn region_set_name(region: *mut c_void, string: *const c_char) -> *const c_char;
    fn region_get_description(region: *mut c_void) -> *const c_char;
    fn region_set_description(region: *mut c_void, string: *const c_char) -> *const c_char;
    fn region_get_author(region: *mut c_void) -> *const c_char;
    fn region_set_author(region: *mut c_void, string: *const c_char) -> *const c_char;
    fn region_get_data_version(region: *mut c_void) -> u32;
    fn region_set_data_version(region: *mut c_void, data_version: u32);
    fn region_set_size(region: *mut c_void, x: i32, y: i32, z: i32);
    fn region_get_x(region: *mut c_void) -> i32;
    fn region_get_y(region: *mut c_void) -> i32;
    fn region_get_z(region: *mut c_void) -> i32;
    fn block_new_to_region(
        region: *mut c_void,
        index: usize,
        id: u32,
        tree: *mut HashMap<String, TreeValue>,
    );
    fn region_get_block_entity_tree_by_index(
        region: *mut c_void,
        index: usize,
    ) -> *mut HashMap<String, TreeValue>;
    fn region_get_block_id_by_index(region: *mut c_void, index: usize) -> u32;
    fn region_append_palette(
        region: *mut c_void,
        id_name: *const c_char,
        property: *mut Vec<(String, String)>,
    ) -> *const c_char;
    fn region_get_palette_id_name(region: *mut c_void, id: usize) -> *const c_char;
    fn region_get_palette_property(region: *mut c_void, index: usize)
    -> *mut Vec<(String, String)>;
    fn region_get_palette_property_len(region: *mut c_void, id: usize) -> usize;
    fn region_get_palette_property_name(
        region: *mut c_void,
        id: usize,
        index: usize,
    ) -> *const c_char;
    fn region_get_palette_property_data(
        region: *mut c_void,
        id: usize,
        index: usize,
    ) -> *const c_char;
    fn region_set_palette_property_name_and_data(
        region: *mut c_void,
        id: usize,
        index: usize,
        name: *const c_char,
        data: *const c_char,
    ) -> *const c_char;
}

fn string_to_ptr_fail_to_null(string: &str) -> *mut c_char {
    let str = CString::new(string);
    match str {
        Ok(real_str) => real_str.into_raw(),
        Err(_err) => null_mut(),
    }
}

#[derive(Debug)]
struct MyError {
    msg: String,
}

impl Display for MyError {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.msg)
    }
}

impl Error for MyError {}

fn cstr_to_str(string: *const c_char) -> Result<String, Box<dyn Error>> {
    if string.is_null() {
        return Err(Box::from(MyError {
            /* NOTE: can be translated */
            msg: "Null pointer detected".to_string(),
        }));
    }
    let str = unsafe { CStr::from_ptr(string) };
    let ref_str = str.to_str()?;
    Ok(ref_str.to_string())
}

fn i18n(string: &str) -> &str {
    string
}

#[unsafe(no_mangle)]
pub extern "C" fn region_type() -> *const c_char {
    string_to_ptr_fail_to_null("nbt")
}

#[unsafe(no_mangle)]
pub extern "C" fn region_is_multi() -> i32 {
    0
}

/* If it's litematic, please add:
 * region_num
 * region_name_index
 * region_crate_from_file_index
 */

fn show_progress(
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
    unsafe {
        if progress_fn as usize != 0 {
            progress_fn(main_klass, progress, msg, real_text);
        }
        string_free(msg);
        string_free(real_text);
    }
}

fn decompress_data(
    file: File,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
) -> Result<Vec<u8>, Box<dyn Error>> {
    let file_size: usize = file.metadata()?.len() as usize;
    let mut decoder = GzDecoder::new(file.try_clone()?);
    let mut data = Vec::new();
    let mut total_len: usize = 0;
    let mut start = Instant::now();
    loop {
        if start.elapsed().as_secs() >= 5 {
            let mut prog_str = String::new();
            prog_str.push_str(&total_len.to_string());
            prog_str.push_str(i18n(" bytes decompressed, original file has size "));
            prog_str.push_str(&file_size.to_string());
            let pos = file.try_clone()?.stream_position()? as usize;
            show_progress(
                progress_fn,
                main_klass,
                (pos * 100 / file_size) as c_int,
                i18n("Uncompressing data, %s"),
                &prog_str,
            );
            start = Instant::now();
        }

        let mut temp_data = vec![0; 100];
        let len = decoder.read(&mut temp_data)?;
        total_len += len;

        data.append(&mut temp_data);
        if len == 0 {
            show_progress(
                progress_fn,
                main_klass,
                100,
                i18n("Uncompress finish!"),
                &String::new(),
            );
            return Ok(data);
        }
    }
}

fn nbt_create_real(
    filename: *const c_char,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
) -> Result<Nbt, Box<dyn Error>> {
    let real_filename = cstr_to_str(filename)?;
    let file = File::open(real_filename)?;
    let ret = decompress_data(file, progress_fn, main_klass)?;

    let mut bytes = Cursor::new(ret);
    show_progress(
        progress_fn,
        main_klass,
        0,
        i18n("Reading NBT."),
        &String::new(),
    );
    let nbt = Nbt::read(&mut bytes)?;
    show_progress(
        progress_fn,
        main_klass,
        100,
        i18n("Reading NBT finish."),
        &String::new(),
    );
    Ok(nbt)
}

struct Block {
    id: u32,
    block_entity: Option<HashMap<String, TreeValue>>,
}

struct Palette {
    id_name: String,
    property: Vec<(String, String)>,
}

struct NbtStruct {
    data_version: u32,
    size: (i32, i32, i32),
}

#[unsafe(no_mangle)]
pub extern "C" fn region_create_from_file(
    filename: *const c_char,
    progress_fn: ProgressFn, // region: *mut *mut c_void,
    main_klass: *mut c_void,
) -> *const c_char {
    // let region = unsafe { region_new() };
    match nbt_create_real(filename, progress_fn, main_klass) {
        Ok(value) => {
            let compound = value.root_tag.child_tags;
            println!("{} {}", compound[0].0, value.name);
        }
        Err(err) => return string_to_ptr_fail_to_null(&err.to_string()),
    }
    null()
}
