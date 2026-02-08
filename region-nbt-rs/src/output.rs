use crate::config::OutputConfig;
use crate::{
    get_size_double, region_get_block_entity, region_get_block_id_by_index,
    region_get_data_version, region_get_entity, region_get_entity_len, region_get_palette_id_name,
    region_get_palette_len, region_get_palette_property_data, region_get_palette_property_len,
    region_get_palette_property_name, region_get_x, region_get_y, region_get_z, string_free,
    vec_try_compress,
};
use common_rs::ProgressFn;
use common_rs::i18n::i18n;
use common_rs::my_error::MyError;
use common_rs::tree_value::TreeValue;
use common_rs::util::{real_show_progress, string_to_ptr_fail_to_null};
use crab_nbt::{Nbt, NbtCompound, NbtTag};
use crab_nbt_ext::convert_vec_to_nbt;
use std::error::Error;
use std::ffi::{CStr, CString, c_char, c_int, c_void};
use std::fs::File;
use std::io::Write;
use std::ptr::{null, null_mut};
use std::sync::atomic::AtomicBool;
use std::time::Instant;
use formatx::formatx;
use sysinfo::System;

trait NbtCreate {
    fn create_size(x: i32, y: i32, z: i32) -> Self;
    fn create_entities(entities: &Vec<Vec<(String, TreeValue)>>) -> Result<Self, MyError>
    where
        Self: Sized;
    fn create_data_version(data_version: i32) -> Self;
    fn create_blocks(
        region: *mut c_void,
        states: &Vec<u32>,
        ignore_air: bool,
        instant: &mut Instant,
        system: &mut System,
        progress_fn: ProgressFn,
        main_klass: *mut c_void,
    ) -> Result<Self, Box<dyn Error>>
    where
        Self: Sized;
    fn create_palette(region: *mut c_void) -> Result<Self, Box<dyn Error>>
    where
        Self: Sized;
}

impl NbtCreate for NbtTag {
    fn create_size(x: i32, y: i32, z: i32) -> Self {
        let x_pos = NbtTag::Int(x);
        let y_pos = NbtTag::Int(y);
        let z_pos = NbtTag::Int(z);
        let size_vec = vec![x_pos, y_pos, z_pos];
        NbtTag::List(size_vec)
    }

    fn create_entities(entities: &Vec<Vec<(String, TreeValue)>>) -> Result<Self, MyError> {
        let mut vec = vec![];
        for entity in entities {
            let nbt = convert_vec_to_nbt(entity, "", false).root_tag;
            let child_nbt = &nbt.child_tags;
            let mut pos_x = 0.0;
            let mut pos_y = 0.0;
            let mut pos_z = 0.0;
            for (str, tag) in child_nbt {
                if str == "Pos" {
                    let real_tag = match tag {
                        NbtTag::List(l) => l,
                        _ => {
                            return Err(MyError {
                                msg: i18n("Nullptr detected!").to_string(),
                            });
                        }
                    };
                    let size = get_size_double(real_tag)?;
                    pos_x = size.0;
                    pos_y = size.1;
                    pos_z = size.2;
                }
            }
            let block_pos_x = NbtTag::Int(pos_x as i32);
            let block_pos_y = NbtTag::Int(pos_y as i32);
            let block_pos_z = NbtTag::Int(pos_z as i32);
            let block_pos = NbtTag::List(vec![block_pos_x, block_pos_y, block_pos_z]);
            let pos = NbtTag::List(vec![
                NbtTag::Double(pos_x),
                NbtTag::Double(pos_y),
                NbtTag::Double(pos_z),
            ]);
            let tag = NbtTag::Compound(nbt);
            let real_tag = NbtTag::Compound(NbtCompound {
                child_tags: vec![
                    ("nbt".to_string(), tag),
                    ("blockPos".to_string(), block_pos),
                    ("pos".to_string(), pos),
                ],
            });
            vec.push(real_tag);
        }
        Ok(NbtTag::List(vec))
    }

    fn create_data_version(data_version: i32) -> Self {
        NbtTag::Int(data_version)
    }

    fn create_blocks(
        region: *mut c_void,
        states: &Vec<u32>,
        ignore_air: bool,
        instant: &mut Instant,
        system: &mut System,
        progress_fn: ProgressFn,
        main_klass: *mut c_void,
    ) -> Result<Self, Box<dyn Error>> {
        let region_x = unsafe { region_get_x(region) };
        let region_y = unsafe { region_get_y(region) };
        let region_z = unsafe { region_get_z(region) };
        let mut x = 0;
        let mut y = 0;
        let mut z = 0;
        fn size_change(
            mut x: &mut i32,
            mut y: &mut i32,
            mut z: &mut i32,
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
            let string = i18n("Adding blocks to NBT: {} / {}.");
            let real_string = formatx!(string, i, states.len())?;
            real_show_progress(
                instant,
                system,
                progress_fn,
                main_klass,
                (((i + 1) as usize * 100) / states.len()) as c_int,
                &real_string,
                "",
            )?;
            let mut single_block_vec = vec![];
            if ignore_air && *state == 0 {
                size_change(&mut x, &mut y, &mut z, region_x, region_y, region_z);
                i += 1;
                continue;
            }
            let pos = NbtTag::create_size(x, y, z);
            let state = NbtTag::Int(*state as i32);
            let nbt = unsafe { region_get_block_entity(region, i) };
            let mut nbt_nbt: Option<NbtTag> = None;

            if !nbt.is_null() {
                let real_nbt = convert_vec_to_nbt(unsafe { &*nbt }, "", false).root_tag;
                nbt_nbt = Some(NbtTag::Compound(real_nbt));
            }
            single_block_vec.push(("pos".to_string(), pos));
            single_block_vec.push(("state".to_string(), state));
            match nbt_nbt {
                Some(nbt) => single_block_vec.push(("nbt".to_string(), nbt)),
                None => {}
            }
            let compound = NbtTag::Compound(NbtCompound {
                child_tags: single_block_vec,
            });
            block_vec.push(compound);
            size_change(&mut x, &mut y, &mut z, region_x, region_y, region_z);
            i += 1;
        }
        Ok(NbtTag::List(block_vec))
    }

    fn create_palette(region: *mut c_void) -> Result<Self, Box<dyn Error>> {
        let len = unsafe { region_get_palette_len(region) };
        let mut i = 0;
        let mut palette_vec = vec![];
        while i < len {
            let mut real_palette_vec = vec![];
            let mut property_vec = vec![];
            let property_len = unsafe { region_get_palette_property_len(region, i) };
            let name = unsafe { region_get_palette_id_name(region, i) };
            let real_name = cstring_to_str(name as *mut c_char)?;
            let name_tag = NbtTag::String(real_name);

            let mut j = 0;
            while j < property_len {
                let property_name = unsafe { region_get_palette_property_name(region, i, j) };
                let property_data = unsafe { region_get_palette_property_data(region, i, j) };
                let real_property_name = cstring_to_str(property_name as *mut c_char)?;
                let real_property_data = cstring_to_str(property_data as *mut c_char)?;
                let tag = NbtTag::String(real_property_data);
                property_vec.push((real_property_name, tag));
                j += 1;
            }
            let mut property_tag: Option<NbtTag> = None;
            if property_len > 0 {
                property_tag = Some(NbtTag::Compound(NbtCompound {
                    child_tags: property_vec,
                }));
            }
            real_palette_vec.push(("Name".to_string(), name_tag));
            if property_tag.is_some() {
                real_palette_vec.push(("Properties".to_string(), property_tag.unwrap()));
            }
            palette_vec.push(NbtTag::Compound(NbtCompound {
                child_tags: real_palette_vec,
            }));
            i += 1;
        }
        Ok(NbtTag::List(palette_vec))
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
    region: *mut c_void,
    filename: *const c_char,
    output_config: *mut OutputConfig,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    cancel_flag: *const c_void,
) -> Result<(), Box<dyn Error>> {
    if region.is_null() || filename.is_null() {
        return Err(Box::new(MyError {
            msg: i18n("Nullptr detected!").to_string(),
        }));
    }
    let mut i = 0;
    let mut entity_vec = vec![];
    while i < unsafe { region_get_entity_len(region) } {
        let entity = unsafe { region_get_entity(region, i) };
        let real_entity = unsafe { (&*entity).clone() };
        entity_vec.push(real_entity);
        i += 1;
    }
    let region_x = unsafe { region_get_x(region) };
    let region_y = unsafe { region_get_y(region) };
    let region_z = unsafe { region_get_z(region) };
    let entity_nbt = NbtTag::create_entities(&entity_vec);
    let size_nbt = NbtTag::create_size(region_x, region_y, region_z);
    let mut id = vec![];
    let mut i = 0;
    let size = region_x * region_y * region_z;
    let mut start = Instant::now();
    let mut sys = System::new_all();
    while i < size {
        real_show_progress(
            &mut start,
            &mut sys,
            progress_fn,
            main_klass,
            (((i + 1) as usize * 100) / size as usize) as c_int,
            i18n("Pushing index."),
            "",
        )?;
        unsafe { id.push(region_get_block_id_by_index(region, i as usize)) };
        i += 1;
    }
    let ignore_air;
    if !output_config.is_null() {
        ignore_air = unsafe { (*output_config).ignore_air };
    } else {
        ignore_air = false;
    }
    let blocks_nbt = NbtTag::create_blocks(
        region,
        &id,
        ignore_air,
        &mut start,
        &mut sys,
        progress_fn,
        main_klass,
    );
    let palette_nbt = NbtTag::create_palette(region);
    let data_version_nbt =
        unsafe { NbtTag::create_data_version(region_get_data_version(region) as i32) };
    let compound_vec = vec![
        ("size".to_string(), size_nbt),
        ("entities".to_string(), entity_nbt?),
        ("blocks".to_string(), blocks_nbt?),
        ("palette".to_string(), palette_nbt?),
        ("DataVersion".to_string(), data_version_nbt),
    ];
    let nbt = Nbt {
        name: "".to_string(),
        root_tag: NbtCompound {
            child_tags: compound_vec,
        },
    };

    let bytes = nbt.write().to_vec();
    let mut failed: c_int = 0;
    let ret = unsafe {
        vec_try_compress(
            Box::into_raw(Box::new(bytes)),
            progress_fn,
            main_klass,
            &mut failed as *mut c_int,
            false,
            cancel_flag as *const AtomicBool,
        )
    };
    let real_ret;
    if failed == 1 {
        return Err(Box::new(MyError {
            msg: unsafe { String::from_utf8(*Box::from_raw(ret))? },
        }));
    } else {
        real_ret = unsafe { Box::from_raw(ret) };
    }

    let file = File::create(unsafe { CStr::from_ptr(filename) }.to_str()?);
    file?.write_all(&real_ret)?;
    Ok(())
}

#[unsafe(no_mangle)]
pub extern "C" fn region_save(
    region: *mut c_void,
    filename: *const c_char,
    output_config: *mut OutputConfig,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    cancel_flag: *const c_void,
) -> *const c_char {
    match region_save_internal(
        region,
        filename,
        output_config,
        progress_fn,
        main_klass,
        cancel_flag,
    ) {
        Ok(()) => null(),
        Err(e) => string_to_ptr_fail_to_null(&e.to_string()),
    }
}
