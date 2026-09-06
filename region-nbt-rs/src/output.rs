use crate::config::OutputConfig;
use crate::get_size_double;
use common_rs::helper_struct::HelperStruct;
use common_rs::i18n::i18n;
use common_rs::my_error::MyError;
use common_rs::region::Region;
use common_rs::util::{string_free, string_to_ptr_fail_to_null, vec_try_compress_real};
use common_rs::{ProgressFn, show_progress_macro};
use formatx::formatx;
use std::collections::HashMap;
use std::error::Error;
use std::ffi::{CStr, CString, c_char, c_int, c_void};
use std::fs::File;
use std::hash::Hash;
use std::io::Write;
use std::ptr::null;
use std::sync::atomic::AtomicBool;
use std::time::Instant;
use sysinfo::System;
use zuri_nbt::encoding::BigEndian;
use zuri_nbt::tag::{Compound, Double, Int, List};
use zuri_nbt::{NBTRoot, NBTTag};
use zurinbt_common::{get_type_from_tag, gettext_text};

trait NbtCreate {
    fn create_size(x: i32, y: i32, z: i32) -> Self;
    fn create_entities(entities: &Vec<Compound>) -> Result<Self, Box<dyn Error>>
    where
        Self: Sized;
    fn create_data_version(data_version: i32) -> Self;
    fn create_blocks(
        region: &Region,
        states: &Vec<u32>,
        ignore_air: bool,
        instant: &mut Instant,
        system: &mut System,
        helper_struct: &HelperStruct,
    ) -> Result<Self, Box<dyn Error>>
    where
        Self: Sized;
    fn create_palette(region: &Region) -> Result<Self, Box<dyn Error>>
    where
        Self: Sized;
}

impl NbtCreate for NBTTag {
    fn create_size(x: i32, y: i32, z: i32) -> Self {
        let x_pos = NBTTag::Int(x.into());
        let y_pos = NBTTag::Int(y.into());
        let z_pos = NBTTag::Int(z.into());
        let size_vec = vec![x_pos, y_pos, z_pos];
        NBTTag::List(size_vec.into())
    }

    fn create_entities(entities: &Vec<Compound>) -> Result<Self, Box<dyn Error>> {
        let mut vec = vec![];
        for entity in entities {
            let mut pos_x = 0.0;
            let mut pos_y = 0.0;
            let mut pos_z = 0.0;
            for (str, tag) in &entity.0 {
                if str == "Pos" {
                    let real_tag = get_type_from_tag!(tag, List, "Pos");
                    let size = get_size_double(real_tag)?;
                    pos_x = size.0;
                    pos_y = size.1;
                    pos_z = size.2;
                }
            }
            let block_pos_x = NBTTag::Int(Int(pos_x as i32));
            let block_pos_y = NBTTag::Int(Int(pos_y as i32));
            let block_pos_z = NBTTag::Int(Int(pos_z as i32));
            let block_pos = NBTTag::List(List(vec![block_pos_x, block_pos_y, block_pos_z]));
            let pos = NBTTag::List(List(vec![
                NBTTag::Double(pos_x.into()),
                NBTTag::Double(pos_y.into()),
                NBTTag::Double(pos_z.into()),
            ]));
            let tag = NBTTag::Compound(entity.clone());
            let mut hashmap = HashMap::new();
            hashmap.insert("nbt".to_string(), tag);
            hashmap.insert("blockPos".to_string(), block_pos);
            hashmap.insert("pos".to_string(), pos);
            let real_tag = NBTTag::Compound(Compound(hashmap));
            vec.push(real_tag);
        }
        Ok(NBTTag::List(List(vec)))
    }

    fn create_data_version(data_version: i32) -> Self {
        NBTTag::Int(data_version.into())
    }

    fn create_blocks(
        region: &Region,
        states: &Vec<u32>,
        ignore_air: bool,
        instant: &mut Instant,
        system: &mut System,
        helper_struct: &HelperStruct,
    ) -> Result<Self, Box<dyn Error>> {
        let region_x = region.region_size.0;
        let region_y = region.region_size.1;
        let region_z = region.region_size.2;
        let mut x = 0;
        let mut y = 0;
        let mut z = 0;
        fn size_change(
            x: &mut i32,
            y: &mut i32,
            z: &mut i32,
            region_x: i32,
            region_y: i32,
            region_z: i32,
        ) {
            if *x < region_x - 1 {
                *x += 1;
            } else if *z < region_z - 1 {
                *x = 0;
                *z += 1;
            } else if *y < region_y - 1 {
                *x = 0;
                *z = 0;
                *y += 1;
            }
        }
        let mut i = 0;
        let mut block_vec = vec![];
        for state in states {
            helper_struct.get_cancel_error(i18n("Adding blocks is cancelled."));
            if instant.elapsed().as_millis() >= helper_struct.elapsed_millisecs as u128 {
                helper_struct.instant_progress(
                    system,
                    instant,
                    (((i + 1) * 100) / states.len()) as c_int,
                    &formatx!(
                        gettext_text(i18n("Adding blocks to NBT: {} / {}.")),
                        i,
                        states.len()
                    )?,
                )?
            }

            let mut single_block_vec = HashMap::new();
            if ignore_air && *state == 0 {
                size_change(&mut x, &mut y, &mut z, region_x, region_y, region_z);
                i += 1;
                continue;
            }
            let pos = NBTTag::create_size(x, y, z);
            let state = NBTTag::Int((*state as i32).into());

            let nbt: Option<&Compound> = match region
                .block_entity_array
                .binary_search_by_key(&i, |entity| entity.index)
            {
                Ok(pos) => Some(&(region.block_entity_array[pos].entity)),
                Err(_) => None,
            };

            single_block_vec.insert("pos".to_string(), pos);
            single_block_vec.insert("state".to_string(), state);
            match nbt {
                Some(nbt) => {
                    single_block_vec.insert("nbt".to_string(), NBTTag::Compound(nbt.clone()));
                    ()
                }
                None => {}
            }
            let compound = NBTTag::Compound(Compound(single_block_vec));
            block_vec.push(compound);
            size_change(&mut x, &mut y, &mut z, region_x, region_y, region_z);
            i += 1;
        }
        Ok(NBTTag::List(block_vec.into()))
    }

    fn create_palette(region: &Region) -> Result<Self, Box<dyn Error>> {
        let len = region.palette_array.len();
        let mut i = 0;
        let mut palette_vec = vec![];
        while i < len {
            let mut real_palette_vec = HashMap::new();
            let mut property_vec = HashMap::new();
            let palette = &region.palette_array[i];
            let property_len = palette.property.len();
            let name = palette.id_name.clone();
            let name_tag = NBTTag::String(name.into());

            let mut j = 0;
            while j < property_len {
                let property_name = palette.property[j].0.clone();
                let property_data = palette.property[j].1.clone();
                let tag = NBTTag::String(property_data.into());
                property_vec.insert(property_name, tag);
                j += 1;
            }
            let mut property_tag: Option<NBTTag> = None;
            if property_len > 0 {
                property_tag = Some(NBTTag::Compound(property_vec.into()));
            }
            real_palette_vec.insert("Name".to_string(), name_tag);
            if property_tag.is_some() {
                real_palette_vec.insert("Properties".to_string(), property_tag.unwrap());
            }
            palette_vec.push(NBTTag::Compound(real_palette_vec.into()));
            i += 1;
        }
        Ok(NBTTag::List(palette_vec.into()))
    }
}

fn cstring_to_str(string: *mut c_char) -> Result<String, MyError> {
    unsafe {
        match CString::from_raw(string).into_string() {
            Ok(str) => Ok(str),
            Err(e) => {
                string_free(string);
                Err(MyError::new(e.to_string()))
            }
        }
    }
}

fn region_save_internal(
    region: *mut Region,
    filename: *const c_char,
    output_config: *mut OutputConfig,
    helper_struct: &HelperStruct,
) -> Result<(), Box<dyn Error>> {
    if region.is_null() || filename.is_null() {
        return Err(Box::new(MyError {
            msg: i18n("Nullptr detected!").to_string(),
        }));
    }
    let real_region = unsafe { &*region };
    let entity_vec = real_region.entity_array.clone();

    let region_x = real_region.region_size.0;
    let region_y = real_region.region_size.1;
    let region_z = real_region.region_size.2;
    let entity_nbt = NBTTag::create_entities(&entity_vec)?;
    let size_nbt = NBTTag::create_size(region_x, region_y, region_z);
    let mut id = vec![];
    let mut i = 0;
    let size = region_x * region_y * region_z;
    let mut start = Instant::now();
    let mut sys = System::new_all();
    while i < size {
        helper_struct.progress(
            &mut sys,
            &mut start,
            (((i + 1) as usize * 100) / size as usize) as c_int,
            i18n("Pushing index."),
            i18n("Pushing index is cancelled!"),
        )?;

        id.push(real_region.block_array[i as usize]);
        i += 1;
    }
    let ignore_air;
    if !output_config.is_null() {
        ignore_air = unsafe { (*output_config).ignore_air };
    } else {
        ignore_air = false;
    }
    let blocks_nbt = NBTTag::create_blocks(
        real_region,
        &id,
        ignore_air,
        &mut start,
        &mut sys,
        helper_struct,
    )?;
    let palette_nbt = NBTTag::create_palette(real_region)?;
    let data_version_nbt = NBTTag::create_data_version(real_region.data_version as i32);
    let mut compound_vec = HashMap::new();
    compound_vec.insert("size".to_string(), size_nbt);
    compound_vec.insert("entities".to_string(), entity_nbt);
    compound_vec.insert("blocks".to_string(), blocks_nbt);
    compound_vec.insert("palette".to_string(), palette_nbt);
    compound_vec.insert("DataVersion".to_string(), data_version_nbt);
    let nbt = NBTRoot {
        tag_name: "".to_string(),
        data: NBTTag::Compound(compound_vec.into()),
    };
    let mut buf = vec![];
    let be = BigEndian::default();
    nbt.write(&mut buf, be)?;
    let ret = vec_try_compress_real(Box::into_raw(Box::new(buf)), helper_struct, false)?;

    let file = File::create(unsafe { CStr::from_ptr(filename) }.to_str()?);
    file?.write_all(&ret)?;
    Ok(())
}

#[unsafe(no_mangle)]
pub extern "C" fn region_save(
    region: *mut Region,
    filename: *const c_char,
    output_config: *mut OutputConfig,
    helper_struct: *mut HelperStruct,
) -> *const c_char {
    match region_save_internal(region, filename, output_config, unsafe { &*helper_struct }) {
        Ok(()) => null(),
        Err(e) => string_to_ptr_fail_to_null(&e.to_string()),
    }
}
