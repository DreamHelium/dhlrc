mod file;
mod region_getter;

use common_rs::ProgressFn;
use common_rs::i18n::i18n;
use common_rs::tree_value::TreeValue;
use common_rs::util::{cstr_to_str, string_to_ptr_fail_to_null};
use std::ffi::c_char;
use std::ops::IndexMut;
use std::ptr::null;
use std::string::String;
use common_rs::region::{BlockEntity, Palette, Region};

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
pub extern "C" fn region_set_region_name(
    region: *mut Region,
    string: *const c_char,
) -> *const c_char {
    let real_str = cstr_to_str(string);
    match real_str {
        Ok(str) => unsafe {
            (*region).base_data.set_region_name(&str);
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
