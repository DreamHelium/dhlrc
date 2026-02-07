use crab_nbt::{Nbt, NbtCompound, NbtTag};
use formatx::formatx;
use gettextrs::gettext;
use std::error::Error;
use std::ffi::{c_char, c_void};
use std::io::{Cursor};
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use common_rs::i18n::i18n;
use common_rs::my_error::MyError;
use common_rs::ProgressFn;
use common_rs::tree_value::TreeValue;
use common_rs::util::{cstr_to_str, show_progress};

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

fn cancel_flag_is_cancelled(cancel_flag: *const AtomicBool) -> bool {
    if cancel_flag.is_null() {
        return false;
    }
    let arc = unsafe { Arc::from_raw(cancel_flag) };
    let ret = arc.load(Ordering::SeqCst);
    let _ = Arc::into_raw(arc);
    ret
}

pub fn nbt_create_real(
    bytes: *mut Vec<u8>,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    cancel_flag: *const AtomicBool,
) -> Result<Nbt, Box<dyn Error>> {
    let uncompressed_bytes = unsafe { (*bytes).clone() };

    let mut bytes = Cursor::new(uncompressed_bytes);
    show_progress(
        progress_fn,
        main_klass,
        0,
        i18n("Reading NBT."),
        &String::new(),
    );
    if cancel_flag_is_cancelled(cancel_flag) {
        return Err(Box::new(MyError {
            msg: gettext(i18n("The parsing operation is cancelled.")),
        }));
    }
    let nbt = Nbt::read(&mut bytes)?;
    if cancel_flag_is_cancelled(cancel_flag) {
        return Err(Box::new(MyError {
            msg: gettext(i18n("The parsing operation is cancelled.")),
        }));
    }
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
        TreeValue::Compound(c) => {
            NbtTag::Compound(convert_vec_to_nbt_compound(&c))
        }
    }
}

fn convert_vec_to_nbt_compound(vec : &Vec<(String, TreeValue)>) -> NbtCompound{
    let mut nbt_vec = vec![];
    for child_node in vec{
        let nbt_tag = convert_tree_value_to_nbt_tag(&child_node.1, &child_node.0);
        let real_nbt = (child_node.0.clone(), nbt_tag);
        nbt_vec.push(real_nbt);
    }
    NbtCompound{
        child_tags : nbt_vec
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

pub struct Palette {
    pub id_name: String,
    pub property: Vec<(String, String)>,
}
pub fn get_palette_from_nbt_tag(
    palette_list: &Vec<NbtTag>,
    cancel_flag: *const AtomicBool,
) -> Result<Vec<Palette>, Box<dyn Error>> {
    let mut palette_vec = vec![];
    for palette in palette_list {
        if cancel_flag_is_cancelled(cancel_flag) {
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
