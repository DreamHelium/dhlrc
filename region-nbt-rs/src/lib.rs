use crab_nbt::{Nbt, NbtCompound, NbtTag};
use std::collections::HashMap;
use std::error::Error;
use std::ffi::{CStr, CString, c_char, c_int, c_void};
use std::fmt::{Display, Formatter};
use std::io::Cursor;
use std::ops::IndexMut;
use std::ptr::{null, null_mut};
use std::string::String;

type ProgressFn = extern "C" fn(
    main_klass: *mut c_void,
    progress: c_int,
    format: *const c_char,
    text: *const c_char,
);

#[derive(Clone)]
pub enum TreeValue {
    Byte(i8),
    Short(i16),
    Int(i32),
    Long(i64),
    Float(f32),
    Double(f64),
    String(String),
    ByteArray(Vec<i8>),
    IntArray(Vec<i32>),
    LongArray(Vec<i64>),
    List(Vec<TreeValue>),
    Compound(HashMap<String, TreeValue>),
}

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
}

fn string_to_ptr_fail_to_null(string: &str) -> *mut c_char {
    let str = CString::new(string);
    match str {
        Ok(real_str) => real_str.into_raw(),
        Err(_err) => null_mut(),
    }
}

#[derive(Debug)]
struct MyError {
    msg: String,
}

impl Display for MyError {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.msg)
    }
}

impl Error for MyError {}

fn cstr_to_str(string: *const c_char) -> Result<String, Box<dyn Error>> {
    if string.is_null() {
        return Err(Box::from(MyError {
            /* NOTE: can be translated */
            msg: "Null pointer detected".to_string(),
        }));
    }
    let str = unsafe { CStr::from_ptr(string) };
    let ref_str = str.to_str()?;
    Ok(ref_str.to_string())
}

fn i18n(string: &str) -> &str {
    string
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

fn show_progress(
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    progress: c_int,
    message: &str,
    text: &str,
) {
    let msg = string_to_ptr_fail_to_null(message);
    let mut real_text: *mut c_char = null_mut();
    if !text.is_empty() {
        real_text = string_to_ptr_fail_to_null(text);
    }
    unsafe {
        if progress_fn as usize != 0 {
            progress_fn(main_klass, progress, msg, real_text);
        }
        string_free(msg);
        string_free(real_text);
    }
}

fn nbt_create_real(
    bytes: *mut Vec<u8>,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
) -> Result<Nbt, Box<dyn Error>> {
    let uncompressed_bytes = unsafe { Box::from_raw(bytes) };

    let mut bytes = Cursor::new(*uncompressed_bytes);
    show_progress(
        progress_fn,
        main_klass,
        0,
        i18n("Reading NBT."),
        &String::new(),
    );
    let nbt = Nbt::read(&mut bytes)?;
    show_progress(
        progress_fn,
        main_klass,
        100,
        i18n("Reading NBT finish."),
        &String::new(),
    );
    Ok(nbt)
}

struct Palette {
    id_name: String,
    property: Vec<(String, String)>,
}

#[derive(Default, Clone)]
struct Block {
    id: u32,
    block_entity: Option<HashMap<String, TreeValue>>,
}

trait GetWithError {
    fn get_compound_with_err(&self, name: &str) -> Result<&NbtCompound, MyError>;
    fn get_int_with_err(&self, name: &str) -> Result<i32, MyError>;
    fn get_list_with_err(&self, name: &str) -> Result<&Vec<NbtTag>, MyError>;
    fn get_string_with_err(&self, name: &str) -> Result<&String, MyError>;
}

impl GetWithError for NbtCompound {
    fn get_compound_with_err(&self, name: &str) -> Result<&NbtCompound, MyError> {
        match self.get_compound(name) {
            Some(ret) => Ok(ret),
            None => Err(MyError {
                msg: i18n("Couldn't get compound.").to_string(),
            }),
        }
    }

    fn get_int_with_err(&self, name: &str) -> Result<i32, MyError> {
        match self.get_int(name) {
            Some(ret) => Ok(ret),
            None => Err(MyError {
                msg: i18n("Couldn't get int.").to_string(),
            }),
        }
    }

    fn get_list_with_err(&self, name: &str) -> Result<&Vec<NbtTag>, MyError> {
        match self.get_list(name) {
            Some(ret) => Ok(ret),
            None => Err(MyError {
                msg: i18n("Couldn't get list.").to_string(),
            }),
        }
    }

    fn get_string_with_err(&self, name: &str) -> Result<&String, MyError> {
        match self.get_string(name) {
            Some(ret) => Ok(ret),
            None => Err(MyError {
                msg: i18n("Couldn't get string.").to_string(),
            }),
        }
    }
}

fn vec_u8_to_i8_safest(vec: Vec<u8>) -> Vec<i8> {
    let mut result = Vec::with_capacity(vec.len());
    for byte in vec {
        result.push(byte as i8);
    }
    result
}

fn convert_nbt_tag_to_tree_value(nbt_tag: NbtTag) -> TreeValue {
    match nbt_tag {
        NbtTag::End => TreeValue::String("error".to_string()),
        NbtTag::Byte(b) => TreeValue::Byte(b),
        NbtTag::Int(i) => TreeValue::Int(i),
        NbtTag::Short(s) => TreeValue::Short(s),
        NbtTag::Long(l) => TreeValue::Long(l),
        NbtTag::ByteArray(ba) => TreeValue::ByteArray(vec_u8_to_i8_safest(ba.to_vec())),
        NbtTag::LongArray(la) => TreeValue::LongArray(la),
        NbtTag::String(str) => TreeValue::String(str),
        NbtTag::Float(f) => TreeValue::Float(f),
        NbtTag::Double(d) => TreeValue::Double(d),
        NbtTag::IntArray(ia) => TreeValue::IntArray(ia),
        NbtTag::List(l) => {
            let mut list: Vec<TreeValue> = vec![];
            for tag in l {
                let new_tag = convert_nbt_tag_to_tree_value(tag);
                list.push(new_tag);
            }
            TreeValue::List(list)
        }
        NbtTag::Compound(c) => TreeValue::Compound(convert_nbt_to_hashmap(&c)),
    }
}

fn convert_nbt_to_hashmap(nbt: &NbtCompound) -> HashMap<String, TreeValue> {
    let child = &nbt.child_tags;
    let mut hashmap = HashMap::new();
    for child_node in child {
        let tree_value = convert_nbt_tag_to_tree_value(child_node.1.clone());
        hashmap.insert(child_node.0.clone(), tree_value);
    }
    hashmap
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
            let string = string_to_ptr_fail_to_null(&palette.id_name);
            region_append_palette(region, string, Box::into_raw(Box::new(palette.property)));
            string_free(string);
        }
        i = 0;
        for block in blocks {
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
pub extern "C" fn region_create_from_file(
    bytes: *mut Vec<u8>,
    progress_fn: ProgressFn,
    region: *mut *mut c_void,
    main_klass: *mut c_void,
) -> *const c_char {
    let mut err_string: String = String::new();
    if !region.is_null() {
        unsafe {
            *region = match region_create_from_bytes_internal(bytes, progress_fn, main_klass) {
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
