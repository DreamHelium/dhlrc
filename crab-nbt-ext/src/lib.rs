use bytes::Buf;
use cesu8::from_java_cesu8;
use common_rs::cancel_flag::cancel_flag_is_cancelled;
use common_rs::i18n::i18n;
use common_rs::my_error::MyError;
use common_rs::region::Palette;
use common_rs::tree_value::TreeValue;
use common_rs::util::{cstr_to_str, show_progress, string_to_ptr_fail_to_null};
use common_rs::util::{finish_oom, system_info_object_free};
use common_rs::{ProgressFn, show_progress_macro};
use crab_nbt::{Nbt, NbtCompound, NbtTag, error};
use formatx::formatx;
use gettextrs::gettext;
use std::error::Error;
use std::ffi::{c_char, c_int, c_void};
use std::io::Cursor;
use std::ptr::null;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Instant;
use sysinfo::System;

#[macro_export]
macro_rules! get_compound {
    ($output_symbol : ident, $tag : ident, $err_msg : expr) => {
        let $output_symbol = match $tag {
            NbtTag::Compound(c) => c,
            _ => {
                return Err(Box::from(MyError {
                    msg: String::from($err_msg),
                }));
            }
        };
    };
}

pub fn init_translation_internal(path: *const c_char) -> Result<(), Box<dyn Error>> {
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

#[unsafe(no_mangle)]
pub extern "C" fn region_get_object(
    bytes: *mut Vec<u8>,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    cancel_flag: *const AtomicBool,
    object: *mut *mut Nbt,
    elapsed_millisecs: u64,
    free_memory: u64,
) -> *const c_char {
    match nbt_create_real(
        bytes,
        progress_fn,
        main_klass,
        cancel_flag,
        elapsed_millisecs as u128,
        free_memory,
    ) {
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

pub trait GetWithError {
    fn get_compound_with_err(&self, name: &str) -> Result<&NbtCompound, Box<dyn Error>>;
    fn get_int_with_err(&self, name: &str) -> Result<i32, Box<dyn Error>>;
    fn get_list_with_err(&self, name: &str) -> Result<&Vec<NbtTag>, Box<dyn Error>>;
    fn get_string_with_err(&self, name: &str) -> Result<&String, Box<dyn Error>>;
    fn get_long_with_err(&self, name: &str) -> Result<i64, Box<dyn Error>>;
    fn get_long_array_with_err(&self, name: &str) -> Result<&Vec<i64>, Box<dyn Error>>;
}

impl GetWithError for NbtCompound {
    fn get_compound_with_err(&self, name: &str) -> Result<&NbtCompound, Box<dyn Error>> {
        match self.get_compound(name) {
            Some(ret) => Ok(ret),
            None => {
                let str = gettext(i18n("Couldn't get compound of {}."));
                let formatted_str = formatx!(str, name)?;
                Err(Box::new(MyError { msg: formatted_str }))
            }
        }
    }

    fn get_int_with_err(&self, name: &str) -> Result<i32, Box<dyn Error>> {
        match self.get_int(name) {
            Some(ret) => Ok(ret),
            None => {
                let str = gettext(i18n("Couldn't get int of {}."));
                let formatted_str = formatx!(str, name)?;
                Err(Box::new(MyError { msg: formatted_str }))
            }
        }
    }

    fn get_list_with_err(&self, name: &str) -> Result<&Vec<NbtTag>, Box<dyn Error>> {
        match self.get_list(name) {
            Some(ret) => Ok(ret),
            None => {
                let str = gettext(i18n("Couldn't get list of {}."));
                let formatted_str = formatx!(str, name)?;
                Err(Box::new(MyError { msg: formatted_str }))
            }
        }
    }

    fn get_string_with_err(&self, name: &str) -> Result<&String, Box<dyn Error>> {
        match self.get_string(name) {
            Some(ret) => Ok(ret),
            None => {
                let str = gettext(i18n("Couldn't get string of {}."));
                let formatted_str = formatx!(str, name)?;
                Err(Box::new(MyError { msg: formatted_str }))
            }
        }
    }

    fn get_long_with_err(&self, name: &str) -> Result<i64, Box<dyn Error>> {
        match self.get_long(name) {
            Some(ret) => Ok(ret),
            None => {
                let str = gettext(i18n("Couldn't get long of {}."));
                let formatted_str = formatx!(str, name)?;
                Err(Box::new(MyError { msg: formatted_str }))
            }
        }
    }

    fn get_long_array_with_err(&self, name: &str) -> Result<&Vec<i64>, Box<dyn Error>> {
        match self.get_long_array(name) {
            Some(ret) => Ok(ret),
            None => {
                let str = gettext(i18n("Couldn't get long array of {}."));
                let formatted_str = formatx!(str, name)?;
                Err(Box::new(MyError { msg: formatted_str }))
            }
        }
    }
}

pub fn nbt_create_real(
    bytes: *mut Vec<u8>,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    cancel_flag: *const AtomicBool,
    elapsed_millisecs: u128,
    free_memory: u64,
) -> Result<Nbt, Box<dyn Error>> {
    let uncompressed_bytes = unsafe { &*bytes };

    let mut bytes = Cursor::new(uncompressed_bytes);

    let nbt = nbt_read_with_progress(
        &mut bytes,
        progress_fn,
        main_klass,
        cancel_flag,
        elapsed_millisecs,
        free_memory,
    )?;

    show_progress(
        progress_fn,
        main_klass,
        100,
        i18n("Reading NBT finish."),
        &String::new(),
    );
    Ok(nbt)
}

pub fn vec_u8_to_i8_safest(vec: Vec<u8>) -> Vec<i8> {
    let mut result = Vec::with_capacity(vec.len());
    for byte in vec {
        result.push(byte as i8);
    }
    result
}

fn vec_i8_to_u8_safest(vec: Vec<i8>) -> Vec<u8> {
    let mut result = Vec::with_capacity(vec.len());
    for byte in vec {
        result.push(byte as u8);
    }
    result
}

pub fn convert_nbt_tag_to_tree_value(nbt_tag: &NbtTag) -> TreeValue {
    match nbt_tag {
        NbtTag::End => TreeValue::String("error".to_string()),
        NbtTag::Byte(b) => TreeValue::Byte(*b),
        NbtTag::Int(i) => TreeValue::Int(*i),
        NbtTag::Short(s) => TreeValue::Short(*s),
        NbtTag::Long(l) => TreeValue::Long(*l),
        NbtTag::ByteArray(ba) => TreeValue::ByteArray(vec_u8_to_i8_safest(ba.to_vec())),
        NbtTag::LongArray(la) => TreeValue::LongArray(la.clone()),
        NbtTag::String(str) => TreeValue::String(str.clone()),
        NbtTag::Float(f) => TreeValue::Float(*f),
        NbtTag::Double(d) => TreeValue::Double(*d),
        NbtTag::IntArray(ia) => TreeValue::IntArray(ia.clone()),
        NbtTag::List(l) => {
            let mut list: Vec<TreeValue> = vec![];
            for tag in l {
                let new_tag = convert_nbt_tag_to_tree_value(tag);
                list.push(new_tag);
            }
            TreeValue::List(list)
        }
        NbtTag::Compound(c) => TreeValue::Compound(convert_nbt_to_vec(&c)),
    }
}

pub fn convert_nbt_to_vec(nbt: &NbtCompound) -> Vec<(String, TreeValue)> {
    let child = &nbt.child_tags;
    let mut ret: Vec<(String, TreeValue)> = vec![];
    for child_node in child {
        let tree_value = convert_nbt_tag_to_tree_value(&child_node.1);
        ret.push((child_node.0.clone(), tree_value));
    }
    ret
}

fn convert_tree_value_to_nbt_tag(tree_value: &TreeValue, str: &str) -> NbtTag {
    match tree_value {
        TreeValue::Byte(b) => NbtTag::Byte(*b),
        TreeValue::Short(s) => NbtTag::Short(*s),
        TreeValue::Int(i) => NbtTag::Int(*i),
        TreeValue::Long(l) => NbtTag::Long(*l),
        TreeValue::Float(f) => NbtTag::Float(*f),
        TreeValue::Double(d) => NbtTag::Double(*d),
        TreeValue::ByteArray(ba) => {
            let vec_u8 = vec_i8_to_u8_safest(ba.clone());
            let bytes = bytes::Bytes::from(vec_u8);
            NbtTag::ByteArray(bytes)
        }
        TreeValue::IntArray(ia) => NbtTag::IntArray(ia.clone()),
        TreeValue::LongArray(la) => NbtTag::LongArray(la.clone()),
        TreeValue::String(s) => NbtTag::String(s.clone()),
        TreeValue::List(l) => {
            let mut list: Vec<NbtTag> = vec![];
            for tag in l {
                let new_tag = convert_tree_value_to_nbt_tag(tag, "");
                list.push(new_tag);
            }
            NbtTag::List(list)
        }
        TreeValue::Compound(c) => NbtTag::Compound(convert_vec_to_nbt_compound(&c)),
    }
}

fn convert_vec_to_nbt_compound(vec: &Vec<(String, TreeValue)>) -> NbtCompound {
    let mut nbt_vec = vec![];
    for child_node in vec {
        let nbt_tag = convert_tree_value_to_nbt_tag(&child_node.1, &child_node.0);
        let real_nbt = (child_node.0.clone(), nbt_tag);
        nbt_vec.push(real_nbt);
    }
    NbtCompound {
        child_tags: nbt_vec,
    }
}

pub fn convert_vec_to_nbt(vec: &Vec<(String, TreeValue)>, str: &str, from_file: bool) -> Nbt {
    let mut nbt_vec = vec![];
    for child_node in vec {
        let nbt_tag = convert_tree_value_to_nbt_tag(&child_node.1, &child_node.0);
        let real_nbt = (child_node.0.clone(), nbt_tag);
        nbt_vec.push(real_nbt);
    }
    if from_file {
        Nbt {
            name: nbt_vec[0].0.clone(),
            root_tag: NbtCompound {
                child_tags: match &nbt_vec[0].1 {
                    NbtTag::Compound(c) => c.child_tags.clone(),
                    _ => vec![],
                },
            },
        }
    } else {
        // let tag = NbtTag::Compound(NbtCompound {
        // child_tags: nbt_vec,
        // });
        // let final_vec = vec![(str.to_string(), tag)];
        Nbt {
            name: str.to_string(),
            root_tag: NbtCompound {
                child_tags: nbt_vec,
            },
        }
    }
}

pub fn get_palette_from_nbt_tag(
    palette_list: &Vec<NbtTag>,
    cancel_flag: *const AtomicBool,
) -> Result<Vec<Palette>, Box<dyn Error>> {
    let mut palette_vec = vec![];
    for palette in palette_list {
        if cancel_flag_is_cancelled(cancel_flag) == 1 {
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
    Ok(palette_vec)
}

pub fn gettext_text(str: &str) -> String {
    gettext(str)
}

pub fn nbt_read_with_progress(
    bytes: &mut impl Buf,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    cancel_flag: *const AtomicBool,
    elapsed_millisecs: u128,
    free_memory: u64,
) -> Result<Nbt, Box<dyn Error>> {
    let size = bytes.remaining();

    let tag_type_id = bytes.get_u8();

    if tag_type_id != 10 {
        return Err(Box::new(error::Error::NoRootCompound(tag_type_id)));
    }

    let mut instant = Instant::now();
    let mut sys = System::new_all();

    let name = get_nbt_string(bytes)?;
    let root_tag = compound_deserialize_content(
        bytes,
        progress_fn,
        main_klass,
        cancel_flag,
        size,
        elapsed_millisecs,
        free_memory,
        &mut instant,
        &mut sys,
    )?;

    Ok(Nbt { name, root_tag })
}

fn compound_deserialize_content(
    bytes: &mut impl Buf,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    cancel_flag: *const AtomicBool,
    size: usize,
    elapsed_millisecs: u128,
    free_memory: u64,
    instant: &mut Instant,
    sys: &mut System,
) -> Result<NbtCompound, Box<dyn Error>> {
    // let mut compound = NbtCompound::new();
    let mut vec = vec![];
    while bytes.has_remaining() {
        let tag_id = bytes.get_u8();
        if tag_id == 0 {
            break;
        }

        show_progress_macro!(
            instant,
            sys,
            progress_fn,
            main_klass,
            (100 - bytes.remaining() * 100 / size) as c_int,
            elapsed_millisecs,
            free_memory,
            i18n("Reading NBT."),
            cancel_flag,
            gettext(i18n("The parsing operation is cancelled."))
        );

        let name = get_nbt_string(bytes)?;

        let tag = deserialize_data(
            bytes,
            tag_id,
            progress_fn,
            main_klass,
            cancel_flag,
            size,
            elapsed_millisecs,
            free_memory,
            instant,
            sys,
        )?;
        let value = (name, tag);
        vec.push(value);
    }

    Ok(NbtCompound { child_tags: vec })
}

pub fn deserialize_data(
    bytes: &mut impl Buf,
    tag_id: u8,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    cancel_flag: *const AtomicBool,
    size: usize,
    elapsed_millisecs: u128,
    free_memory: u64,
    instant: &mut Instant,
    sys: &mut System,
) -> Result<NbtTag, Box<dyn Error>> {
    show_progress_macro!(
        instant,
        sys,
        progress_fn,
        main_klass,
        (100 - bytes.remaining() * 100 / size) as c_int,
        elapsed_millisecs,
        free_memory,
        i18n("Reading NBT."),
        cancel_flag,
        gettext(i18n("The parsing operation is cancelled."))
    );
    match tag_id {
        0 => Ok(NbtTag::End),
        1 => {
            let byte = bytes.get_i8();
            Ok(NbtTag::Byte(byte))
        }
        2 => {
            let short = bytes.get_i16();
            Ok(NbtTag::Short(short))
        }
        3 => {
            let int = bytes.get_i32();
            Ok(NbtTag::Int(int))
        }
        4 => {
            let long = bytes.get_i64();
            Ok(NbtTag::Long(long))
        }
        5 => {
            let float = bytes.get_f32();
            Ok(NbtTag::Float(float))
        }
        6 => {
            let double = bytes.get_f64();
            Ok(NbtTag::Double(double))
        }
        7 => {
            let len = bytes.get_i32() as usize;
            let byte_array = bytes.copy_to_bytes(len);
            Ok(NbtTag::ByteArray(byte_array))
        }
        8 => Ok(NbtTag::String(get_nbt_string(bytes).unwrap())),
        9 => {
            let tag_type_id = bytes.get_u8();
            let len = bytes.get_i32();
            let mut list = Vec::with_capacity(len as usize);
            for _ in 0..len {
                let tag = deserialize_data(
                    bytes,
                    tag_type_id,
                    progress_fn,
                    main_klass,
                    cancel_flag,
                    size,
                    elapsed_millisecs,
                    free_memory,
                    instant,
                    sys,
                )?;
                assert_eq!(tag.get_type_id(), tag_type_id);
                list.push(tag);
            }
            Ok(NbtTag::List(list))
        }
        10 => Ok(NbtTag::Compound(compound_deserialize_content(
            bytes,
            progress_fn,
            main_klass,
            cancel_flag,
            size,
            elapsed_millisecs,
            free_memory,
            instant,
            sys,
        )?)),
        11 => {
            const BYTES: usize = size_of::<i32>();

            let len = bytes.get_i32() as usize;
            let numbers = read_array::<i32, BYTES, _>(bytes, len, i32::from_be_bytes);
            Ok(NbtTag::IntArray(numbers))
        }
        12 => {
            const BYTES: usize = size_of::<i64>();

            let len = bytes.get_i32() as usize;
            let numbers = read_array::<i64, BYTES, _>(bytes, len, i64::from_be_bytes);
            Ok(NbtTag::LongArray(numbers))
        }
        _ => Err(Box::from(error::Error::UnknownTagId(tag_id))),
    }
}

// Copied from crab_nbt
// This can be improved once rust-lang/rust#132980 is resolved:
// Instead of passing `BYTES` manually, we could use const generics, e.g. `size_of::<T>()`.
fn read_array<T, const N: usize, F>(bytes: &mut impl Buf, len: usize, from_be: F) -> Vec<T>
where
    F: Fn([u8; N]) -> T,
{
    bytes
        .copy_to_bytes(len * N)
        .chunks_exact(N)
        .map(|chunk| {
            let arr: [u8; N] = chunk.try_into().expect("chunk size mismatch");
            from_be(arr)
        })
        .collect()
}

fn get_nbt_string(bytes: &mut impl Buf) -> Result<String, error::Error> {
    let len = bytes.get_u16() as usize;
    let string_bytes = bytes.copy_to_bytes(len);
    let string = from_java_cesu8(&string_bytes).map_err(|_| error::Error::InvalidJavaString)?;
    Ok(string.to_string())
}
