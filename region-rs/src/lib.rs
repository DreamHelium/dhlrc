mod file;
mod region_getter;

use common_rs::ProgressFn;
use common_rs::i18n::i18n;
use common_rs::tree_value::TreeValue;
use common_rs::util::{cstr_to_str, string_to_ptr_fail_to_null};
use std::error::Error;
use std::ffi::c_char;
use std::ops::IndexMut;
use std::ptr::null;
use std::string::String;
use time::{Duration, UtcDateTime};

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
pub extern "C" fn region_set_data_version(region: *mut Region, data_version: u32) {
    unsafe { (*region).data_version = data_version };
}

#[unsafe(no_mangle)]
pub extern "C" fn region_set_size(region: *mut Region, x: i32, y: i32, z: i32) {
    unsafe { (*region).region_size = (x, y, z) };
}

#[unsafe(no_mangle)]
pub extern "C" fn region_set_offset(region: *mut Region, x: i32, y: i32, z: i32) {
    unsafe { (*region).region_offset = (x, y, z) };
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
