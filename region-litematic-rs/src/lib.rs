use common_rs::helper_struct::HelperStruct;
use common_rs::i18n::i18n;
use common_rs::my_error::MyError;
use common_rs::region::{BlockEntity, Palette, Region};
use common_rs::util::show_progress;
use common_rs::util::string_to_ptr_fail_to_null;
use formatx::formatx;
use std::error::Error;
use std::ffi::{c_char, c_int};
use std::ops::{Shl, Shr};
use std::ptr::{null, null_mut};
use std::time::Instant;
use sysinfo::System;
use zuri_nbt::{NBTRoot, NBTTag};
use zurinbt_common::{
    get_compound_value_err_return, get_palette_from_nbt_tag, get_type_from_compound,
    get_type_from_tag, gettext_text,
};

#[link(name = "region_rs")]
unsafe extern "C" {
    fn region_new() -> *mut Region;
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
pub extern "C" fn region_file_suffix() -> *const c_char {
    string_to_ptr_fail_to_null("litematic")
}

#[unsafe(no_mangle)]
pub extern "C" fn region_base_type() -> *const c_char {
    string_to_ptr_fail_to_null("JavaNBT")
}

#[unsafe(no_mangle)]
pub extern "C" fn region_file_type() -> *const c_char {
    string_to_ptr_fail_to_null(&gettext_text(i18n("Litematic File (*.litematic)")))
}

#[unsafe(no_mangle)]
pub extern "C" fn region_num(region: *mut NBTRoot) -> i32 {
    if !region.is_null() {
        let real_nbt = unsafe { &*region };
        let region_nbt = real_nbt.data.view().at("Regions").compound();
        let ret;
        match region_nbt {
            Ok(r) => ret = r.len() as i32,
            Err(_) => ret = 0,
        }
        ret
    } else {
        0
    }
}

fn region_name_index_real(region: *mut NBTRoot, index: i32) -> Result<String, Box<dyn Error>> {
    let real_nbt = unsafe { &*region };
    let region_name = real_nbt
        .data
        .view()
        .compound()?
        .get_key_value("Regions")
        .unwrap()
        .1;
    Ok(region_name
        .view()
        .compound()?
        .keys()
        .collect::<Vec<&String>>()[index as usize]
        .clone())
}

#[unsafe(no_mangle)]
pub extern "C" fn region_name_index(region: *mut NBTRoot, index: i32) -> *const c_char {
    match region_name_index_real(region, index) {
        Ok(key) => string_to_ptr_fail_to_null(&key),
        _ => null(),
    }
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
    helper_struct: &HelperStruct,
    sys: &mut System,
    instant: &mut Instant,
) -> Result<Vec<u32>, Box<dyn Error>> {
    let mut i = 0;
    let mut buf = vec![0; block_num as usize];
    loop {
        helper_struct.progress(
            sys,
            instant,
            (i as u64 * 100 / block_num as u64) as c_int,
            &formatx!(
                gettext_text(i18n("Reading block id: {} / {}.")),
                i,
                block_num
            )?,
            i18n("Cancelled when reading blocks"),
        )?;

        let start_bit = i as u32 * move_bit;
        let start_state = start_bit / 64;
        let and_num = (1 << move_bit) - 1;
        let move_num = start_bit & 63;
        let end_num = move_num + move_bit;

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
        buf[i] = id as u32;
        i += 1;
        if i == block_num as usize {
            show_progress(
                helper_struct.progress_fn,
                helper_struct.main_klass,
                100,
                i18n("Reading block finished!"),
                "",
            );
            break;
        }
    }
    Ok(buf)
}

fn region_create_from_bytes_internal(
    o_nbt: *mut NBTRoot,
    index: i32,
    helper_struct: &HelperStruct,
) -> Result<*mut Region, Box<dyn Error>> {
    let nbt = &unsafe { &*o_nbt }.data;
    let data_version = nbt.view().at("MinecraftDataVersion").int()?;
    let metadata = nbt.view().at("Metadata").compound()?;
    let create_time = get_compound_value_err_return!(metadata, "TimeCreated")
        .view()
        .long()?;
    let modify_time = get_compound_value_err_return!(metadata, "TimeModified")
        .view()
        .long()?;
    let description = get_compound_value_err_return!(metadata, "Description")
        .view()
        .string()?;
    let author = get_compound_value_err_return!(metadata, "Author")
        .view()
        .string()?;
    let name = get_compound_value_err_return!(metadata, "Name")
        .view()
        .string()?;

    let region_parent_nbt = nbt.view().at("Regions").compound()?;
    let region_real_vec = region_parent_nbt
        .iter()
        .collect::<Vec<(&String, &NBTTag)>>();
    let region_name = region_real_vec[index as usize].0;
    let region_nbt = region_real_vec[index as usize].1;
    let real_region_nbt = match region_nbt {
        NBTTag::Compound(c) => c,
        _ => {
            return Err(Box::new(MyError {
                msg: i18n("Wrong type of region.").to_string(),
            }));
        }
    };

    let size_nbt = get_compound_value_err_return!(real_region_nbt, "Size")
        .view()
        .compound()?;
    let region_x = get_compound_value_err_return!(size_nbt, "x")
        .view()
        .int()?
        .abs();
    let region_y = get_compound_value_err_return!(size_nbt, "y")
        .view()
        .int()?
        .abs();
    let region_z = get_compound_value_err_return!(size_nbt, "z")
        .view()
        .int()?
        .abs();

    let offset_nbt = get_compound_value_err_return!(real_region_nbt, "Position")
        .view()
        .compound()?;
    let offset_x = get_compound_value_err_return!(offset_nbt, "x")
        .view()
        .int()?;
    let offset_y = get_compound_value_err_return!(offset_nbt, "y")
        .view()
        .int()?;
    let offset_z = get_compound_value_err_return!(offset_nbt, "z")
        .view()
        .int()?;

    let block_states = get_type_from_compound!(real_region_nbt, "BlockStates", LongArray);

    let palette_list = get_type_from_compound!(real_region_nbt, "BlockStatePalette", List);

    let mut sys = System::new_all();
    let mut instant = Instant::now();

    let palette_vec: Vec<Palette> =
        get_palette_from_nbt_tag(palette_list, helper_struct, &mut sys, &mut instant)?;

    let block_num = region_x * region_y * region_z;
    let palette_num = palette_vec.len();
    let move_bit = get_bits(palette_num - 1);
    let real_move_bit = if move_bit <= 2 { 2 } else { move_bit };

    let block_ids = get_block_id(
        block_states,
        block_num,
        real_move_bit,
        helper_struct,
        &mut sys,
        &mut instant,
    )?;
    let blocks = block_ids;
    let mut tile_entities_vec = vec![];
    let tile_entities_list = &get_type_from_compound!(real_region_nbt, "TileEntities", List).0;

    for tile_entity in tile_entities_list {
        let real_tile_entity = get_type_from_tag!(tile_entity, Compound, "Tile Entity");
        let entity_x = get_compound_value_err_return!(real_tile_entity, "x")
            .view()
            .int()?;
        let entity_y = get_compound_value_err_return!(real_tile_entity, "y")
            .view()
            .int()?;
        let entity_z = get_compound_value_err_return!(real_tile_entity, "z")
            .view()
            .int()?;
        let entity_index = region_x * region_z * entity_y + region_x * entity_z + entity_x;
        let entity_id = blocks[entity_index as usize];
        let id = real_tile_entity.get_key_value("id");
        let mut has_id: bool = true;
        let real_id = match id {
            Some(s) => s.1.view().string()?,
            None => {
                has_id = false;
                &palette_vec[entity_id as usize].id_name
            }
        };
        let mut clone_real_tile_entity = real_tile_entity.clone();
        clone_real_tile_entity.0.remove("x");
        clone_real_tile_entity.0.remove("y");
        clone_real_tile_entity.0.remove("z");

        if !has_id {
            clone_real_tile_entity.insert(
                "id".to_string(),
                NBTTag::String(zuri_nbt::tag::String(real_id.to_string())),
            );
        }
        let block_entity = BlockEntity {
            pos: (entity_x, entity_y, entity_z),
            entity: clone_real_tile_entity,
            index: entity_index as usize,
        };
        tile_entities_vec.push(block_entity);
    }
    let mut entities_vec = vec![];
    let entities_list = &get_type_from_compound!(real_region_nbt, "Entities", List).0;
    for entity in entities_list {
        let internal_entity = get_type_from_tag!(entity, Compound, "internal entity").clone();
        entities_vec.push(internal_entity);
    }

    let mut region = unsafe { Box::from_raw(region_new()) };
    region.data_version = data_version as u32;
    region.region_size = (region_x, region_y, region_z);
    region.region_offset = (offset_x, offset_y, offset_z);
    region.palette_array = palette_vec;
    region.block_array = blocks;
    region.base_data.set_name(name);
    region.base_data.set_description(description);
    region.base_data.set_author(author);
    region.base_data.set_region_name(region_name);
    region.entity_array = entities_vec;
    region.block_entity_array = tile_entities_vec;
    region.sort_block_entity_array();
    region.set_data_time(create_time, modify_time)?;

    Ok(Box::into_raw(region))
}

#[unsafe(no_mangle)]
pub extern "C" fn region_create_from_file_as_index(
    nbt: *mut NBTRoot,
    region: *mut *mut Region,
    index: i32,
    helper_struct: *mut HelperStruct,
) -> *const c_char {
    let mut err_string: String = String::new();
    if !region.is_null() {
        unsafe {
            *region = match region_create_from_bytes_internal(nbt, index, &*helper_struct) {
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
