use crab_nbt::NbtTag;
use crab_nbt_ext::{
    GetWithError, MyError, ProgressFn, TreeValue, convert_nbt_to_hashmap, i18n,
    init_translation_internal, nbt_create_real, string_to_ptr_fail_to_null,
};
use std::collections::HashMap;
use std::error::Error;
use std::ffi::{c_char, c_int, c_void};
use std::ops::IndexMut;
use std::ptr::{null, null_mut};
use std::string::String;
use std::sync::atomic::AtomicBool;

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
}

#[unsafe(no_mangle)]
pub extern "C" fn region_type() -> *const c_char {
    string_to_ptr_fail_to_null("nbt")
}

#[unsafe(no_mangle)]
pub extern "C" fn region_is_multi() -> i32 {
    0
}

/* If it's litematic, please add:
 * region_num
 * region_name_index
 * region_crate_from_file_index
 */

struct Palette {
    id_name: String,
    property: Vec<(String, String)>,
}

#[derive(Default, Clone)]
struct Block {
    id: u32,
    block_entity: Option<HashMap<String, TreeValue>>,
}

fn get_int_from_nbt_tag(nbt: &NbtTag) -> Result<i32, MyError> {
    match nbt {
        NbtTag::Int(x) => Ok(*x),
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

fn region_create_from_bytes_internal(
    bytes: *mut Vec<u8>,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    cancel_flag: *const AtomicBool,
) -> Result<*mut c_void, Box<dyn Error>> {
    let nbt = nbt_create_real(bytes, progress_fn, main_klass)?;
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
    let mut palette_vec: Vec<Palette> = vec![];
    for palette in palette_list {
        if unsafe { cancel_flag_is_cancelled(cancel_flag) } == 1 {
            return Err(Box::from(MyError {
                msg: String::from(i18n("Reading palette is cancelled!")),
            }));
        }
        let internal_compound = match palette {
            NbtTag::Compound(c) => c,
            _ => {
                return Err(Box::from(MyError {
                    msg: String::from(i18n("Wrong type of palette!")),
                }));
            }
        };
        let internal_string = internal_compound.get_string_with_err("Name")?;
        let internal_properties: Vec<(String, String)> =
            match internal_compound.get_compound("Properties") {
                Some(properties) => {
                    let child = &properties.child_tags;
                    let mut ret = vec![];
                    for (name, data) in child {
                        let real_data = match data {
                            NbtTag::String(x) => x,
                            _ => {
                                return Err(Box::from(MyError {
                                    msg: String::from(i18n("Wrong type of property!")),
                                }));
                            }
                        };
                        ret.push((name.clone(), real_data.clone()));
                    }
                    ret
                }
                None => vec![],
            };
        palette_vec.push(Palette {
            id_name: internal_string.clone(),
            property: internal_properties,
        });
    }
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
    let mut blocks = vec![Block::default(); (x * y * z) as usize];

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
            i += 1;
        } else {
            min_x = min_x.min(block_pos.0);
            min_y = min_y.min(block_pos.0);
            min_z = min_z.min(block_pos.0);
        }
    }

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
        let tree_nbt = match internal_block.get_compound("nbt") {
            None => None,
            Some(c) => Some(convert_nbt_to_hashmap(c)),
        };
        if index > blocks.len() as i32 {
            return Err(Box::from(MyError {
                msg: String::from(i18n("Block out of range!")),
            }));
        }
        blocks.index_mut(index as usize).id = state as u32;
        blocks.index_mut(index as usize).block_entity = tree_nbt;
    }
    unsafe {
        let region = region_new();
        region_set_data_version(region, data_version as u32);
        region_set_size(region, x, y, z);
        for palette in palette_vec {
            if { cancel_flag_is_cancelled(cancel_flag) } == 1 {
                return Err(Box::from(MyError {
                    msg: String::from(i18n("Adding palette is cancelled!")),
                }));
            }
            let string = string_to_ptr_fail_to_null(&palette.id_name);
            region_append_palette(region, string, Box::into_raw(Box::new(palette.property)));
            string_free(string);
        }
        i = 0;
        for block in blocks {
            if { cancel_flag_is_cancelled(cancel_flag) } == 1 {
                return Err(Box::from(MyError {
                    msg: String::from(i18n("Adding block is cancelled!")),
                }));
            }
            let entities = match block.block_entity {
                Some(e) => Box::into_raw(Box::new(e)),
                None => null_mut(),
            };
            block_new_to_region(region, i, block.id, entities);
            i += 1;
        }
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
    bytes: *mut Vec<u8>,
    progress_fn: ProgressFn,
    region: *mut *mut c_void,
    main_klass: *mut c_void,
    cancel_flag: *const AtomicBool,
) -> *const c_char {
    let mut err_string: String = String::new();
    if !region.is_null() {
        unsafe {
            *region =
                match region_create_from_bytes_internal(bytes, progress_fn, main_klass, cancel_flag)
                {
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
