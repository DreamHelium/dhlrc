mod cancel_flag;
mod default;
mod file;
mod helper_struct;
mod region_getter;

use crate::default::NewOne;
use common_rs::ProgressFn;
use common_rs::i18n::i18n;
use common_rs::region::{BlockEntity, Palette, Region};
use common_rs::util::{cstr_to_str, string_to_ptr_fail_to_null};
use std::any::Any;
use std::collections::HashMap;
use std::ffi::{CString, c_char};
use std::ops::IndexMut;
use std::os::raw::c_int;
use std::ptr::{self, null};
use std::string::String;
use sysinfo::System;
use zuri_nbt::tag::{Compound, List};
use zuri_nbt::{NBTRoot, NBTTag};

#[unsafe(no_mangle)]
pub extern "C" fn string_free(string: *mut c_char) {
    if string.is_null() {
        return;
    }
    drop(unsafe { CString::from_raw(string) });
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
pub extern "C" fn region_new() -> *mut Region {
    let region = Box::new(Region::new_one());
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

#[unsafe(no_mangle)]
pub extern "C" fn nbt_root_get_string(root: *mut NBTRoot) -> *const c_char {
    string_to_ptr_fail_to_null(unsafe { &(*root).tag_name })
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_root_to_compound(root: *mut NBTRoot) -> *const Compound {
    unsafe {
        match &(*root).data {
            NBTTag::Compound(c) => ptr::from_ref(c),
            _ => return null(),
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_compound_len(compound: *const Compound) -> u32 {
    unsafe { (*compound).len() as u32 }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_compound_index_tag(compound: *const Compound, index: u32) -> *const NBTTag {
    unsafe { &(*compound) }.0.values().collect::<Vec<&NBTTag>>()[index as usize]
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_compound_index_key(compound: *const Compound, index: u32) -> *const c_char {
    string_to_ptr_fail_to_null(
        unsafe { &(*compound) }.0.keys().collect::<Vec<&String>>()[index as usize],
    )
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_tag_type_int(tag: *const NBTTag) -> c_int {
    match unsafe { &(*tag) } {
        NBTTag::Byte(_) => 1,
        NBTTag::Short(_) => 2,
        NBTTag::Int(_) => 3,
        NBTTag::Long(_) => 4,
        NBTTag::Float(_) => 5,
        NBTTag::Double(_) => 6,
        NBTTag::String(_) => 7,
        NBTTag::Compound(_) => 8,
        NBTTag::List(_) => 9,
        NBTTag::ByteArray(_) => 10,
        NBTTag::IntArray(_) => 11,
        NBTTag::LongArray(_) => 12,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_tag_type_string(tag: *const NBTTag) -> *const c_char {
    string_to_ptr_fail_to_null(&unsafe { &(*tag) }.tag_type().to_string())
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_tag_value(tag: *const NBTTag) -> *const c_char {
    let tag = unsafe { &(*tag) };
    if tag.tag_type() == Compound(HashMap::new()).tag_type()
        || tag.tag_type() == List(vec![]).tag_type()
    {
        return null();
    } else {
        let view = tag.view();
        if !view.is_container() {
            return match view.any_int() {
                Ok(i) => string_to_ptr_fail_to_null(&i.to_string()),
                Err(_) => match view.any_float() {
                    Ok(f) => string_to_ptr_fail_to_null(&f.to_string()),
                    Err(_) => match view.string() {
                        Ok(s) => string_to_ptr_fail_to_null(s),
                        Err(_) => null(),
                    },
                },
            };
        } else {
            return match tag {
                NBTTag::ByteArray(b) => string_to_ptr_fail_to_null(&format!("{:?}", b.0)),
                NBTTag::IntArray(i) => string_to_ptr_fail_to_null(&format!("{:?}", i.0)),
                NBTTag::LongArray(l) => string_to_ptr_fail_to_null(&format!("{:?}", l.0)),
                _ => null(),
            };
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_tag_list_to_list(tag: *const NBTTag) -> *const List {
    match unsafe { &(*tag) } {
        NBTTag::List(l) => ptr::from_ref(l),
        _ => null(),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_tag_compound_to_compound(tag: *const NBTTag) -> *const Compound {
    match unsafe { &(*tag) } {
        NBTTag::Compound(c) => ptr::from_ref(c),
        _ => null(),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_list_len(list: *const List) -> u32 {
    unsafe { (*list).len() as u32 }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_list_index_tag(list: *const List, index: u32) -> *const NBTTag {
    ptr::from_ref(&unsafe { &(*list) }.0[index as usize])
}
