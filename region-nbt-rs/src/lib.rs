mod config;
mod output;

use common_rs::i18n::i18n;
use common_rs::my_error::MyError;
use common_rs::region::{BlockEntity, Palette, Region};
use common_rs::tree_value::TreeValue;
use common_rs::util::finish_oom;
use common_rs::util::show_progress;
use common_rs::util::{real_show_progress, string_to_ptr_fail_to_null};
use common_rs::{ProgressFn, show_progress_macro};
use crab_nbt::{Nbt, NbtCompound, NbtTag};
use crab_nbt_ext::{
    GetWithError, convert_nbt_to_vec, get_compound, get_palette_from_nbt_tag, gettext_text,
    nbt_create_real,
};
use formatx::formatx;
use std::collections::HashMap;
use std::error::Error;
use std::ffi::{c_char, c_int, c_void};
use std::io::Write;
use std::ptr::{null, null_mut};
use std::string::String;
use std::sync::atomic::AtomicBool;
use std::time::Instant;
use sysinfo::System;

#[link(name = "region_rs")]
unsafe extern "C" {
    fn region_new() -> *mut Region;
    fn cancel_flag_is_cancelled(ptr: *const AtomicBool) -> c_int;
    fn string_free(string: *mut c_char);
    fn region_get_data_version(region: *mut c_void) -> u32;
    fn region_get_x(region: *mut c_void) -> i32;
    fn region_get_y(region: *mut c_void) -> i32;
    fn region_get_z(region: *mut c_void) -> i32;
    fn region_get_block_id_by_index(region: *mut c_void, index: usize) -> u32;
    fn region_get_palette_id_name(region: *mut c_void, id: usize) -> *const c_char;
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
    fn region_get_entity_len(region: *mut c_void) -> usize;
    fn region_get_entity_id(region: *mut c_void, index: usize) -> *const c_char;
    fn region_get_entity(region: *mut c_void, index: usize) -> *const Vec<(String, TreeValue)>;
    fn region_get_block_entity(region: *mut c_void, index: u32) -> *const Vec<(String, TreeValue)>;
    fn region_get_palette_len(region: *mut c_void) -> usize;
    fn vec_try_compress(
        vec: *mut Vec<u8>,
        progress_fn: ProgressFn,
        main_klass: *mut c_void,
        failed: *mut c_int,
        zlib: bool,
        cancel_flag: *const AtomicBool,
        elapsed_millisecs: u64,
        free_memory: u64,
    ) -> *mut Vec<u8>;
}

#[unsafe(no_mangle)]
pub extern "C" fn region_type() -> *const c_char {
    string_to_ptr_fail_to_null("nbt")
}

#[unsafe(no_mangle)]
pub extern "C" fn region_is_multi() -> i32 {
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn region_file_suffix() -> *const c_char {
    string_to_ptr_fail_to_null("nbt")
}

#[unsafe(no_mangle)]
pub extern "C" fn region_base_type() -> *const c_char {
    string_to_ptr_fail_to_null("JavaNBT")
}

#[unsafe(no_mangle)]
pub extern "C" fn region_file_type() -> *const c_char {
    string_to_ptr_fail_to_null(&gettext_text(i18n("NBT File (*.nbt)")))
}

fn get_int_from_nbt_tag(nbt: &NbtTag) -> Result<i32, MyError> {
    match nbt {
        NbtTag::Int(x) => Ok(*x),
        _ => Err(MyError {
            msg: String::from(i18n("Wrong type of size!")),
        }),
    }
}

fn get_double_from_nbt_tag(nbt: &NbtTag) -> Result<f64, MyError> {
    match nbt {
        NbtTag::Double(x) => Ok(*x),
        _ => Err(MyError {
            msg: String::from(i18n("Wrong type of size!")),
        }),
    }
}

fn get_size(nbt: &Vec<NbtTag>) -> Result<(i32, i32, i32), MyError> {
    if nbt.len() != 3 {
        return Err(MyError {
            msg: i18n("The length of the size is wrong.").to_string(),
        });
    }
    let x = get_int_from_nbt_tag(&nbt[0])?;
    let y = get_int_from_nbt_tag(&nbt[1])?;
    let z = get_int_from_nbt_tag(&nbt[2])?;
    Ok((x, y, z))
}

fn get_size_double(nbt: &Vec<NbtTag>) -> Result<(f64, f64, f64), MyError> {
    if nbt.len() != 3 {
        return Err(MyError {
            msg: i18n("The length of the size is wrong.").to_string(),
        });
    }
    let x = get_double_from_nbt_tag(&nbt[0])?;
    let y = get_double_from_nbt_tag(&nbt[1])?;
    let z = get_double_from_nbt_tag(&nbt[2])?;
    Ok((x, y, z))
}

fn region_get_entity_internal(
    nbt: &Nbt,
    cancel_flag: *const AtomicBool,
) -> Result<Vec<Vec<(String, TreeValue)>>, Box<dyn Error>> {
    let entity_nbt = nbt.get_list_with_err("entities")?;
    let mut ret = vec![];
    for entity in entity_nbt {
        if unsafe { cancel_flag_is_cancelled(cancel_flag) } == 1 {
            return Err(Box::from(MyError {
                msg: String::from(i18n("Reading entities is cancelled!")),
            }));
        }

        get_compound!(internal_entity, entity, i18n("Wrong type of entity!"));
        /* Then we need to process the nbt */
        let real_nbt = internal_entity.get_compound_with_err("nbt")?;
        let block_pos = internal_entity.get_list_with_err("pos")?;
        let block_pos_value = get_size_double(block_pos)?;

        let mut value = convert_nbt_to_vec(real_nbt);
        for (str, val) in &mut value {
            if str == "Pos" {
                match val {
                    TreeValue::List(l) => {
                        fn modify_tree_value_double(
                            tree_value: &mut TreeValue,
                            value: f64,
                        ) -> Result<(), MyError> {
                            match tree_value {
                                TreeValue::Double(d) => *d = value,
                                _ => {
                                    return Err(MyError {
                                        msg: i18n("Not a double value").to_string(),
                                    });
                                }
                            }
                            Ok(())
                        }
                        modify_tree_value_double(&mut l[0], block_pos_value.0)?;
                        modify_tree_value_double(&mut l[1], block_pos_value.1)?;
                        modify_tree_value_double(&mut l[2], block_pos_value.2)?;
                    }
                    _ => {
                        return Err(Box::from(MyError {
                            msg: String::from(i18n("Wrong type of entity's pos!")),
                        }));
                    }
                }
            }
        }
        /* Finally we put the parent into ret */
        ret.push(value);
    }
    Ok(ret)
}

fn region_create_from_bytes_internal(
    o_nbt: *mut Nbt,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    cancel_flag: *const AtomicBool,
    elapsed_millisecs: u128,
    free_memory: u64,
) -> Result<*mut Region, Box<dyn Error>> {
    let nbt = unsafe { &*o_nbt };
    let data_version = nbt.get_int_with_err("DataVersion")?;
    let size_compound = nbt.get_list_with_err("size")?;
    let size_compound_size = size_compound.len();
    if size_compound_size != 3 {
        return Err(Box::from(MyError {
            msg: String::from(i18n("Wrong size count!")),
        }));
    }
    let region_size = get_size(size_compound)?;
    let x = region_size.0;
    let y = region_size.1;
    let z = region_size.2;
    let palette_list = nbt.get_list_with_err("palette")?;
    let mut palette_vec: Vec<Palette> = get_palette_from_nbt_tag(palette_list, cancel_flag)?;
    let mut air_palette = 0;
    let mut i = 0;
    for palette in &palette_vec {
        if palette.id_name.eq("minecraft:air") {
            air_palette = i;
            break;
        }
        i += 1;
    }
    palette_vec.swap(0, air_palette);

    let block_compound = nbt.get_list_with_err("blocks")?;
    let mut blocks: Vec<u32> = vec![0; (x * y * z) as usize];
    let mut block_entities: Vec<BlockEntity> = vec![];

    let mut min_x = 0;
    let mut min_y = 0;
    let mut min_z = 0;
    let mut sys = System::new_all();

    i = 0;

    fn get_internal_block(block: &NbtTag) -> Result<&NbtCompound, MyError> {
        match block {
            NbtTag::Compound(c) => Ok(c),
            _ => Err(MyError {
                msg: String::from(i18n("Wrong type of block!")),
            }),
        }
    }

    for block in block_compound {
        if unsafe { cancel_flag_is_cancelled(cancel_flag) } == 1 {
            return Err(Box::from(MyError {
                msg: String::from(i18n("Reading blocks is cancelled!")),
            }));
        }
        let internal_block = get_internal_block(block)?;
        let pos = internal_block.get_list_with_err("pos")?;
        let block_pos = get_size(pos)?;
        if i == 0 {
            min_x = block_pos.0;
            min_y = block_pos.1;
            min_z = block_pos.2;
        } else {
            min_x = min_x.min(block_pos.0);
            min_y = min_y.min(block_pos.0);
            min_z = min_z.min(block_pos.0);
        }
        i += 1;
    }
    let block_num = i;
    let mut j = 0;
    let mut start = Instant::now();
    for block in block_compound {
        let internal_block = get_internal_block(block)?;

        show_progress_macro!(
            &mut start,
            &mut sys,
            progress_fn,
            main_klass,
            (j * 100 / block_num) as c_int,
            elapsed_millisecs,
            free_memory,
            &formatx!(
                gettext_text(i18n("Processing blocks: {} / {}.")),
                j,
                block_num
            )?,
            cancel_flag,
            i18n("Reading blocks is cancelled!")
        );

        let pos = internal_block.get_list_with_err("pos")?;
        let block_pos = get_size(pos)?;
        let block_x = block_pos.0 - min_x;
        let block_y = block_pos.1 - min_y;
        let block_z = block_pos.2 - min_z;
        let index = x * z * block_y + x * block_z + block_x;
        let mut state = internal_block.get_int_with_err("state")?;
        if state == air_palette as i32 {
            state = 0;
        } else if state == 0 {
            state = air_palette as i32;
        }
        match internal_block.get_compound("nbt") {
            None => (),
            Some(c) => {
                let block_entity = convert_nbt_to_vec(c);
                let pos = (block_x, block_y, block_z);
                let real_entity = BlockEntity {
                    pos,
                    entity: block_entity,
                    index: j,
                };
                block_entities.push(real_entity);
            }
        };
        blocks[index as usize] = state as u32;

        j += 1;
    }
    let entities = region_get_entity_internal(&nbt, cancel_flag)?;

    let mut region = unsafe { Box::from_raw(region_new()) };
    region.data_version = data_version as u32;
    region.region_size = (x, y, z);
    region.palette_array = palette_vec;
    region.block_entity_array = block_entities;
    region.block_array = blocks;
    region.entity_array = entities;
    region.sort_block_entity_array();
    Ok(Box::into_raw(region))
}

#[unsafe(no_mangle)]
pub extern "C" fn region_create_from_file(
    nbt: *mut Nbt,
    progress_fn: ProgressFn,
    region: *mut *mut Region,
    main_klass: *mut c_void,
    cancel_flag: *const AtomicBool,
    elapsed_millisecs: u64,
    free_memory: u64,
) -> *const c_char {
    let mut err_string: String = String::new();
    if !region.is_null() {
        unsafe {
            *region = match region_create_from_bytes_internal(
                nbt,
                progress_fn,
                main_klass,
                cancel_flag,
                elapsed_millisecs as u128,
                free_memory,
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
