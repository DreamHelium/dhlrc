use crate::{Region, string_to_ptr_fail_to_null};
use common_rs::tree_value::TreeValue;
use std::ffi::c_char;
use std::ptr;
use std::ptr::{null, null_mut};

#[unsafe(no_mangle)]
pub extern "C" fn region_get_palette_len(region: *mut Region) -> usize {
    unsafe { (*region).get_palette_len() }
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
            };
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

#[unsafe(no_mangle)]
pub extern "C" fn region_get_data_version(region: *mut Region) -> u32 {
    unsafe { (*region).data_version }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_author(region: *mut Region) -> *const c_char {
    unsafe { string_to_ptr_fail_to_null((*region).base_data.get_author()) }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_description(region: *mut Region) -> *const c_char {
    unsafe { string_to_ptr_fail_to_null((*region).base_data.get_description()) }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_name(region: *mut Region) -> *const c_char {
    unsafe { string_to_ptr_fail_to_null((*region).base_data.get_name()) }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_region_name(region: *mut Region) -> *const c_char {
    unsafe { string_to_ptr_fail_to_null((*region).base_data.get_region_name()) }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_index(region: *mut Region, x: i32, y: i32, z: i32) -> i32 {
    let size = unsafe { (*region).region_size };
    size.0 * size.2 * y + size.0 * z + x
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
