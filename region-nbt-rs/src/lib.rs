mod config;
mod output;

use common_rs::helper_struct::HelperStruct;
use common_rs::i18n::i18n;
use common_rs::my_error::MyError;
use common_rs::region::{BlockEntity, Palette, Region};
use common_rs::util::string_to_ptr_fail_to_null;
use formatx::formatx;
use std::error::Error;
use std::ffi::{c_char, c_int};
use std::ptr::{null, null_mut};
use std::string::String;
use std::time::Instant;
use sysinfo::System;
use zuri_nbt::tag::Compound;
use zuri_nbt::{NBTRoot, NBTTag};
use zurinbt_common::{
    get_compound_value_err_return, get_palette_from_nbt_tag, get_type_from_compound,
    get_type_from_tag, gettext_text,
};

#[link(name = "region_rs")]
unsafe extern "C" {
    fn region_new() -> *mut Region;
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

fn get_size(nbt: &Vec<NBTTag>) -> Result<(i32, i32, i32), Box<dyn Error>> {
    if nbt.len() != 3 {
        return Err(Box::new(MyError {
            msg: i18n("The length of the size is wrong.").to_string(),
        }));
    }
    let x = nbt[0].view().int()?;
    let y = nbt[1].view().int()?;
    let z = nbt[2].view().int()?;
    Ok((x, y, z))
}

fn get_size_double(nbt: &Vec<NBTTag>) -> Result<(f64, f64, f64), Box<dyn Error>> {
    if nbt.len() != 3 {
        return Err(Box::new(MyError {
            msg: i18n("The length of the size is wrong.").to_string(),
        }));
    }
    let x = nbt[0].view().double()?;
    let y = nbt[1].view().double()?;
    let z = nbt[2].view().double()?;
    Ok((x, y, z))
}

fn region_get_entity_internal(
    nbt: &Compound,
    helper_struct: &HelperStruct,
) -> Result<Vec<Compound>, Box<dyn Error>> {
    let entity_nbt = &get_type_from_compound!(nbt, "entities", List).0;
    let mut ret = vec![];
    for entity in entity_nbt {
        helper_struct.get_cancel_error(i18n("Reading entities is cancelled!"))?;

        let internal_entity = get_type_from_tag!(entity, Compound, "Entity");

        /* Then we need to process the nbt */
        let mut real_nbt = get_type_from_compound!(internal_entity, "nbt", Compound).clone();
        let block_pos = get_type_from_compound!(internal_entity, "pos", List);
        let block_pos_value = get_size_double(block_pos)?;

        for (str, val) in &mut real_nbt.0 {
            if str == "Pos" {
                match val {
                    NBTTag::List(l) => {
                        fn modify_tree_value_double(
                            tree_value: &mut NBTTag,
                            value: f64,
                        ) -> Result<(), MyError> {
                            match tree_value {
                                NBTTag::Double(d) => d.0 = value,
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
        ret.push(real_nbt);
    }
    Ok(ret)
}

fn region_create_from_bytes_internal(
    o_nbt: *mut NBTRoot,
    helper_struct: &HelperStruct,
) -> Result<*mut Region, Box<dyn Error>> {
    let nbt = &unsafe { &*o_nbt }.data;
    let compound = nbt.view().compound()?;
    let data_version = nbt.view().at("DataVersion").int()?;

    let real_size_list = get_type_from_compound!(compound, "size", List);

    let region_size = get_size(real_size_list)?;
    let x = region_size.0;
    let y = region_size.1;
    let z = region_size.2;
    let palette_list = get_type_from_compound!(compound, "palette", List);
    let mut sys = System::new_all();
    let mut instant = Instant::now();
    let mut palette_vec: Vec<Palette> =
        get_palette_from_nbt_tag(palette_list, helper_struct, &mut sys, &mut instant)?;
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

    let block_compound = &get_type_from_compound!(compound, "blocks", List).0;
    let mut blocks: Vec<u32> = vec![0; (x * y * z) as usize];
    let mut block_entities: Vec<BlockEntity> = vec![];

    let mut min_x = 0;
    let mut min_y = 0;
    let mut min_z = 0;
    let mut sys = System::new_all();

    i = 0;

    for block in block_compound {
        helper_struct.progress(
            &mut sys,
            &mut instant,
            0,
            "Get Block Total.",
            i18n("Reading blocks is cancelled!"),
        )?;
        let internal_block = get_type_from_tag!(block, Compound, "block");
        let pos = get_type_from_compound!(internal_block, "pos", List);
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

    for block in block_compound {
        let internal_block = get_type_from_tag!(block, Compound, "block");
        helper_struct.get_cancel_error(i18n("Reading blocks is cancelled!"))?;
        if instant.elapsed().as_millis() >= helper_struct.elapsed_millisecs as u128 {
            helper_struct.instant_progress(
                &mut sys,
                &mut instant,
                (j * 100 / block_num) as c_int,
                &formatx!(
                    gettext_text(i18n("Processing blocks: {} / {}.")),
                    j,
                    block_num
                )?,
            )?
        }

        let pos = get_type_from_compound!(internal_block, "pos", List);
        let block_pos = get_size(pos)?;
        let block_x = block_pos.0 - min_x;
        let block_y = block_pos.1 - min_y;
        let block_z = block_pos.2 - min_z;
        let index = x * z * block_y + x * block_z + block_x;
        let mut state = get_compound_value_err_return!(internal_block, "state")
            .view()
            .int()?;
        if state == air_palette as i32 {
            state = 0;
        } else if state == 0 {
            state = air_palette as i32;
        }
        match internal_block.get_key_value("nbt") {
            None => (),
            Some(c) => {
                let block_entity = get_type_from_tag!(c.1, Compound, "BlockEntity");
                let pos = (block_x, block_y, block_z);
                let real_entity = BlockEntity {
                    pos,
                    entity: block_entity.clone(),
                    index: j,
                };
                block_entities.push(real_entity);
            }
        };
        blocks[index as usize] = state as u32;

        j += 1;
    }
    let entities = region_get_entity_internal(compound, helper_struct)?;

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
    nbt: *mut NBTRoot,
    region: *mut *mut Region,
    helper_struct: *mut HelperStruct,
) -> *const c_char {
    let mut err_string: String = String::new();
    if !region.is_null() {
        unsafe {
            *region = match region_create_from_bytes_internal(nbt, &*helper_struct) {
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
