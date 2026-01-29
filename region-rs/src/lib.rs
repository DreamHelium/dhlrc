mod i18n;

use crate::i18n::i18n;
use flate2::{Decompress, FlushDecompress, Status};
use std::error::Error;
use std::ffi::{CStr, CString, c_char, c_int, c_void};
use std::fmt::{Display, Formatter};
use std::fs::File;
use std::io::{Read, Seek, SeekFrom};
use std::ops::IndexMut;
use std::ptr;
use std::ptr::{null, null_mut};
use std::string::String;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Instant;
use sysinfo::System;
use time::{Duration, UtcDateTime};

static mut FREE_MEMORY: usize = 500 * 1024 * 1024;

#[derive(Clone)]
#[repr(i32)]
pub enum TreeValue {
    Byte(i8) = 1,
    Short(i16) = 2,
    Int(i32) = 3,
    Long(i64) = 4,
    Float(f32) = 5,
    Double(f64) = 6,
    String(String) = 7,
    ByteArray(Vec<i8>) = 8,
    IntArray(Vec<i32>) = 9,
    LongArray(Vec<i64>) = 10,
    List(Vec<TreeValue>) = 11,
    Compound(Vec<(String, TreeValue)>) = 12,
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
        self.create_time = UtcDateTime::from_unix_timestamp(create_time / 1000)?;
        self.create_time += Duration::milliseconds(create_time % 1000);
        self.modify_time = UtcDateTime::from_unix_timestamp(modify_time / 1000)?;
        self.modify_time += Duration::milliseconds(modify_time % 1000);
        Ok(true)
    }
    fn get_create_timestamp(&self) -> i64 {
        self.create_time.unix_timestamp() * 1000 + self.create_time.millisecond() as i64
    }

    fn get_modify_timestamp(&self) -> i64 {
        self.modify_time.unix_timestamp() * 1000 + self.modify_time.millisecond() as i64
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
            name: "Converted".to_string(),
        }
    }
}

struct Palette {
    id_name: String,
    property: Vec<(String, String)>,
}

pub struct BlockEntity {
    pos: (i32, i32, i32),
    entity: Vec<(String, TreeValue)>,
}

#[derive(Default)]
pub struct Region {
    /* Base information */
    data_version: u32,
    base_data: BaseData,
    /** The size of the region */
    region_size: (i32, i32, i32),
    /** The offset */
    region_offset: (i32, i32, i32),
    /** The block info array */
    block_array: Vec<u32>,
    /** The block entity array */
    block_entity_array: Vec<BlockEntity>,
    /** Entity array, use TreeValue::Compound */
    entity_array: Vec<Vec<(String, TreeValue)>>,
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

    fn get_size(&self) {}
    fn get_palette_len(&self) -> usize {
        self.palette_array.len()
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

#[unsafe(no_mangle)]
pub extern "C" fn region_get_index(region: *mut Region, x: i32, y: i32, z: i32) -> i32 {
    let size = unsafe { (*region).region_size };
    size.0 * size.2 * y + size.0 * z + x
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

#[unsafe(no_mangle)]
pub extern "C" fn region_set_offset(region: *mut Region, x: i32, y: i32, z: i32) {
    unsafe { (*region).region_offset = (x, y, z) };
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_offset_x(region: *mut Region) -> i32 {
    unsafe { (*region).region_offset.0 }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_offset_y(region: *mut Region) -> i32 {
    unsafe { (*region).region_offset.1 }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_offset_z(region: *mut Region) -> i32 {
    unsafe { (*region).region_offset.2 }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_block_id_by_index(region: *mut Region, index: usize) -> u32 {
    let r = unsafe { &*region };
    r.block_array[index]
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
pub extern "C" fn region_set_blocks_from_vec(region: *mut Region, blocks: *mut Vec<u32>) {
    let r = unsafe { &mut *region };
    let real_blocks = unsafe { Box::from_raw(blocks) };
    r.block_array = *real_blocks;
}

#[unsafe(no_mangle)]
pub extern "C" fn region_set_entity_from_vec(
    region: *mut Region,
    entities: *mut Vec<Vec<(String, TreeValue)>>,
) {
    let r = unsafe { &mut *region };
    let real_entities = unsafe { Box::from_raw(entities) };
    r.entity_array = *real_entities;
}

#[unsafe(no_mangle)]
pub extern "C" fn region_set_block_entities_from_vec(
    region: *mut Region,
    block_entities: *mut Vec<BlockEntity>,
) {
    let r = unsafe { &mut *region };
    let real_blocks = unsafe { Box::from_raw(block_entities) };
    r.block_entity_array = *real_blocks;
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

type ProgressFn = Option<
    extern "C" fn(
        main_klass: *mut c_void,
        progress: c_int,
        format: *const c_char,
        text: *const c_char,
    ),
>;

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
    if !progress_fn.is_none() {
        if progress_fn.unwrap() as usize != 0 {
            progress_fn.unwrap()(main_klass, progress, msg, real_text);
        }
    }

    string_free(msg);
    string_free(real_text);
}

fn finish_oom(system: &mut System) -> Result<(), MyError> {
    system.refresh_all();
    if unsafe { system.available_memory() < FREE_MEMORY as u64 } {
        println!("{}", system.free_memory());
        return Err(MyError {
            msg: i18n("Out of memory!").to_string(),
        });
    }
    Ok(())
}

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
        if start.elapsed().as_secs() >= 1 {
            finish_oom(&mut sys)?;
            show_progress(
                progress_fn,
                main_klass,
                (file_pos * 100 / file_size) as c_int,
                i18n("Loading file."),
                &String::new(),
            );
            start = Instant::now();
        }

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
        if start.elapsed().as_secs() >= 1 {
            finish_oom(&mut sys)?;
            show_progress(
                progress_fn,
                main_klass,
                (pos * 100 / data_size) as c_int,
                i18n("Uncompressing data."),
                &String::new(),
            );
            start = Instant::now();
        }

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

#[unsafe(no_mangle)]
pub extern "C" fn cancel_flag_new() -> *const AtomicBool {
    Arc::into_raw(Arc::new(AtomicBool::new(false)))
}

#[unsafe(no_mangle)]
pub extern "C" fn cancel_flag_is_cancelled(ptr: *const AtomicBool) -> c_int {
    if ptr.is_null() {
        return 0;
    }
    let arc = unsafe { Arc::from_raw(ptr) };
    let ret = arc.load(Ordering::SeqCst);
    let _ = Arc::into_raw(arc);
    ret as c_int
}

#[unsafe(no_mangle)]
pub extern "C" fn cancel_flag_cancel(ptr: *const AtomicBool) {
    if ptr.is_null() {
        return;
    }
    let arc = unsafe { Arc::from_raw(ptr) };
    arc.store(true, Ordering::SeqCst);
    let _ = Arc::into_raw(arc);
}

#[unsafe(no_mangle)]
pub extern "C" fn cancel_flag_destroy(ptr: *const AtomicBool) {
    if !ptr.is_null() {
        unsafe {
            Arc::from_raw(ptr);
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn cancel_flag_clone(ptr: *const AtomicBool) -> *const AtomicBool {
    if ptr.is_null() {
        return null();
    }
    let arc = unsafe { Arc::from_raw(ptr) };
    let cloned_arc = arc.clone();
    let _ = Arc::into_raw(arc);
    Arc::into_raw(cloned_arc)
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_palette_len(region: *mut Region) -> usize {
    unsafe { (*region).get_palette_len() }
}

#[unsafe(no_mangle)]
pub extern "C" fn get_system_info_object() -> *mut System {
    Box::into_raw(Box::new(System::new_all()))
}

#[unsafe(no_mangle)]
pub extern "C" fn get_free_memory(system: *mut System) -> u64 {
    let sys = unsafe { &mut *system };
    sys.refresh_memory();
    sys.free_memory()
}

#[unsafe(no_mangle)]
pub extern "C" fn system_info_object_free(system: *mut System) {
    drop(unsafe { Box::from_raw(system) })
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_block_entity(
    region: *mut Region,
    index: u32,
) -> *const Vec<(String, TreeValue)> {
    let r = unsafe { &*region };
    for entity in &r.block_entity_array {
        let entity_index = region_get_index(region, entity.pos.0, entity.pos.1, entity.pos.2);
        if entity_index == index as i32 {
            return ptr::from_ref(&entity.entity);
        }
    }
    null()
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_entity_len(region: *mut Region) -> usize {
    unsafe { (*region).entity_array.len() }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_entity_id(region: *mut Region, index: usize) -> *const c_char {
    let entity = unsafe { &(&*region).entity_array[index] };
    for (str, val) in entity {
        if str == "id" {
            return match val {
                TreeValue::String(s) => string_to_ptr_fail_to_null(&s),
                _ => null(),
            }
        }
    }
    null()
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_entity(
    region: *mut Region,
    index: usize,
) -> *const Vec<(String, TreeValue)> {
    let r = unsafe { &*region };
    ptr::from_ref(&r.entity_array[index])
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_vec_get_len(nbt: *const Vec<(String, TreeValue)>) -> usize {
    if !nbt.is_null() {
        unsafe { nbt.as_ref().unwrap().len() }
    } else {
        0
    }
}

impl TreeValue {
    fn type_to_str(&self) -> &str {
        match self {
            TreeValue::Byte(_) => i18n("Byte"),
            TreeValue::Int(_) => i18n("Int"),
            TreeValue::Short(_) => i18n("Short"),
            TreeValue::Long(_) => i18n("Long"),
            TreeValue::ByteArray(_) => i18n("Byte Array"),
            TreeValue::LongArray(_) => i18n("Long Array"),
            TreeValue::String(_) => i18n("String"),
            TreeValue::Float(_) => i18n("Float"),
            TreeValue::Double(_) => i18n("Double"),
            TreeValue::IntArray(_) => i18n("Int Array"),
            TreeValue::List(_) => i18n("List"),
            TreeValue::Compound(_) => i18n("Compound"),
        }
    }

    fn to_string(&self) -> String {
        match self {
            TreeValue::Byte(b) => b.to_string(),
            TreeValue::Int(i) => i.to_string(),
            TreeValue::Short(s) => s.to_string(),
            TreeValue::Long(l) => l.to_string(),
            TreeValue::ByteArray(ba) => format!("{:?}", ba),
            TreeValue::LongArray(la) => format!("{:?}", la),
            TreeValue::String(s) => s.clone(),
            TreeValue::Float(f) => f.to_string(),
            TreeValue::Double(d) => d.to_string(),
            TreeValue::IntArray(ia) => format!("{:?}", ia),
            TreeValue::List(_) => String::new(),
            TreeValue::Compound(_) => String::new(),
        }
    }

    fn to_int(&self) -> i32 {
        unsafe { *(self as *const TreeValue as *const i32) }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_vec_get_key(
    nbt: *const Vec<(String, TreeValue)>,
    index: usize,
) -> *const c_char {
    if !nbt.is_null() {
        unsafe { string_to_ptr_fail_to_null(&nbt.as_ref().unwrap()[index].0) }
    } else {
        null()
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_vec_get_value_type(
    nbt: *const Vec<(String, TreeValue)>,
    index: usize,
) -> *const c_char {
    if !nbt.is_null() {
        unsafe { string_to_ptr_fail_to_null(&nbt.as_ref().unwrap()[index].1.type_to_str()) }
    } else {
        null()
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_vec_get_value_string(
    nbt: *const Vec<(String, TreeValue)>,
    index: usize,
) -> *const c_char {
    if !nbt.is_null() {
        unsafe { string_to_ptr_fail_to_null(&nbt.as_ref().unwrap()[index].1.to_string()) }
    } else {
        null()
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_vec_get_value_type_int(
    nbt: *const Vec<(String, TreeValue)>,
    index: usize,
) -> i32 {
    if !nbt.is_null() {
        unsafe { nbt.as_ref().unwrap()[index].1.to_int() }
    } else {
        0
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_vec_get_value_to_child(
    nbt: *const Vec<(String, TreeValue)>,
    index: usize,
) -> *const Vec<(String, TreeValue)> {
    if !nbt.is_null() {
        let value = unsafe { &nbt.as_ref().unwrap()[index].1 };
        match value {
            TreeValue::Compound(c) => ptr::from_ref(c),
            _ => null(),
        }
    } else {
        null()
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_vec_get_value_list_to_child(
    nbt: *const Vec<(String, TreeValue)>,
    index: usize,
) -> *const Vec<TreeValue> {
    if !nbt.is_null() {
        let value = unsafe { &nbt.as_ref().unwrap()[index].1 };
        match value {
            TreeValue::List(l) => ptr::from_ref(l),
            _ => null(),
        }
    } else {
        null()
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_vec_tree_value_get_len(tree_value: *const Vec<TreeValue>) -> usize {
    unsafe { (*tree_value).len() }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_vec_tree_value_get_tree_value(
    tree_value: *const Vec<TreeValue>,
    index: usize,
) -> *const TreeValue {
    unsafe { ptr::from_ref(&tree_value.as_ref().unwrap()[index]) }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_tree_value_get_value_string(tree_value: *const TreeValue) -> *const c_char {
    unsafe { string_to_ptr_fail_to_null(&(*tree_value).to_string()) }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_tree_value_get_type_string(tree_value: *const TreeValue) -> *const c_char {
    unsafe { string_to_ptr_fail_to_null(&(*tree_value).type_to_str()) }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_tree_value_get_type_int(tree_value: *const TreeValue) -> i32 {
    unsafe { (*tree_value).to_int() }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_tree_value_get_value_to_child(
    tree_value: *const TreeValue,
) -> *const Vec<(String, TreeValue)> {
    if !tree_value.is_null() {
        let value = unsafe { &*tree_value };
        match value {
            TreeValue::Compound(c) => ptr::from_ref(&c),
            _ => null(),
        }
    } else {
        null()
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_tree_value_get_value_list_to_child(
    tree_value: *const TreeValue,
) -> *const Vec<TreeValue> {
    if !tree_value.is_null() {
        let value = unsafe { &*tree_value };
        match value {
            TreeValue::List(l) => ptr::from_ref(l),
            _ => null(),
        }
    } else {
        null()
    }
}
