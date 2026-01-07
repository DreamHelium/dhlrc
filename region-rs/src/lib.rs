use std::error::Error;
use std::ffi::{CString, c_char, c_void};
use time::UtcDateTime;
struct BaseData {
    /** Default: time of generated */
    create_time: UtcDateTime,
    /** Default: time of generated */
    modify_time: UtcDateTime,
    /** Default: "" */
    description: String,
    /** Default: username */
    author: String,
    /** Default: Converted */
    name: String,
}

impl Default for BaseData {
    fn default() -> Self {
        BaseData {
            create_time: UtcDateTime::now(),
            modify_time: UtcDateTime::now(),
            description: "".to_string(),
            author: "".to_string(),
            name: "".to_string(),
        }
    }
}

struct BlockEntity {
    pos: (i32, i32, i32),
    // preserved
    tree: *mut c_void,
}

struct Palette {
    id_name: String,
    property: Vec<(String, String)>,
}

#[derive(Default)]
pub struct Region {
    /* Base information */
    data_version: u8,
    base_data: BaseData,
    /** The size of the region */
    region_size: (i32, i32, i32),
    /** The block info array */
    block_array: Vec<i64>,
    /** Block Entity Array */
    block_entity_array: Vec<BlockEntity>,
    /** The Palette info array*/
    palette_array: Vec<Palette>,
    air_palette: u8,
}

#[unsafe(no_mangle)]
pub extern "C" fn region_new() -> *mut Region {
    let region = Box::new(Region::default());
    Box::into_raw(region)
}

#[unsafe(no_mangle)]
pub extern "C" fn region_free(region: *mut Region) {
    let r = unsafe { Box::from_raw(region) };
    drop(r);
}

fn region_set_time_internal(
    base_data: &mut BaseData,
    create_time: i64,
    modify_time: i64,
) -> Result<bool, Box<dyn Error>> {
    base_data.create_time = UtcDateTime::from_unix_timestamp(create_time)?;
    base_data.modify_time = UtcDateTime::from_unix_timestamp(modify_time)?;
    Ok(true)
}

#[unsafe(no_mangle)]
pub extern "C" fn region_set_time(
    region: *mut Region,
    create_time: i64,
    modify_time: i64,
) -> *const c_char {
    let r = unsafe { Box::from_raw(region) };
    let mut base_data = r.base_data;
    let set_time_true = region_set_time_internal(&mut base_data, create_time, modify_time);
    match set_time_true {
        Ok(ret) => CString::new("").unwrap().into_raw(),
        Err(err) => CString::new("").unwrap().into_raw(),
    }
}
