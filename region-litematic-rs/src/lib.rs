use crab_nbt::NbtTag;
use crab_nbt_ext::{
    GetWithError, MyError, Palette, ProgressFn, TreeValue, get_palette_from_nbt_tag, i18n,
    init_translation_internal, nbt_create_real, show_progress, string_to_ptr_fail_to_null,
};
use formatx::formatx;
use std::collections::HashMap;
use std::error::Error;
use std::ffi::{CStr, c_char, c_int, c_void};
use std::ops::{IndexMut, Shl, Shr};
use std::ptr::{null, null_mut};
use std::sync::atomic::AtomicBool;
use std::time::Instant;
use sysinfo::System;
use tiny_varint::{VarInt, VarIntEncoder, varint_size};

static mut FREE_MEMORY: usize = 500 * 1024 * 1024;

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
    fn region_set_offset(region: *mut c_void, x: i32, y: i32, z: i32);
    fn region_get_offset_x(region: *mut c_void) -> i32;
    fn region_get_offset_y(region: *mut c_void) -> i32;
    fn region_get_offset_z(region: *mut c_void) -> i32;
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
    fn file_try_uncompress(
        filename: *const c_char,
        progress_fn: ProgressFn,
        main_klass: *mut c_void,
        failed: *mut c_int,
    ) -> *mut Vec<u8>;
    fn vec_free(vec: *mut Vec<u8>);
    fn vec_to_cstr(vec: *mut Vec<u8>) -> *mut c_char;
    fn region_get_index(region: *mut c_void, x: i32, y: i32, z: i32) -> i32;
    fn cancel_flag_is_cancelled(ptr: *const AtomicBool) -> c_int;
    fn region_set_blocks_from_vec(region: *mut c_void, blocks: *mut Vec<u8>);
}

#[unsafe(no_mangle)]
pub extern "C" fn region_type() -> *const c_char {
    string_to_ptr_fail_to_null("litematic")
}

#[unsafe(no_mangle)]
pub extern "C" fn region_is_multi() -> i32 {
    1
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_object(
    bytes: *mut Vec<u8>,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    cancel_flag: *const AtomicBool,
    object: *mut *mut crab_nbt::Nbt,
) -> *const c_char {
    match nbt_create_real(bytes, progress_fn, main_klass, cancel_flag) {
        Ok(nbt) => {
            if object.is_null() {
                return string_to_ptr_fail_to_null(i18n("Region value not provided"));
            }
            unsafe {
                *object = Box::into_raw(Box::new(nbt));
            }
            null()
        }
        Err(e) => string_to_ptr_fail_to_null(&e.to_string()),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_num(region: *mut crab_nbt::Nbt) -> i32 {
    let real_nbt = unsafe { Box::from_raw(region) };
    let clone_nbt = real_nbt.clone();
    let _ = Box::into_raw(real_nbt);
    let region_nbt = clone_nbt.get_compound_with_err("Regions");
    match region_nbt {
        Ok(r) => r.child_tags.len() as i32,
        Err(e) => 0,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_name_index(region: *mut crab_nbt::Nbt, index: i32) -> *const c_char {
    let real_nbt = unsafe { Box::from_raw(region) };
    let clone_nbt = real_nbt.clone();
    let _ = Box::into_raw(real_nbt);
    let region_nbt = clone_nbt.get_compound("Regions").unwrap();
    string_to_ptr_fail_to_null(&region_nbt.child_tags[index as usize].0)
}

fn get_bits(num: usize) -> u32 {
    let mut ret: u32 = 0;
    let mut number = num;
    loop {
        ret += 1;
        number >>= 1;
        if number == 0 {
            break;
        }
    }
    ret
}

fn finish_oom(system: &mut System) -> Result<(), MyError> {
    system.refresh_all();
    if unsafe { system.free_memory() < FREE_MEMORY as u64 } {
        return Err(MyError {
            msg: i18n("Out of memory!").to_string(),
        });
    }
    Ok(())
}

fn get_block_id(
    states: &Vec<i64>,
    block_num: i32,
    move_bit: u32,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    cancel_flag: *const AtomicBool,
    sys: &mut System,
) -> Result<Vec<u8>, Box<dyn Error>> {
    let mut time = Instant::now();
    let mut i = 0;
    let mut buf = vec![];
    let mut ret_size = 0;
    loop {
        if time.elapsed().as_secs() >= 1 {
            finish_oom(sys)?;
            let str = i18n("Reading block id: {} / {}.");
            let real_str = formatx!(str, i, block_num)?;
            show_progress(
                progress_fn,
                main_klass,
                (i as u64 * 100 / block_num as u64) as c_int,
                &real_str,
                &String::new(),
            );
            time = Instant::now();
        }

        let start_bit = i as u32 * move_bit;
        let start_state = start_bit / 64;
        let and_num = (1 << move_bit) - 1;
        let move_num = start_bit & 63;
        let end_num = move_num + move_bit;

        if unsafe { cancel_flag_is_cancelled(cancel_flag) } == 1 {
            return Err(Box::new(MyError {
                msg: i18n("Cancelled when reading blocks").to_string(),
            }));
        }

        let id;
        if end_num <= 64 {
            id = states[start_state as usize] as u64 >> move_num & and_num;
        } else {
            let move_num_2 = 64 - move_num;
            if (start_state + 1 >= block_num as u32) {
                return Err(Box::new(MyError {
                    msg: i18n("Out of range!").to_string(),
                }));
            }
            id = (((states[start_state as usize] as u64).shr(move_num as u64))
                | ((states[(start_state + 1) as usize] as u64).shl(move_num_2 as u64)))
                & and_num;
        }
        buf.push(id as u32);
        ret_size += varint_size(id);
        i += 1;
        if i == block_num as usize {
            show_progress(
                progress_fn,
                main_klass,
                100,
                i18n("Reading block finished!"),
                &String::new(),
            );
            break;
        }
    }
    println!("{ret_size} {block_num}");
    let mut ret = vec![0u8; ret_size];
    let mut encoder = VarIntEncoder::<u32>::new(&mut ret);
    i = 0;
    for value in buf {
        if time.elapsed().as_secs() >= 1 {
            finish_oom(sys)?;
            let str = i18n("Encoding block id: {} / {}.");
            let real_str = formatx!(str, i, block_num)?;
            show_progress(
                progress_fn,
                main_klass,
                (i as u64 * 100 / block_num as u64) as c_int,
                &real_str,
                &String::new(),
            );
            time = Instant::now();
        }
        match encoder.write(value) {
            Ok(a) => a,
            Err(_) => {
                return Err(Box::new(MyError {
                    msg: i18n("Write error!").to_string(),
                }));
            }
        };
        i += 1;
    }
    println!("{i}");
    Ok(ret)
}

#[unsafe(no_mangle)]
pub extern "C" fn init_translation(path: *const c_char) -> *const c_char {
    match init_translation_internal(path) {
        Ok(_) => null(),
        Err(e) => string_to_ptr_fail_to_null(&e.to_string()),
    }
}

fn region_create_from_bytes_internal(
    o_nbt: *mut crab_nbt::Nbt,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    cancel_flag: *const AtomicBool,
    index: i32,
) -> Result<*mut c_void, Box<dyn Error>> {
    let o_nbt_box = unsafe { Box::from_raw(o_nbt) };
    let nbt = o_nbt_box.clone();
    let _ = Box::into_raw(o_nbt_box);
    let data_version = nbt.get_int_with_err("MinecraftDataVersion")?;
    let metadata = nbt.get_compound_with_err("Metadata")?;
    let create_time = metadata.get_long_with_err("TimeCreated")?;
    let modify_time = metadata.get_long_with_err("TimeModified")?;
    let description = metadata.get_string_with_err("Description")?;
    let author = metadata.get_string_with_err("Author")?;
    let name = metadata.get_string_with_err("Name")?;

    let region_parent_nbt = nbt.get_compound_with_err("Regions")?;
    let region_nbt = &region_parent_nbt.child_tags[index as usize].1;
    let real_region_nbt = match region_nbt {
        NbtTag::Compound(c) => c,
        _ => {
            return Err(Box::new(MyError {
                msg: i18n("Wrong type of region.").to_string(),
            }));
        }
    };

    let size_nbt = real_region_nbt.get_compound_with_err("Size")?;
    let region_x = size_nbt.get_int_with_err("x")?.abs();
    let region_y = size_nbt.get_int_with_err("y")?.abs();
    let region_z = size_nbt.get_int_with_err("z")?.abs();

    let offset_nbt = real_region_nbt.get_compound_with_err("Position")?;
    let offset_x = offset_nbt.get_int_with_err("x")?;
    let offset_y = offset_nbt.get_int_with_err("y")?;
    let offset_z = offset_nbt.get_int_with_err("z")?;

    let block_states = real_region_nbt.get_long_array_with_err("BlockStates")?;

    let palette_list = real_region_nbt.get_list_with_err("BlockStatePalette")?;
    let palette_vec: Vec<Palette> = get_palette_from_nbt_tag(palette_list, cancel_flag)?;

    let block_num = region_x * region_y * region_z;
    let palette_num = palette_vec.len();
    let move_bit = get_bits(palette_num - 1);
    let real_move_bit = if move_bit <= 2 { 2 } else { move_bit };

    let mut sys = System::new_all();

    let block_ids = get_block_id(
        block_states,
        block_num,
        real_move_bit,
        progress_fn,
        main_klass,
        cancel_flag,
        &mut sys,
    )?;
    let blocks = block_ids;

    let entities_list = real_region_nbt.get_list_with_err("TileEntities")?;
    { /* TODO */ }

    unsafe {
        let region = region_new();
        region_set_data_version(region, data_version as u32);
        region_set_size(region, region_x, region_y, region_z);
        region_set_offset(region, offset_x, offset_y, offset_z);
        for palette in palette_vec {
            if { cancel_flag_is_cancelled(cancel_flag) } == 1 {
                region_free(region);
                return Err(Box::from(MyError {
                    msg: String::from(i18n("Adding palette is cancelled!")),
                }));
            }
            let string = string_to_ptr_fail_to_null(&palette.id_name);
            region_append_palette(region, string, Box::into_raw(Box::new(palette.property)));
            string_free(string);
        }
        region_set_blocks_from_vec(region, Box::into_raw(Box::new(blocks)));
        let real_name = string_to_ptr_fail_to_null(name);
        let real_description = string_to_ptr_fail_to_null(description);
        let real_author = string_to_ptr_fail_to_null(author);
        region_set_name(region, real_name);
        region_set_description(region, real_description);
        region_set_author(region, real_author);
        string_free(real_name);
        string_free(real_description);
        string_free(real_author);
        let msg = region_set_time(region, create_time, modify_time);
        if !msg.is_null() {
            region_free(region);
            let err = Box::new(MyError {
                msg: cstr_to_str(msg)?,
            });
            string_free(msg as *mut c_char);
            return Err(err);
        }

        Ok(region)
    }
}

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
pub extern "C" fn region_create_from_file_as_index(
    nbt: *mut crab_nbt::Nbt,
    progress_fn: ProgressFn,
    region: *mut *mut c_void,
    main_klass: *mut c_void,
    cancel_flag: *const AtomicBool,
    index: i32,
) -> *const c_char {
    let mut err_string: String = String::new();
    if !region.is_null() {
        unsafe {
            *region = match region_create_from_bytes_internal(
                nbt,
                progress_fn,
                main_klass,
                cancel_flag,
                index,
            ) {
                Ok(ret) => ret,
                Err(err) => {
                    err_string = err.to_string();
                    null_mut()
                }
            }
        }
    } else {
        err_string = String::from(i18n("Region value not provided"));
    }
    if !err_string.is_empty() {
        string_to_ptr_fail_to_null(&err_string)
    } else {
        null()
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn object_free(object: *mut crab_nbt::Nbt) {
    drop(unsafe { Box::from_raw(object) });
}
