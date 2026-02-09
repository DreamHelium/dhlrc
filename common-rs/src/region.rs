use std::error::Error;
use time::{Duration, UtcDateTime};
use crate::tree_value::TreeValue;

pub struct BaseData {
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
    /** Default: Unnamed */
    region_name: String,
}

impl BaseData {
    fn set_time(&mut self, create_time: i64, modify_time: i64) -> Result<bool, Box<dyn Error>> {
        self.create_time = UtcDateTime::from_unix_timestamp(create_time / 1000)?;
        self.create_time += Duration::milliseconds(create_time % 1000);
        self.modify_time = UtcDateTime::from_unix_timestamp(modify_time / 1000)?;
        self.modify_time += Duration::milliseconds(modify_time % 1000);
        Ok(true)
    }
    pub fn get_create_timestamp(&self) -> i64 {
        self.create_time.unix_timestamp() * 1000 + self.create_time.millisecond() as i64
    }

    pub fn get_modify_timestamp(&self) -> i64 {
        self.modify_time.unix_timestamp() * 1000 + self.modify_time.millisecond() as i64
    }

    pub fn get_description(&self) -> &str {
        self.description.as_str()
    }

    pub fn set_description(&mut self, string: &str) {
        self.description = string.to_string()
    }

    pub fn get_author(&self) -> &str {
        self.author.as_str()
    }

    pub fn set_author(&mut self, string: &str) {
        self.author = string.to_string()
    }

    pub fn get_name(&self) -> &str {
        self.name.as_str()
    }

    pub fn set_name(&mut self, string: &str) {
        self.name = string.to_string()
    }

    pub fn set_region_name(&mut self, string: &str) {
        self.region_name = string.to_string();
    }

    pub fn get_region_name(&mut self) -> &str {
        self.region_name.as_str()
    }
}

impl Default for BaseData {
    fn default() -> Self {
        let temp_username = whoami::username();
        let real_username = temp_username.unwrap_or_else(|_err| "".to_string());
        BaseData {
            create_time: UtcDateTime::now(),
            modify_time: UtcDateTime::now(),
            description: "".to_string(),
            author: real_username,
            name: "Converted".to_string(),
            region_name: "Unnamed".to_string(),
        }
    }
}

pub struct Palette {
    pub id_name: String,
    pub property: Vec<(String, String)>,
}

pub struct BlockEntity {
    pub pos: (i32, i32, i32),
    pub entity: Vec<(String, TreeValue)>,
}

#[derive(Default)]
pub struct Region {
    /* Base information */
    pub data_version: u32,
    pub base_data: BaseData,
    /** The size of the region */
    pub region_size: (i32, i32, i32),
    /** The offset */
    pub region_offset: (i32, i32, i32),
    /** The block info array */
    pub block_array: Vec<u32>,
    /** The block entity array */
    pub block_entity_array: Vec<BlockEntity>,
    /** Entity array, use TreeValue::Compound */
    pub entity_array: Vec<Vec<(String, TreeValue)>>,
    /** The Palette info array*/
    pub palette_array: Vec<Palette>,
}

impl Region {
    pub fn set_data_time(
        &mut self,
        create_time: i64,
        modify_time: i64,
    ) -> Result<bool, Box<dyn Error>> {
        self.base_data.set_time(create_time, modify_time)
    }

    pub fn get_size(&self) {}
    pub fn get_palette_len(&self) -> usize {
        self.palette_array.len()
    }
}