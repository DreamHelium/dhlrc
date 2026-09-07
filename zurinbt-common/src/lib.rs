use common_rs::{
    helper_struct::HelperStruct,
    i18n::i18n,
    my_error::MyError,
    region::Palette,
    util::{cstr_to_str, string_to_ptr_fail_to_null},
};
use formatx::formatx;
use gettextrs::gettext;
use std::{
    error::Error,
    ffi::{c_char, c_int},
    ptr::null,
    time::Instant,
};
use sysinfo::System;
use zuri_nbt::NBTTag;

fn init_translation_internal(path: *const c_char) -> Result<(), Box<dyn Error>> {
    gettextrs::bindtextdomain("dhlrc", cstr_to_str(path)?)?;
    gettextrs::textdomain("dhlrc")?;
    Ok(())
}

#[unsafe(no_mangle)]
pub extern "C" fn init_translation(path: *const c_char) -> *const c_char {
    match init_translation_internal(path) {
        Ok(_) => null(),
        Err(e) => string_to_ptr_fail_to_null(&e.to_string()),
    }
}

pub fn gettext_text(str: &str) -> String {
    gettext(str)
}

#[macro_export]
macro_rules! get_compound_value_err_return {
    ($compound : ident, $key : expr) => {{
        let mid_val = $compound.get_key_value($key);
        if mid_val.is_none() {
            return Err(Box::new(MyError {
                msg: format!("Get key {} failed!", $key),
            }));
        }
        mid_val.unwrap().1
    }};
}

#[macro_export]
macro_rules! get_type_from_tag {
    ($tag : expr, $type : ident, $tag_name : expr) => {
        match $tag {
            NBTTag::$type(val) => val,
            _ => {
                return Err(Box::new(MyError {
                    msg: formatx!(gettext_text(i18n("Failed to get {}.")), $tag_name)?,
                }))
            }
        }
    };
}

#[macro_export]
macro_rules! get_type_from_compound {
    ($compound : ident, $key : expr, $type : ident) => {
        get_type_from_tag!(get_compound_value_err_return!($compound, $key), $type, $key)
    };
}

pub fn get_palette_from_nbt_tag(
    palette_list: &Vec<NBTTag>,
    helper_struct: &HelperStruct,
    sys: &mut System,
    instant: &mut Instant,
) -> Result<Vec<Palette>, Box<dyn Error>> {
    let mut palette_vec = vec![];
    let mut i = 0;
    let length = palette_list.len();
    for palette in palette_list {
        helper_struct.progress(
            sys,
            instant,
            (i * 100 / length) as c_int,
            i18n("Getting Palette."),
            "Reading palette is cancelled!",
        )?;
        let internal_compound = get_type_from_tag!(palette, Compound, "Palette");
        let internal_string = get_compound_value_err_return!(internal_compound, "Name")
            .view()
            .string()?;
        let properties_compound_option = internal_compound.get_key_value("Properties");
        let mut has_option = true;
        if properties_compound_option.is_none() {
            has_option = false;
        }
        let mut ret = vec![];
        if has_option {
            let child = &properties_compound_option.unwrap().1.view().compound()?.0;

            for (name, data) in child {
                let real_data = &get_type_from_tag!(data, String, "Property").0;
                ret.push((name.clone(), real_data.clone()));
            }
        }
        palette_vec.push(Palette {
            id_name: internal_string.to_string(),
            property: ret,
        });
        i += 1;
    }
    Ok(palette_vec)
}
