use common_rs::tree_value::TreeValue;
use crab_nbt_ext::{convert_nbt_to_vec, convert_vec_to_nbt, nbt_create_real, vec_u8_to_i8_safest};
use std::error::Error;
use std::ffi::{CStr, c_char, c_int, c_void};
use std::fs::File;
use std::io::{Cursor, Read, Write};
use std::ops::Deref;
use std::ptr::{null, null_mut};
use std::sync::atomic::AtomicBool;
use zuri_nbt::NBTTag;
use zuri_nbt::encoding::LittleEndian;
use zuri_nbt::reader::{Reader, Res};
use zuri_nbt::tag::Compound;

type ProgressFn = Option<
    extern "C" fn(
        main_klass: *mut c_void,
        progress: c_int,
        format: *const c_char,
        text: *const c_char,
    ),
>;

#[link(name = "region_rs")]
unsafe extern "C" {
    fn file_try_uncompress(
        filename: *const c_char,
        progress_fn: ProgressFn,
        main_klass: *mut c_void,
        failed: *mut c_int,
        cancel_flag: *const AtomicBool,
    ) -> *mut Vec<u8>;
    fn vec_free(vec: *mut Vec<u8>);
}

fn le_nbt_compound_to_tree_value(compound: &Compound) -> TreeValue {
    let real_compound = &compound.0;
    let mut ret: Vec<(String, TreeValue)> = vec![];
    for child_node in real_compound {
        let tree_value = le_nbt_to_tree_value(&child_node.1);
        if child_node.0 == "Schematic" {
            println!("Schematic");
        }
        ret.push((child_node.0.clone(), tree_value));
    }
    TreeValue::Compound(ret)
}

fn le_nbt_to_tree_value(nbt_tag: &NBTTag) -> TreeValue {
    match nbt_tag {
        NBTTag::Byte(b) => TreeValue::Byte(b.0 as i8),
        NBTTag::Int(i) => TreeValue::Int(i.0),
        NBTTag::Short(s) => TreeValue::Short(s.0),
        NBTTag::Long(l) => TreeValue::Long(l.0),
        NBTTag::Float(f) => TreeValue::Float(f.0),
        NBTTag::Double(d) => TreeValue::Double(d.0),
        NBTTag::String(s) => TreeValue::String(s.0.clone()),
        NBTTag::IntArray(ia) => TreeValue::IntArray(ia.0.clone()),
        NBTTag::LongArray(la) => TreeValue::LongArray(la.0.clone()),
        NBTTag::List(l) => {
            let mut list = vec![];
            for tag in l.0.clone() {
                let new_tag = le_nbt_to_tree_value(&tag);
                list.push(new_tag);
            }
            TreeValue::List(list)
        }
        NBTTag::Compound(c) => le_nbt_compound_to_tree_value(c),
        NBTTag::ByteArray(ba) => TreeValue::ByteArray(vec_u8_to_i8_safest(ba.0.clone())),
    }
}

fn vec_to_le_nbt(vec: Vec<u8>) -> Result<TreeValue, Box<dyn Error>> {
    let nbt = NBTTag::read(&*vec, LittleEndian)?;
    let mut le = LittleEndian::default();
    let mut cursor = Cursor::new(vec);
    let long = le.u8(&mut cursor)?;
    let str = le.string(&mut cursor)?;
    let tree_value = le_nbt_to_tree_value(&nbt);
    let final_value = TreeValue::Compound(vec![(str, tree_value)]);
    Ok(final_value)
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_vec_to_file(
    vec: *const Vec<(String, TreeValue)>,
    filename: *const c_char,
    from_file: c_int,
) {
    let tree_vec = unsafe { &(*vec) };
    let nbt = convert_vec_to_nbt(tree_vec, "", from_file != 0);
    let mut file = File::create(unsafe { CStr::from_ptr(filename) }.to_str().unwrap()).unwrap();
    let bytes = nbt.write();
    file.write_all(&*bytes).unwrap();
}

#[unsafe(no_mangle)]
pub extern "C" fn file_to_nbt_vec(
    filename: *const c_char,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
) -> *mut Vec<(String, TreeValue)> {
    let mut failed = 0;
    let vector = unsafe {
        file_try_uncompress(
            filename,
            progress_fn,
            main_klass,
            &mut failed as *mut c_int,
            null(),
        )
    };
    if failed == 1 {
        unsafe { vec_free(vector) };
        null_mut()
    } else {
        match nbt_create_real(vector, progress_fn, main_klass, null()) {
            Ok(nbt) => {
                unsafe { vec_free(vector) };
                let mid_val = convert_nbt_to_vec(&nbt.root_tag);
                let tree_value = TreeValue::Compound(mid_val);
                let nbt_string = nbt.name;
                let ret = vec![(nbt_string, tree_value)];
                Box::into_raw(Box::new(ret))
            }
            Err(_) => {
                /* Try to convert to Bedrock NBT */
                let real_vec = unsafe { Box::from_raw(vector) };
                match vec_to_le_nbt(*real_vec) {
                    Err(_) => null_mut(),
                    Ok(vec) => match vec {
                        TreeValue::Compound(c) => Box::into_raw(Box::new(c)),
                        _ => null_mut(),
                    },
                }
            }
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_vec_free(vec: *mut Vec<(String, TreeValue)>) {
    if !vec.is_null() {
        drop(unsafe { Box::from_raw(vec) })
    }
}
