use std::ffi::c_char;
use std::ptr;
use std::ptr::null;
use crate::i18n::i18n;
use crate::util::string_to_ptr_fail_to_null;

#[derive(Clone)]
#[repr(i32)]
pub enum TreeValue {
    Byte(i8) = 1,
    Short(i16) = 2,
    Int(i32) = 3,
    Long(i64) = 4,
    Float(f32) = 5,
    Double(f64) = 6,
    String(String) = 7,
    ByteArray(Vec<i8>) = 8,
    IntArray(Vec<i32>) = 9,
    LongArray(Vec<i64>) = 10,
    List(Vec<TreeValue>) = 11,
    Compound(Vec<(String, TreeValue)>) = 12,
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_vec_get_len(nbt: *const Vec<(String, TreeValue)>) -> usize {
    if !nbt.is_null() {
        unsafe { nbt.as_ref().unwrap().len() }
    } else {
        0
    }
}

impl TreeValue {
    fn type_to_str(&self) -> &str {
        match self {
            TreeValue::Byte(_) => i18n("Byte"),
            TreeValue::Int(_) => i18n("Int"),
            TreeValue::Short(_) => i18n("Short"),
            TreeValue::Long(_) => i18n("Long"),
            TreeValue::ByteArray(_) => i18n("Byte Array"),
            TreeValue::LongArray(_) => i18n("Long Array"),
            TreeValue::String(_) => i18n("String"),
            TreeValue::Float(_) => i18n("Float"),
            TreeValue::Double(_) => i18n("Double"),
            TreeValue::IntArray(_) => i18n("Int Array"),
            TreeValue::List(_) => i18n("List"),
            TreeValue::Compound(_) => i18n("Compound"),
        }
    }

    fn to_string(&self) -> String {
        match self {
            TreeValue::Byte(b) => b.to_string(),
            TreeValue::Int(i) => i.to_string(),
            TreeValue::Short(s) => s.to_string(),
            TreeValue::Long(l) => l.to_string(),
            TreeValue::ByteArray(ba) => format!("{:?}", ba),
            TreeValue::LongArray(la) => format!("{:?}", la),
            TreeValue::String(s) => s.clone(),
            TreeValue::Float(f) => f.to_string(),
            TreeValue::Double(d) => d.to_string(),
            TreeValue::IntArray(ia) => format!("{:?}", ia),
            TreeValue::List(_) => String::new(),
            TreeValue::Compound(_) => String::new(),
        }
    }

    fn to_int(&self) -> i32 {
        unsafe { *(self as *const TreeValue as *const i32) }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_vec_get_key(
    nbt: *const Vec<(String, TreeValue)>,
    index: usize,
) -> *const c_char {
    if !nbt.is_null() {
        unsafe { string_to_ptr_fail_to_null(&nbt.as_ref().unwrap()[index].0) }
    } else {
        null()
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_vec_get_value_type(
    nbt: *const Vec<(String, TreeValue)>,
    index: usize,
) -> *const c_char {
    if !nbt.is_null() {
        unsafe { string_to_ptr_fail_to_null(&nbt.as_ref().unwrap()[index].1.type_to_str()) }
    } else {
        null()
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_vec_get_value_string(
    nbt: *const Vec<(String, TreeValue)>,
    index: usize,
) -> *const c_char {
    if !nbt.is_null() {
        unsafe { string_to_ptr_fail_to_null(&nbt.as_ref().unwrap()[index].1.to_string()) }
    } else {
        null()
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_vec_get_value_type_int(
    nbt: *const Vec<(String, TreeValue)>,
    index: usize,
) -> i32 {
    if !nbt.is_null() {
        unsafe { nbt.as_ref().unwrap()[index].1.to_int() }
    } else {
        0
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_vec_get_value_to_child(
    nbt: *const Vec<(String, TreeValue)>,
    index: usize,
) -> *const Vec<(String, TreeValue)> {
    if !nbt.is_null() {
        let value = unsafe { &nbt.as_ref().unwrap()[index].1 };
        match value {
            TreeValue::Compound(c) => ptr::from_ref(c),
            _ => null(),
        }
    } else {
        null()
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_vec_get_value_list_to_child(
    nbt: *const Vec<(String, TreeValue)>,
    index: usize,
) -> *const Vec<TreeValue> {
    if !nbt.is_null() {
        let value = unsafe { &nbt.as_ref().unwrap()[index].1 };
        match value {
            TreeValue::List(l) => ptr::from_ref(l),
            _ => null(),
        }
    } else {
        null()
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_vec_tree_value_get_len(tree_value: *const Vec<TreeValue>) -> usize {
    unsafe { (*tree_value).len() }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_vec_tree_value_get_tree_value(
    tree_value: *const Vec<TreeValue>,
    index: usize,
) -> *const TreeValue {
    unsafe { ptr::from_ref(&tree_value.as_ref().unwrap()[index]) }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_tree_value_get_value_string(tree_value: *const TreeValue) -> *const c_char {
    unsafe { string_to_ptr_fail_to_null(&(*tree_value).to_string()) }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_tree_value_get_type_string(tree_value: *const TreeValue) -> *const c_char {
    unsafe { string_to_ptr_fail_to_null(&(*tree_value).type_to_str()) }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_tree_value_get_type_int(tree_value: *const TreeValue) -> i32 {
    unsafe { (*tree_value).to_int() }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_tree_value_get_value_to_child(
    tree_value: *const TreeValue,
) -> *const Vec<(String, TreeValue)> {
    if !tree_value.is_null() {
        let value = unsafe { &*tree_value };
        match value {
            TreeValue::Compound(c) => ptr::from_ref(&c),
            _ => null(),
        }
    } else {
        null()
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_tree_value_get_value_list_to_child(
    tree_value: *const TreeValue,
) -> *const Vec<TreeValue> {
    if !tree_value.is_null() {
        let value = unsafe { &*tree_value };
        match value {
            TreeValue::List(l) => ptr::from_ref(l),
            _ => null(),
        }
    } else {
        null()
    }
}
