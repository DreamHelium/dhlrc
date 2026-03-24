use common_rs::i18n::i18n;
use common_rs::region::{BaseData, Region};
use common_rs::util::cstr_to_str;
use gettextrs::gettext;
use std::ffi::c_char;
use std::string::ToString;
use std::sync::{LazyLock, Mutex};
use time::UtcDateTime;

static DEFAULT_AUTHOR: LazyLock<Mutex<String>> = LazyLock::new(|| Mutex::new(String::new()));
static DEFAULT_BASE_NAME: LazyLock<Mutex<String>> = LazyLock::new(|| Mutex::new(String::new()));
static DEFAULT_REGION_NAME: LazyLock<Mutex<String>> = LazyLock::new(|| Mutex::new(String::new()));
static DEFAULT_DESCRIPTION: LazyLock<Mutex<String>> = LazyLock::new(|| Mutex::new(String::new()));

pub trait NewOne {
    fn new_one() -> Self;
}

impl NewOne for BaseData {
    fn new_one() -> Self {
        BaseData {
            create_time: UtcDateTime::now(),
            modify_time: UtcDateTime::now(),
            description: DEFAULT_DESCRIPTION.lock().unwrap().clone(),
            author: DEFAULT_AUTHOR.lock().unwrap().clone(),
            name: DEFAULT_BASE_NAME.lock().unwrap().clone(),
            region_name: DEFAULT_REGION_NAME.lock().unwrap().clone(),
        }
    }
}

impl NewOne for Region {
    fn new_one() -> Self {
        Region {
            data_version: 0,
            base_data: BaseData::new_one(),
            region_size: (0, 0, 0),
            region_offset: (0, 0, 0),
            block_array: vec![],
            block_entity_array: vec![],
            entity_array: vec![],
            palette_array: vec![],
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn init_default_strings(
    new_author: *const c_char,
    new_base_name: *const c_char,
    new_region_name: *const c_char,
    new_description: *const c_char,
) {
    let mut author = LazyLock::force(&DEFAULT_AUTHOR).lock().unwrap();
    *author = cstr_to_str(new_author)
        .unwrap_or_else(|_| whoami::username().unwrap_or_else(|_err| "unknown".to_string()));
    *LazyLock::force(&DEFAULT_BASE_NAME).lock().unwrap() =
        cstr_to_str(new_base_name).unwrap_or_else(|_err| gettext(i18n("Converted")));
    *LazyLock::force(&DEFAULT_REGION_NAME).lock().unwrap() =
        cstr_to_str(new_region_name).unwrap_or_else(|_err| gettext(i18n("Unnamed")));
    *LazyLock::force(&DEFAULT_DESCRIPTION).lock().unwrap() =
        cstr_to_str(new_description).unwrap_or_else(|_err| "".to_string());
}

#[unsafe(no_mangle)]
pub extern "C" fn reset_default_description(new_description: *const c_char) {
    let mut real_s = DEFAULT_DESCRIPTION.lock().unwrap();
    *real_s = cstr_to_str(new_description).unwrap_or_else(|_err| "".to_string());
}
