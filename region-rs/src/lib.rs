mod cancel_flag;
mod default;
mod file;
mod helper_struct;
mod region_getter;

use crate::default::NewOne;
use common_rs::i18n::i18n;
use common_rs::region::{Palette, Region};
use common_rs::util::{cstr_to_str, string_to_ptr_fail_to_null};
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
    if region.is_null() {
        return string_to_ptr_fail_to_null(i18n("The region is null."));
    }
    let real_name = match cstr_to_str(name) {
        Ok(str) => str,
        Err(err) => return string_to_ptr_fail_to_null(&err.to_string()),
    };
    let real_data = match cstr_to_str(data) {
        Ok(str) => str,
        Err(err) => return string_to_ptr_fail_to_null(&err.to_string()),
    };
    let palette_array = unsafe { &mut (*region).palette_array };
    if palette_array.len() <= id {
        /* Note: Can be translated */
        return string_to_ptr_fail_to_null(i18n("The index is out of range."));
    }
    let palette_property = &mut palette_array[id].property;
    if palette_property.len() <= index {
        return string_to_ptr_fail_to_null(i18n("The index is out of range."));
    }
    palette_property.index_mut(index).0 = real_name;
    palette_property.index_mut(index).1 = real_data;
    null()
}

/* Writes the given properties on a single block.
 *
 * The block's palette entry is copied, the copy is changed and only the given
 * block is pointed at it, so every other block that still uses the old entry
 * keeps its properties. Returns null on success and an error message (to be
 * released with `string_free`) on failure. An added property name is appended
 * to the copy. */
#[unsafe(no_mangle)]
pub extern "C" fn region_set_block_properties(
    region: *mut Region,
    index: usize,
    names: *const *const c_char,
    datas: *const *const c_char,
    count: usize,
    out_id: *mut u32,
) -> *const c_char {
    if region.is_null() {
        return string_to_ptr_fail_to_null(i18n("The region is null."));
    }
    if out_id.is_null() {
        return string_to_ptr_fail_to_null(i18n("The output pointer is null."));
    }
    if names.is_null() || datas.is_null() {
        return string_to_ptr_fail_to_null(i18n("The property list is null."));
    }
    if count == 0 {
        return string_to_ptr_fail_to_null(i18n("No property is given."));
    }
    let r = unsafe { &mut *region };
    if index >= r.block_array.len() {
        return string_to_ptr_fail_to_null(i18n("The index is out of range."));
    }
    let old_id = r.block_array[index] as usize;
    if old_id >= r.palette_array.len() {
        return string_to_ptr_fail_to_null(i18n("The palette index is out of range."));
    }

    /* Build the copy first: a bad name/value pair must leave the region as it
     * was instead of half modified. */
    let id_name = r.palette_array[old_id].id_name.clone();
    let mut property = r.palette_array[old_id].property.clone();
    for i in 0..count {
        let name_ptr = unsafe { *names.add(i) };
        let data_ptr = unsafe { *datas.add(i) };
        let real_name = match cstr_to_str(name_ptr) {
            Ok(str) => str,
            Err(err) => return string_to_ptr_fail_to_null(&err.to_string()),
        };
        let real_data = match cstr_to_str(data_ptr) {
            Ok(str) => str,
            Err(err) => return string_to_ptr_fail_to_null(&err.to_string()),
        };
        match property.iter_mut().find(|(key, _)| *key == real_name) {
            Some((_, value)) => *value = real_data,
            /* The dialog's rows are existing properties, but adding a new one
             * is cheap here and keeps the API useful. */
            None => property.push((real_name, real_data)),
        }
    }

    /* Every block with this palette entry has to keep it when nothing changed,
     * otherwise each OK press would grow the palette. */
    let new_id = if property == r.palette_array[old_id].property {
        old_id
    } else {
        r.palette_array.push(Palette { id_name, property });
        r.palette_array.len() - 1
    };
    r.block_array[index] = new_id as u32;
    unsafe { *out_id = new_id as u32 };
    null()
}

/// Sets the block at `index` to the state `name` with the given properties,
/// which is how a block is added or replaced.
///
/// The palette is looked through first: a state it already has is pointed at
/// rather than appended, so placing the same block a thousand times does not
/// grow the file. `count` may be 0 — most blocks take no properties at all — and
/// then `names` and `datas` may be null. Returns NULL on success and an error
/// message on failure; the palette id used is written to `out_id`.
#[unsafe(no_mangle)]
pub extern "C" fn region_set_block_state(
    region: *mut Region,
    index: usize,
    name: *const c_char,
    names: *const *const c_char,
    datas: *const *const c_char,
    count: usize,
    out_id: *mut u32,
) -> *const c_char {
    if region.is_null() {
        return string_to_ptr_fail_to_null(i18n("The region is null."));
    }
    if out_id.is_null() {
        return string_to_ptr_fail_to_null(i18n("The output pointer is null."));
    }
    if count > 0 && (names.is_null() || datas.is_null()) {
        return string_to_ptr_fail_to_null(i18n("The property list is null."));
    }
    let r = unsafe { &mut *region };
    if index >= r.block_array.len() {
        return string_to_ptr_fail_to_null(i18n("The index is out of range."));
    }
    let real_name = match cstr_to_str(name) {
        Ok(text) => text,
        Err(err) => return string_to_ptr_fail_to_null(&err.to_string()),
    };
    if real_name.is_empty() {
        return string_to_ptr_fail_to_null(i18n("No block name is given."));
    }

    /* Build the state first, so a bad pair leaves the region as it was. */
    let mut property: Vec<(String, String)> = Vec::with_capacity(count);
    for i in 0..count {
        let name_ptr = unsafe { *names.add(i) };
        let data_ptr = unsafe { *datas.add(i) };
        let real_property_name = match cstr_to_str(name_ptr) {
            Ok(text) => text,
            Err(err) => return string_to_ptr_fail_to_null(&err.to_string()),
        };
        let real_data = match cstr_to_str(data_ptr) {
            Ok(text) => text,
            Err(err) => return string_to_ptr_fail_to_null(&err.to_string()),
        };
        property.push((real_property_name, real_data));
    }

    let id = match r
        .palette_array
        .iter()
        .position(|palette| palette.id_name == real_name && same_properties(&palette.property, &property))
    {
        Some(id) => id,
        None => {
            r.palette_array.push(Palette {
                id_name: real_name,
                property,
            });
            r.palette_array.len() - 1
        }
    };
    r.block_array[index] = id as u32;
    unsafe { *out_id = id as u32 };
    null()
}

/// Whether two property lists say the same thing, whatever order they are in.
fn same_properties(one: &[(String, String)], other: &[(String, String)]) -> bool {
    if one.len() != other.len() {
        return false;
    }
    let mut one: Vec<&(String, String)> = one.iter().collect();
    let mut other: Vec<&(String, String)> = other.iter().collect();
    one.sort();
    other.sort();
    one == other
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

#[cfg(test)]
mod tests {
    use super::*;
    use crate::region_getter::{region_get_palette_property_len, region_get_palette_property_name};
    use common_rs::region::Palette;

    /* An error message is handed out as a C string the caller owns. */
    fn take_message(message: *const c_char) -> Option<String> {
        if message.is_null() {
            return None;
        }
        let text = unsafe { std::ffi::CStr::from_ptr(message) }
            .to_string_lossy()
            .into_owned();
        string_free(message as *mut c_char);
        Some(text)
    }

    /* Four blocks that all share the same palette entry, as an editor sees
     * after loading a file. */
    fn test_region() -> *mut Region {
        let region = region_new();
        let r = unsafe { &mut *region };
        r.region_size = (2, 1, 2);
        r.block_array = vec![0, 0, 0, 0];
        r.palette_array = vec![Palette {
            id_name: "minecraft:oak_slab".to_string(),
            property: vec![
                ("type".to_string(), "bottom".to_string()),
                ("waterlogged".to_string(), "false".to_string()),
            ],
        }];
        region
    }

    fn call_set_block_properties(
        region: *mut Region,
        index: usize,
        pairs: &[(&str, &str)],
    ) -> (Option<String>, u32) {
        let names: Vec<CString> = pairs
            .iter()
            .map(|(name, _)| CString::new(*name).unwrap())
            .collect();
        let datas: Vec<CString> = pairs
            .iter()
            .map(|(_, data)| CString::new(*data).unwrap())
            .collect();
        let name_ptrs: Vec<*const c_char> = names.iter().map(|name| name.as_ptr()).collect();
        let data_ptrs: Vec<*const c_char> = datas.iter().map(|data| data.as_ptr()).collect();
        let mut out_id = 0;
        let message = region_set_block_properties(
            region,
            index,
            name_ptrs.as_ptr(),
            data_ptrs.as_ptr(),
            name_ptrs.len(),
            &mut out_id,
        );
        (take_message(message), out_id)
    }

    fn properties(region: *mut Region, id: usize) -> Vec<(String, String)> {
        unsafe { (&(*region).palette_array)[id].property.clone() }
    }

    #[test]
    fn per_block_write_copies_the_entry_for_one_block_only() {
        let region = test_region();
        let (error, new_id) = call_set_block_properties(region, 1, &[("type", "top")]);
        assert!(error.is_none(), "unexpected error: {error:?}");
        assert_eq!(new_id, 1);
        unsafe {
            assert_eq!((*region).palette_array.len(), 2);
            /* Only block 1 moved to the copy; its neighbours still point at
             * the unchanged entry. */
            assert_eq!((*region).block_array, vec![0, 1, 0, 0]);
        }
        assert_eq!(
            properties(region, 0),
            vec![
                ("type".to_string(), "bottom".to_string()),
                ("waterlogged".to_string(), "false".to_string())
            ]
        );
        assert_eq!(
            properties(region, 1),
            vec![
                ("type".to_string(), "top".to_string()),
                ("waterlogged".to_string(), "false".to_string())
            ]
        );
        region_free(region);
    }

    #[test]
    fn in_place_write_changes_every_block_that_shares_the_entry() {
        let region = test_region();
        let name = CString::new("type").unwrap();
        let data = CString::new("top").unwrap();
        let error = region_set_palette_property_name_and_data(
            region,
            0,
            0,
            name.as_ptr(),
            data.as_ptr(),
        );
        assert!(error.is_null());
        unsafe {
            assert_eq!((*region).palette_array.len(), 1);
            assert_eq!((*region).block_array, vec![0, 0, 0, 0]);
        }
        assert_eq!(
            properties(region, 0)[0],
            ("type".to_string(), "top".to_string())
        );
        region_free(region);
    }

    #[test]
    fn unknown_property_name_is_added() {
        let region = test_region();
        let (error, new_id) =
            call_set_block_properties(region, 0, &[("half", "upper"), ("type", "top")]);
        assert!(error.is_none(), "unexpected error: {error:?}");
        assert_eq!(new_id, 1);
        assert_eq!(
            properties(region, 1),
            vec![
                ("type".to_string(), "top".to_string()),
                ("waterlogged".to_string(), "false".to_string()),
                ("half".to_string(), "upper".to_string())
            ]
        );
        region_free(region);
    }

    #[test]
    fn writing_the_same_values_keeps_the_palette() {
        let region = test_region();
        let (error, new_id) = call_set_block_properties(region, 2, &[("type", "bottom")]);
        assert!(error.is_none(), "unexpected error: {error:?}");
        assert_eq!(new_id, 0);
        unsafe {
            assert_eq!((*region).palette_array.len(), 1);
            assert_eq!((*region).block_array, vec![0, 0, 0, 0]);
        }
        region_free(region);
    }

    #[test]
    fn setting_a_block_state_reuses_a_palette_entry_it_already_has() {
        let region = test_region();
        let name = std::ffi::CString::new("minecraft:oak_slab").unwrap();
        let mut id = 0u32;

        /* The same state as the palette entry already holds, written in the
         * other order: no new entry. */
        let names = [std::ffi::CString::new("waterlogged").unwrap(),
                     std::ffi::CString::new("type").unwrap()];
        let datas = [std::ffi::CString::new("false").unwrap(),
                     std::ffi::CString::new("bottom").unwrap()];
        let name_ptrs: Vec<*const c_char> = names.iter().map(|s| s.as_ptr()).collect();
        let data_ptrs: Vec<*const c_char> = datas.iter().map(|s| s.as_ptr()).collect();
        let message = unsafe {
            region_set_block_state(region, 1, name.as_ptr(), name_ptrs.as_ptr(),
                                   data_ptrs.as_ptr(), 2, &mut id)
        };
        assert_eq!(take_message(message), None);
        assert_eq!(id, 0);
        let r = unsafe { &*region };
        assert_eq!(r.palette_array.len(), 1, "the palette must not grow");
        assert_eq!(r.block_array[1], 0);
        region_free(region);
    }

    #[test]
    fn setting_a_block_state_adds_an_entry_when_the_state_is_new() {
        let region = test_region();
        let name = std::ffi::CString::new("minecraft:oak_stairs").unwrap();
        let names = [std::ffi::CString::new("facing").unwrap()];
        let datas = [std::ffi::CString::new("east").unwrap()];
        let name_ptrs: Vec<*const c_char> = names.iter().map(|s| s.as_ptr()).collect();
        let data_ptrs: Vec<*const c_char> = datas.iter().map(|s| s.as_ptr()).collect();
        let mut id = 0u32;
        let message = unsafe {
            region_set_block_state(region, 2, name.as_ptr(), name_ptrs.as_ptr(),
                                   data_ptrs.as_ptr(), 1, &mut id)
        };
        assert_eq!(take_message(message), None);
        assert_eq!(id, 1);
        let r = unsafe { &*region };
        assert_eq!(r.palette_array.len(), 2);
        assert_eq!(r.palette_array[1].id_name, "minecraft:oak_stairs");
        assert_eq!(r.palette_array[1].property, [("facing".to_string(), "east".to_string())]);
        assert_eq!(r.block_array[2], 1);
        /* The neighbours are untouched. */
        assert_eq!(&r.block_array[..2], &[0, 0]);
        region_free(region);
    }

    #[test]
    fn a_block_with_no_properties_can_be_set() {
        let region = test_region();
        let name = std::ffi::CString::new("minecraft:stone").unwrap();
        let mut id = 0u32;
        let message = unsafe {
            region_set_block_state(region, 0, name.as_ptr(), std::ptr::null(),
                                   std::ptr::null(), 0, &mut id)
        };
        assert_eq!(take_message(message), None);
        let r = unsafe { &*region };
        assert_eq!(r.palette_array[id as usize].id_name, "minecraft:stone");
        assert!(r.palette_array[id as usize].property.is_empty());
        region_free(region);
    }

    #[test]
    fn setting_a_block_state_rejects_what_it_cannot_do() {
        let region = test_region();
        let name = std::ffi::CString::new("minecraft:stone").unwrap();
        let mut id = 0u32;
        /* Past the end of the region. */
        let message = unsafe {
            region_set_block_state(region, 99, name.as_ptr(), std::ptr::null(),
                                   std::ptr::null(), 0, &mut id)
        };
        assert!(take_message(message).is_some());
        /* No name at all. */
        let empty = std::ffi::CString::new("").unwrap();
        let message = unsafe {
            region_set_block_state(region, 0, empty.as_ptr(), std::ptr::null(),
                                   std::ptr::null(), 0, &mut id)
        };
        assert!(take_message(message).is_some());
        /* A property count with no arrays to read. */
        let message = unsafe {
            region_set_block_state(region, 0, name.as_ptr(), std::ptr::null(),
                                   std::ptr::null(), 2, &mut id)
        };
        assert!(take_message(message).is_some());
        /* Nothing was changed. */
        let r = unsafe { &*region };
        assert_eq!(r.palette_array.len(), 1);
        assert_eq!(r.block_array, [0, 0, 0, 0]);
        region_free(region);
    }

    #[test]
    fn bad_arguments_return_a_message_instead_of_panicking() {
        let region = test_region();

        let (error, _) = call_set_block_properties(region, 99, &[("type", "top")]);
        assert!(error.is_some());

        let (error, _) = call_set_block_properties(region, 0, &[]);
        assert!(error.is_some());
        region_free(region);
    }

    #[test]
    fn null_pointers_return_a_message() {
        let region = test_region();
        let mut out_id = 0;
        let message = region_set_block_properties(
            region,
            0,
            std::ptr::null(),
            std::ptr::null(),
            1,
            &mut out_id,
        );
        assert!(take_message(message).is_some());

        let message = region_set_block_properties(
            region,
            0,
            std::ptr::null(),
            std::ptr::null(),
            1,
            std::ptr::null_mut(),
        );
        assert!(take_message(message).is_some());
        region_free(region);
    }

    #[test]
    fn out_of_range_palette_and_property_indices_are_rejected() {
        let region = test_region();
        let name = CString::new("type").unwrap();
        let data = CString::new("top").unwrap();

        let message = region_set_palette_property_name_and_data(
            region,
            1, /* id == palette len */
            0,
            name.as_ptr(),
            data.as_ptr(),
        );
        assert!(take_message(message).is_some());

        let message = region_set_palette_property_name_and_data(
            region,
            0,
            2, /* index == property len */
            name.as_ptr(),
            data.as_ptr(),
        );
        assert!(take_message(message).is_some());

        /* The getters must answer with null rather than panic. */
        assert!(region_get_palette_property_name(region, 1, 0).is_null());
        assert!(region_get_palette_property_name(region, 0, 2).is_null());
        assert_eq!(region_get_palette_property_len(region, 1), 0);
        region_free(region);
    }
}
