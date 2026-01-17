use crab_nbt::{Nbt, NbtCompound, NbtTag};
use formatx::formatx;
use std::collections::HashMap;
use std::error::Error;
use std::ffi::{CStr, CString, c_char, c_int, c_void};
use std::fmt::{Display, Formatter};
use std::io::Cursor;
use std::ptr::null_mut;

pub type ProgressFn = extern "C" fn(
    main_klass: *mut c_void,
    progress: c_int,
    format: *const c_char,
    text: *const c_char,
);

pub fn string_to_ptr_fail_to_null(string: &str) -> *mut c_char {
    let str = CString::new(string);
    match str {
        Ok(real_str) => real_str.into_raw(),
        Err(_err) => null_mut(),
    }
}

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

#[derive(Debug)]
pub struct MyError {
    pub msg: String,
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

pub fn i18n(string: &str) -> &str {
    string
}

pub fn init_translation_internal(path: *const c_char) -> Result<(), Box<dyn Error>> {
    gettextrs::bindtextdomain("dhlrc", cstr_to_str(path)?)?;
    gettextrs::textdomain("dhlrc")?;
    Ok(())
}

pub trait GetWithError {
    fn get_compound_with_err(&self, name: &str) -> Result<&NbtCompound, Box<dyn Error>>;
    fn get_int_with_err(&self, name: &str) -> Result<i32, Box<dyn Error>>;
    fn get_list_with_err(&self, name: &str) -> Result<&Vec<NbtTag>, Box<dyn Error>>;
    fn get_string_with_err(&self, name: &str) -> Result<&String, Box<dyn Error>>;
}

impl GetWithError for NbtCompound {
    fn get_compound_with_err(&self, name: &str) -> Result<&NbtCompound, Box<dyn Error>> {
        match self.get_compound(name) {
            Some(ret) => Ok(ret),
            None => {
                let str = gettextrs::gettext(i18n("Couldn't get compound of {}."));
                let formatted_str = formatx!(str, name)?;
                Err(Box::new(MyError { msg: formatted_str }))
            }
        }
    }

    fn get_int_with_err(&self, name: &str) -> Result<i32, Box<dyn Error>> {
        match self.get_int(name) {
            Some(ret) => Ok(ret),
            None => {
                let str = gettextrs::dgettext("dhlrc", i18n("Couldn't get int of {}."));
                let formatted_str = formatx!(str, name)?;
                Err(Box::new(MyError { msg: formatted_str }))
            }
        }
    }

    fn get_list_with_err(&self, name: &str) -> Result<&Vec<NbtTag>, Box<dyn Error>> {
        match self.get_list(name) {
            Some(ret) => Ok(ret),
            None => {
                let str = gettextrs::gettext(i18n("Couldn't get list of {}."));
                let formatted_str = formatx!(str, name)?;
                Err(Box::new(MyError { msg: formatted_str }))
            }
        }
    }

    fn get_string_with_err(&self, name: &str) -> Result<&String, Box<dyn Error>> {
        match self.get_string(name) {
            Some(ret) => Ok(ret),
            None => {
                let str = gettextrs::gettext(i18n("Couldn't get string of {}."));
                let formatted_str = formatx!(str, name)?;
                Err(Box::new(MyError { msg: formatted_str }))
            }
        }
    }
}

pub fn show_progress(
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
        drop(Box::from_raw(msg));
        if !real_text.is_null() {
            drop(Box::from_raw(real_text));
        }
    }
}

pub fn nbt_create_real(
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

pub fn convert_nbt_to_hashmap(nbt: &NbtCompound) -> HashMap<String, TreeValue> {
    let child = &nbt.child_tags;
    let mut hashmap = HashMap::new();
    for child_node in child {
        let tree_value = convert_nbt_tag_to_tree_value(child_node.1.clone());
        hashmap.insert(child_node.0.clone(), tree_value);
    }
    hashmap
}
