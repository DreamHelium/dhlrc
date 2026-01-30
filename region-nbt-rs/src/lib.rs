use crab_nbt::{Nbt, NbtCompound, NbtTag};
use crab_nbt_ext::{
    GetWithError, MyError, Palette, ProgressFn, TreeValue, convert_nbt_to_vec, convert_vec_to_nbt,
    get_palette_from_nbt_tag, i18n, init_translation_internal, nbt_create_real, show_progress,
    string_to_ptr_fail_to_null,
};
use formatx::formatx;
use std::collections::HashMap;
use std::error::Error;
use std::ffi::{CStr, c_char, c_int, c_void};
use std::fs::File;
use std::io::Write;
use std::ops::IndexMut;
use std::ptr;
use std::ptr::{null, null_mut};
use std::string::String;
use std::sync::atomic::AtomicBool;
use std::time::Instant;

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
    fn region_set_block_entities_from_vec(
        region: *mut c_void,
        block_entities: *mut Vec<BlockEntity>,
    );
    fn region_set_blocks_from_vec(region: *mut c_void, blocks: *mut Vec<u32>);
    fn region_set_entity_from_vec(
        region: *mut c_void,
        entities: *mut Vec<Vec<(String, TreeValue)>>,
    );
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
    ) -> *mut Vec<u8>;
}

trait NbtCreate {
    fn create_size(x: i32, y: i32, z: i32) -> Self;
    fn create_entities(entities: &Vec<Vec<(String, TreeValue)>>) -> Result<Self, MyError>
    where
        Self: Sized;
    fn create_data_version(data_version: i32) -> Self;
    fn create_blocks(region: *mut c_void, states: &Vec<u32>, ignore_air: bool) -> Self;
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

    fn create_blocks(region: *mut c_void, states: &Vec<u32>, ignore_air: bool) -> Self {
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
        NbtTag::List(block_vec)
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
            let real_name = match unsafe { CStr::from_ptr(name) }.to_str() {
                Err(e) => {
                    unsafe {
                        string_free(name as *mut c_char);
                    }
                    return Err(Box::from(e));
                }
                Ok(str) => str.to_string(),
            };
            let name_tag = NbtTag::String(real_name);
            unsafe {
                string_free(name as *mut c_char);
            }

            let mut j = 0;
            while j < property_len {
                let property_name = unsafe { region_get_palette_property_name(region, i, j) };
                let property_data = unsafe { region_get_palette_property_data(region, i, j) };
                let real_property_name = match unsafe { CStr::from_ptr(property_name) }.to_str() {
                    Err(e) => {
                        unsafe {
                            string_free(property_name as *mut c_char);
                        }
                        return Err(Box::from(e));
                    }
                    Ok(str) => str.to_string(),
                };
                let real_property_data = match unsafe { CStr::from_ptr(property_data) }.to_str() {
                    Err(e) => {
                        unsafe {
                            string_free(property_data as *mut c_char);
                        }
                        return Err(Box::from(e));
                    }
                    Ok(str) => str.to_string(),
                };
                let tag = NbtTag::String(real_property_data);
                property_vec.push((real_property_name, tag));
                unsafe {
                    string_free(property_name as *mut c_char);
                    string_free(property_data as *mut c_char);
                }
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

#[unsafe(no_mangle)]
pub extern "C" fn region_type() -> *const c_char {
    string_to_ptr_fail_to_null("nbt")
}

#[unsafe(no_mangle)]
pub extern "C" fn region_is_multi() -> i32 {
    0
}

pub struct BlockEntity {
    pos: (i32, i32, i32),
    entity: Vec<(String, TreeValue)>,
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

fn region_get_entity_internal(
    nbt: &crab_nbt::Nbt,
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

        let internal_entity = match entity {
            NbtTag::Compound(c) => c,
            _ => {
                return Err(Box::from(MyError {
                    msg: String::from(i18n("Wrong type of entity!")),
                }));
            }
        };
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
) -> Result<*mut c_void, Box<dyn Error>> {
    let nbt = unsafe { (*o_nbt).clone() };
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

    i = 0;
    for block in block_compound {
        if unsafe { cancel_flag_is_cancelled(cancel_flag) } == 1 {
            return Err(Box::from(MyError {
                msg: String::from(i18n("Reading blocks is cancelled!")),
            }));
        }
        let internal_block = match block {
            NbtTag::Compound(c) => c,
            _ => {
                return Err(Box::from(MyError {
                    msg: String::from(i18n("Wrong type of block!")),
                }));
            }
        };
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
        if unsafe { cancel_flag_is_cancelled(cancel_flag) } == 1 {
            return Err(Box::from(MyError {
                msg: String::from(i18n("Reading blocks is cancelled!")),
            }));
        }
        let internal_block = match block {
            NbtTag::Compound(c) => c,
            _ => {
                return Err(Box::from(MyError {
                    msg: String::from(i18n("Wrong type of block!")),
                }));
            }
        };
        let string = i18n("Processing blocks: {} / {}.");
        let real_string = formatx!(string, j, block_num)?;
        if start.elapsed().as_millis() >= 500 || j == block_num - 1 {
            show_progress(
                progress_fn,
                main_klass,
                (j * 100 / block_num) as c_int,
                &real_string,
                &String::new(),
            );
            start = Instant::now();
        }

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
                };
                block_entities.push(real_entity);
            }
        };
        blocks[index as usize] = state as u32;

        j += 1;
    }
    let entities = region_get_entity_internal(&nbt, cancel_flag)?;
    unsafe {
        let region = region_new();
        region_set_data_version(region, data_version as u32);
        region_set_size(region, x, y, z);
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
        region_set_block_entities_from_vec(region, Box::into_raw(Box::new(block_entities)));
        region_set_blocks_from_vec(region, Box::into_raw(Box::new(blocks)));
        region_set_entity_from_vec(region, Box::into_raw(Box::new(entities)));
        Ok(region)
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn init_translation(path: *const c_char) -> *const c_char {
    match init_translation_internal(path) {
        Ok(_) => null(),
        Err(e) => string_to_ptr_fail_to_null(&e.to_string()),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_create_from_file(
    nbt: *mut crab_nbt::Nbt,
    progress_fn: ProgressFn,
    region: *mut *mut c_void,
    main_klass: *mut c_void,
    cancel_flag: *const AtomicBool,
) -> *const c_char {
    let mut err_string: String = String::new();
    if !region.is_null() {
        unsafe {
            *region =
                match region_create_from_bytes_internal(nbt, progress_fn, main_klass, cancel_flag) {
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

fn region_save_internal(
    region: *mut c_void,
    filename: *const c_char,
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
    while i < region_x * region_y * region_z {
        unsafe { id.push(region_get_block_id_by_index(region, i as usize)) };
        i += 1;
    }
    let blocks_nbt = NbtTag::create_blocks(region, &id, false);
    let palette_nbt = NbtTag::create_palette(region);
    let data_version_nbt =
        unsafe { NbtTag::create_data_version(region_get_data_version(region) as i32) };
    let compound_vec = vec![
        ("size".to_string(), size_nbt),
        ("entities".to_string(), entity_nbt?),
        ("blocks".to_string(), blocks_nbt),
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
            None,
            null_mut(),
            &mut failed as *mut c_int,
            false,
            null(),
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
pub extern "C" fn region_save(region: *mut c_void, filename: *const c_char) -> *const c_char {
    match region_save_internal(region, filename) {
        Ok(()) => null(),
        Err(e) => string_to_ptr_fail_to_null(&e.to_string()),
    }
}
