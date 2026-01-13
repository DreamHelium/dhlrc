mod i18n;

use crate::i18n::i18n;
use flate2::write::{GzDecoder, ZlibDecoder};
use flate2::{Decompress, FlushDecompress, Status};
use std::cell::RefCell;
use std::collections::HashMap;
use std::error::Error;
use std::ffi::{CStr, CString, c_char, c_int, c_void};
use std::fmt::{Display, Formatter};
use std::fs::File;
use std::io::{Cursor, Read, Seek, SeekFrom, Write};
use std::ops::IndexMut;
use std::ptr::{null, null_mut};
use std::rc::Rc;
use std::string::String;
use std::time::Instant;
use time::UtcDateTime;

#[derive(Clone)]
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

struct BaseData {
    /** Default: time of generated */
    create_time: UtcDateTime,
    /** Default: time of generated */
    modify_time: UtcDateTime,
    /** Default: "" */
    description: String,
    /** Default: username */
    author: String,
    /** Default: Converted */
    name: String,
}

impl BaseData {
    fn set_time(&mut self, create_time: i64, modify_time: i64) -> Result<bool, Box<dyn Error>> {
        self.create_time = UtcDateTime::from_unix_timestamp(create_time)?;
        self.modify_time = UtcDateTime::from_unix_timestamp(modify_time)?;
        Ok(true)
    }
    fn get_create_timestamp(&self) -> i64 {
        self.create_time.unix_timestamp()
    }

    fn get_modify_timestamp(&self) -> i64 {
        self.modify_time.unix_timestamp()
    }

    fn get_description(&self) -> &str {
        self.description.as_str()
    }

    fn set_description(&mut self, string: &str) {
        self.description = string.to_string()
    }

    fn get_author(&self) -> &str {
        self.author.as_str()
    }

    fn set_author(&mut self, string: &str) {
        self.author = string.to_string()
    }

    fn get_name(&self) -> &str {
        self.name.as_str()
    }

    fn set_name(&mut self, string: &str) {
        self.name = string.to_string()
    }
}

impl Default for BaseData {
    fn default() -> Self {
        let temp_username = whoami::username();
        let real_username = temp_username.unwrap_or_else(|_err| "".to_string());
        BaseData {
            create_time: UtcDateTime::now(),
            modify_time: UtcDateTime::now(),
            description: "".to_string(),
            author: real_username,
            /* NOTE: can be translated */
            name: i18n("Converted").to_string(),
        }
    }
}

struct Palette {
    id_name: String,
    property: Vec<(String, String)>,
}

struct Block {
    id: u32,
    block_entity: Option<HashMap<String, TreeValue>>,
}

#[derive(Default)]
pub struct Region {
    /* Base information */
    data_version: u32,
    base_data: BaseData,
    /** The size of the region */
    region_size: (i32, i32, i32),
    /** The block info array */
    block_array: Vec<Block>,
    /** The Palette info array*/
    palette_array: Vec<Palette>,
}

impl Region {
    fn set_data_time(
        &mut self,
        create_time: i64,
        modify_time: i64,
    ) -> Result<bool, Box<dyn Error>> {
        self.base_data.set_time(create_time, modify_time)
    }

    fn get_size(&self) {
        println!("{}", size_of::<Region>());
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_new() -> *mut Region {
    let region = Box::new(Region::default());
    Box::into_raw(region)
}

#[unsafe(no_mangle)]
pub extern "C" fn region_free(region: *mut Region) {
    let r = unsafe { Box::from_raw(region) };
    drop(r);
}

#[unsafe(no_mangle)]
pub extern "C" fn region_set_time(
    region: *mut Region,
    create_time: i64,
    modify_time: i64,
) -> *const c_char {
    let r = unsafe { &mut *region };
    let set_time_real = r.set_data_time(create_time, modify_time);
    match set_time_real {
        Ok(_ret) => null(),
        Err(err) => string_to_ptr_fail_to_null(&err.to_string()),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn string_free(string: *mut c_char) {
    if string.is_null() {
        return;
    }
    drop(unsafe { CString::from_raw(string) });
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_create_timestamp(region: *mut Region) -> i64 {
    unsafe { (*region).base_data.get_create_timestamp() }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_modify_timestamp(region: *mut Region) -> i64 {
    unsafe { (*region).base_data.get_modify_timestamp() }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_size(region: *mut Region) {
    unsafe { (*region).get_size() };
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
            msg: i18n("Null pointer detected").to_string(),
        }));
    }
    let str = unsafe { CStr::from_ptr(string) };
    let ref_str = str.to_str()?;
    Ok(ref_str.to_string())
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_name(region: *mut Region) -> *const c_char {
    unsafe { string_to_ptr_fail_to_null((*region).base_data.get_name()) }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_set_name(region: *mut Region, string: *const c_char) -> *const c_char {
    let real_str = cstr_to_str(string);
    match real_str {
        Ok(str) => unsafe {
            (*region).base_data.set_name(&str);
            null()
        },
        Err(err) => string_to_ptr_fail_to_null(&err.to_string()),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_description(region: *mut Region) -> *const c_char {
    unsafe { string_to_ptr_fail_to_null((*region).base_data.get_description()) }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_set_description(
    region: *mut Region,
    string: *const c_char,
) -> *const c_char {
    let real_str = cstr_to_str(string);
    match real_str {
        Ok(str) => unsafe {
            (*region).base_data.set_description(&str);
            null()
        },
        Err(err) => string_to_ptr_fail_to_null(&err.to_string()),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_author(region: *mut Region) -> *const c_char {
    unsafe { string_to_ptr_fail_to_null((*region).base_data.get_author()) }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_set_author(region: *mut Region, string: *const c_char) -> *const c_char {
    let real_str = cstr_to_str(string);
    match real_str {
        Ok(str) => unsafe {
            (*region).base_data.set_author(&str);
            null()
        },
        Err(err) => string_to_ptr_fail_to_null(&err.to_string()),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_data_version(region: *mut Region) -> u32 {
    unsafe { (*region).data_version }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_set_data_version(region: *mut Region, data_version: u32) {
    unsafe { (*region).data_version = data_version };
}

#[unsafe(no_mangle)]
pub extern "C" fn region_set_size(region: *mut Region, x: i32, y: i32, z: i32) {
    unsafe { (*region).region_size = (x, y, z) };
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_x(region: *mut Region) -> i32 {
    unsafe { (*region).region_size.0 }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_y(region: *mut Region) -> i32 {
    unsafe { (*region).region_size.1 }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_z(region: *mut Region) -> i32 {
    unsafe { (*region).region_size.2 }
}

/* The two functions are used for Rust only */
/* WARNING: This function will take ownership of the hashmap */
#[unsafe(no_mangle)]
pub extern "C" fn block_new_to_region(
    region: *mut Region,
    index: usize,
    id: u32,
    tree: *mut HashMap<String, TreeValue>,
) {
    let opt: Option<HashMap<String, TreeValue>>;
    if tree.is_null() {
        opt = None;
    } else {
        let real_tree = unsafe { Box::from_raw(tree) };
        opt = Option::from(*real_tree);
    }
    let block = Block {
        id,
        block_entity: opt,
    };
    let r = unsafe { &mut *region };
    match r.block_array.get_mut(index) {
        Some(b) => *b = block,
        None => r.block_array.insert(index, block),
    }
}

/* This will create a new HashMap */
#[unsafe(no_mangle)]
pub extern "C" fn region_get_block_entity_tree_by_index(
    region: *mut Region,
    index: usize,
) -> *mut HashMap<String, TreeValue> {
    let block_array = unsafe { &(*region).block_array };
    if block_array.len() < index {
        return null_mut();
    }
    let opt = block_array[index].block_entity.clone();
    match opt {
        None => null_mut(),
        Some(tree) => {
            let ret = Box::new(tree);
            Box::into_raw(ret)
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_block_id_by_index(region: *mut Region, index: usize) -> u32 {
    unsafe { (&*region).block_array[index].id }
}

/* WARNING: This will take the ownership of the property */
#[unsafe(no_mangle)]
pub extern "C" fn region_append_palette(
    region: *mut Region,
    id_name: *const c_char,
    property: *mut Vec<(String, String)>,
) -> *const c_char {
    let palette_array = unsafe { &mut (*region).palette_array };
    let real_property = unsafe { *Box::from_raw(property) };
    let string = cstr_to_str(id_name);
    let real_string = match string {
        Ok(str) => str,
        Err(err) => return string_to_ptr_fail_to_null(&err.to_string()),
    };
    palette_array.push(Palette {
        id_name: real_string,
        property: real_property,
    });
    null()
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_palette_id_name(region: *mut Region, id: usize) -> *const c_char {
    let palette_array = unsafe { &(*region).palette_array };
    if palette_array.len() < id {
        return null();
    }
    let palette = &palette_array[id];
    string_to_ptr_fail_to_null(&palette.id_name)
}

/* This will create a new vector */
#[unsafe(no_mangle)]
pub extern "C" fn region_get_palette_property(
    region: *mut Region,
    index: usize,
) -> *mut Vec<(String, String)> {
    let palette_array = unsafe { &(*region).palette_array };
    if palette_array.len() < index {
        return null_mut();
    }
    let vec = palette_array[index].property.clone();
    let ret = Box::new(vec);
    Box::into_raw(ret)
}

/* It might be complicated... */
#[unsafe(no_mangle)]
pub extern "C" fn region_get_palette_property_len(region: *mut Region, id: usize) -> usize {
    let palette_array = unsafe { &(*region).palette_array };
    if palette_array.len() < id {
        return 0;
    }
    let palette = &palette_array[id];
    palette.property.len()
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_palette_property_name(
    region: *mut Region,
    id: usize,
    index: usize,
) -> *const c_char {
    let palette_array = unsafe { &(*region).palette_array };
    if palette_array.len() < id {
        return null();
    }
    let palette_property = &palette_array[id].property;
    if palette_property.len() < index {
        return null();
    }
    string_to_ptr_fail_to_null(&palette_property[index].0)
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_palette_property_data(
    region: *mut Region,
    id: usize,
    index: usize,
) -> *const c_char {
    let palette_array = unsafe { &(*region).palette_array };
    if palette_array.len() < id {
        return null();
    }
    let palette_property = &palette_array[id].property;
    if palette_property.len() < index {
        return null();
    }
    string_to_ptr_fail_to_null(&palette_property[index].1)
}

#[unsafe(no_mangle)]
pub extern "C" fn region_set_palette_property_name_and_data(
    region: *mut Region,
    id: usize,
    index: usize,
    name: *const c_char,
    data: *const c_char,
) -> *const c_char {
    let real_name = match cstr_to_str(name) {
        Ok(str) => str,
        Err(err) => return string_to_ptr_fail_to_null(&err.to_string()),
    };
    let real_data = match cstr_to_str(data) {
        Ok(str) => str,
        Err(err) => return string_to_ptr_fail_to_null(&err.to_string()),
    };
    let palette_array = unsafe { &mut (*region).palette_array };
    if palette_array.len() < id {
        /* Note: Can be translated */
        return string_to_ptr_fail_to_null(i18n("The index is out of range."));
    }
    let palette_property = &mut palette_array[id].property;
    if palette_property.len() < index {
        return string_to_ptr_fail_to_null(i18n("The index is out of range."));
    }
    palette_property.index_mut(index).0 = real_name;
    palette_property.index_mut(index).1 = real_data;
    null()
}

type ProgressFn = extern "C" fn(
    main_klass: *mut c_void,
    progress: c_int,
    format: *const c_char,
    text: *const c_char,
);

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
    if progress_fn as usize != 0 {
        progress_fn(main_klass, progress, msg, real_text);
    }
    string_free(msg);
    string_free(real_text);
}

fn file_try_uncompress_real(
    filename: *const c_char,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
) -> Result<Vec<u8>, Box<dyn Error>> {
    let str = cstr_to_str(filename)?;
    let mut file = File::open(str)?;
    let file_size = file.metadata()?.len();
    let mut start = Instant::now();
    let mut file_data = vec![];
    loop {
        let mut temp_data = vec![0; 100];
        let file_pos = file.try_clone()?.seek(SeekFrom::Current(0))?;
        if start.elapsed().as_secs() >= 5 {
            show_progress(
                progress_fn,
                main_klass,
                (file_pos * 100 / file_size) as c_int,
                i18n("Loading file."),
                &String::new(),
            );
            start = Instant::now();
        }

        let len = file.read(&mut temp_data)?;
        if len == 0 {
            break;
        }
        file_data.append(&mut temp_data);
    }

    let mut decoder: Decompress;
    let mut ret = vec![];

    if file_data.len() > 1 && file_data[0] == 0x1f && file_data[1] == 0x8b {
        /* file is gzip */
        decoder = Decompress::new_gzip(15);
    } else if file_data[0] == 0x78 {
        /* file is zlib */
        decoder = Decompress::new(true);
    } else {
        return Ok(file_data);
    }

    let mut pos: usize = 0;

    let data_size = file_data.len();
    loop {
        if start.elapsed().as_secs() >= 5 {
            show_progress(
                progress_fn,
                main_klass,
                (pos * 100 / data_size) as c_int,
                i18n("Uncompressing data."),
                &String::new(),
            );
            start = Instant::now();
        }

        let decompress_result =
            decoder.decompress_vec(&file_data[pos..], &mut ret, FlushDecompress::None)?;
        println!("{}", ret.capacity());
        match decompress_result {
            Status::Ok => {
            }
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
) -> *mut Vec<u8> {
    match file_try_uncompress_real(filename, progress_fn, main_klass) {
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

#[unsafe(no_mangle)]
pub extern "C" fn vec_free(vec: *mut Vec<u8>) {
    unsafe {
        drop(Box::from_raw(vec));
    }
}

/* Will take the ownership of vec */
#[unsafe(no_mangle)]
pub extern "C" fn vec_to_cstr(vec: *mut Vec<u8>) -> *mut c_char {
    let real_vec = unsafe { Box::from_raw(vec) };
    match String::from_utf8(*real_vec) {
        Ok(ret_str) => string_to_ptr_fail_to_null(&ret_str),
        Err(err) => string_to_ptr_fail_to_null(&err.to_string()),
    }
}
