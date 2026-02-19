use common_rs::ProgressFn;
use common_rs::i18n::i18n;
use common_rs::my_error::MyError;
use common_rs::region::{BlockEntity, Palette, Region};
use common_rs::tree_value::TreeValue;
use common_rs::util::string_to_ptr_fail_to_null;
use crab_nbt::{Nbt, NbtTag};
use crab_nbt_ext::{
    GetWithError, convert_nbt_tag_to_tree_value, convert_nbt_to_vec, get_palette_from_nbt_tag,
    gettext_text,
};
use formatx::formatx;
use std::error::Error;
use std::ffi::{c_char, c_int, c_void};
use std::ops::{Shl, Shr};
use std::ptr::{null, null_mut};
use std::sync::atomic::AtomicBool;
use std::time::Instant;
use sysinfo::System;

#[link(name = "region_rs")]
unsafe extern "C" {
    fn cancel_flag_is_cancelled(ptr: *const AtomicBool) -> c_int;
    pub fn real_show_progress(
        instant: &mut Instant,
        system: &mut System,
        progress_fn: ProgressFn,
        main_klass: *mut c_void,
        percentage: c_int,
        msg: &str,
        text: &str,
    ) -> Result<(), MyError>;
}

#[derive(Default)]
pub struct InputConfig {
    ignore_base_data: bool,
}

#[unsafe(no_mangle)]
pub extern "C" fn input_config_new() -> *mut InputConfig {
    Box::into_raw(Box::new(InputConfig::default()))
}

#[unsafe(no_mangle)]
pub extern "C" fn input_config_free(input_config: *mut InputConfig) {
    drop(unsafe { Box::from_raw(input_config) })
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
pub extern "C" fn region_num(region: *mut Nbt) -> i32 {
    if !region.is_null() {
        let real_nbt = unsafe { Box::from_raw(region) };
        let clone_nbt = real_nbt.clone();
        let _ = Box::into_raw(real_nbt);
        let region_nbt = clone_nbt.get_compound_with_err("Regions");
        match region_nbt {
            Ok(r) => r.child_tags.len() as i32,
            Err(_) => 0,
        }
    } else {
        0
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_name_index(region: *mut Nbt, index: i32) -> *const c_char {
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

fn get_block_id(
    states: &Vec<i64>,
    block_num: i32,
    move_bit: u32,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    cancel_flag: *const AtomicBool,
    sys: &mut System,
) -> Result<Vec<u32>, Box<dyn Error>> {
    let mut time = Instant::now();
    let mut i = 0;
    let mut buf = vec![];
    loop {
        let str = gettext_text(i18n("Reading block id: {} / {}."));
        let real_str = formatx!(str, i, block_num)?;
        unsafe {
            real_show_progress(
                &mut time,
                sys,
                progress_fn,
                main_klass,
                (i as u64 * 100 / block_num as u64) as c_int,
                &real_str,
                "",
            )?;
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
            if start_state + 1 >= block_num as u32 {
                return Err(Box::new(MyError {
                    msg: i18n("Out of range!").to_string(),
                }));
            }
            id = (((states[start_state as usize] as u64).shr(move_num as u64))
                | ((states[(start_state + 1) as usize] as u64).shl(move_num_2 as u64)))
                & and_num;
        }
        buf.push(id as u32);
        i += 1;
        if i == block_num as usize {
            unsafe {
                real_show_progress(
                    &mut Instant::now(),
                    &mut System::new_all(),
                    progress_fn,
                    main_klass,
                    100,
                    i18n("Reading block finished!"),
                    "",
                )?;
            }
            break;
        }
    }
    Ok(buf)
}

fn region_create_from_bytes_internal(
    o_nbt: *mut Nbt,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    cancel_flag: *const AtomicBool,
    index: i32,
) -> Result<*mut Region, Box<dyn Error>> {
    let nbt = unsafe { (*o_nbt).clone() };
    let data_version = nbt.get_int_with_err("MinecraftDataVersion")?;
    let metadata = nbt.get_compound_with_err("Metadata")?;
    let create_time = metadata.get_long_with_err("TimeCreated")?;
    let modify_time = metadata.get_long_with_err("TimeModified")?;
    let description = metadata.get_string_with_err("Description")?;
    let author = metadata.get_string_with_err("Author")?;
    let name = metadata.get_string_with_err("Name")?;

    let region_parent_nbt = nbt.get_compound_with_err("Regions")?;
    let region_name = &region_parent_nbt.child_tags[index as usize].0;
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
    let mut tile_entities_vec = vec![];
    let tile_entities_list = real_region_nbt.get_list_with_err("TileEntities")?;
    for tile_entity in tile_entities_list {
        let real_tile_entity = match tile_entity {
            NbtTag::Compound(c) => c,
            _ => {
                return Err(Box::new(MyError {
                    msg: i18n("Wrong type of tile entity.").to_string(),
                }));
            }
        };
        let entity_x = real_tile_entity.get_int_with_err("x")?;
        let entity_y = real_tile_entity.get_int_with_err("y")?;
        let entity_z = real_tile_entity.get_int_with_err("z")?;
        let entity_index = region_x * region_z * entity_y + region_x * entity_z + entity_x;
        let entity_id = blocks[entity_index as usize];
        let id = real_tile_entity.get_string("id");
        let mut has_id: bool = true;
        let real_id = match id {
            Some(s) => s,
            None => {
                has_id = false;
                &palette_vec[entity_id as usize].id_name
            }
        };
        let mut single_entity_vec = vec![];
        for (str, val) in &real_tile_entity.child_tags {
            if str == "x" || str == "y" || str == "z" {
                continue;
            }
            let tree_value = convert_nbt_tag_to_tree_value(val);
            single_entity_vec.push((str.to_string(), tree_value));
        }
        if !has_id {
            single_entity_vec.push(("id".to_string(), TreeValue::String(real_id.to_string())));
        }
        let block_entity = BlockEntity {
            pos: (entity_x, entity_y, entity_z),
            entity: single_entity_vec,
        };
        tile_entities_vec.push(block_entity);
    }
    let mut entities_vec = vec![];
    let entities_list = real_region_nbt.get_list_with_err("Entities")?;
    for entity in entities_list {
        if unsafe { cancel_flag_is_cancelled(cancel_flag) } == 1 {
            return Err(Box::from(MyError {
                msg: String::from(i18n("Reading entities is cancelled!")),
            }));
        }
        let internal_entity = match entity {
            NbtTag::Compound(c) => c,
            _ => {
                return Err(Box::from(MyError {
                    msg: String::from(i18n("Wrong type of entity!")),
                }));
            }
        };
        let value = convert_nbt_to_vec(internal_entity);
        entities_vec.push(value);
    }

    let mut region = Region::default();
    region.data_version = data_version as u32;
    region.region_size = (region_x, region_y, region_z);
    region.region_offset = (offset_x, offset_y, offset_z);
    region.palette_array = palette_vec;
    region.block_array = blocks;
    region.base_data.set_name(name);
    region.base_data.set_description(description);
    region.base_data.set_author(author);
    region.base_data.set_region_name(region_name);
    region.block_entity_array = tile_entities_vec;
    region.set_data_time(create_time, modify_time)?;

    Ok(Box::into_raw(Box::new(region)))
}

#[unsafe(no_mangle)]
pub extern "C" fn region_create_from_file_as_index(
    nbt: *mut Nbt,
    progress_fn: ProgressFn,
    region: *mut *mut Region,
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
pub extern "C" fn object_free(object: *mut Nbt) {
    drop(unsafe { Box::from_raw(object) });
}
