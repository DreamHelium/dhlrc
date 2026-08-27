use cesu8::from_java_cesu8;
use common_rs::{
    ProgressFn,
    i18n::i18n,
    tree_value::TreeValue,
    util::{cstr_to_str, finish_oom, show_progress, string_free, string_to_ptr_fail_to_null},
};
use gettextrs::gettext;
use std::{
    any::Any,
    collections::HashMap,
    error::Error,
    ffi::{CString, c_char, c_int, c_void},
    io::prelude::Read,
    ptr::{self, null, null_mut},
    sync::atomic::AtomicBool,
    time::Instant,
};
use sysinfo::System;
use zuri_nbt::{
    NBTRoot, NBTTag,
    encoding::{BigEndian, LittleEndian, NetworkLittleEndian},
    err::{NBTError, PathPart, ReadError},
    reader::{Reader, Res},
    tag::{self, Compound, List},
};

#[link(name = "region_rs")]
unsafe extern "C" {
    fn cancel_flag_is_cancelled(ptr: *const AtomicBool) -> c_int;
    fn file_try_uncompress(
        filename: *const c_char,
        progress_fn: ProgressFn,
        main_klass: *mut c_void,
        failed: *mut c_int,
        cancel_flag: *const AtomicBool,
        elapsed_millisecs: u64,
        free_memory: u64,
    ) -> *mut Vec<u8>;
    fn vec_free(vec: *mut Vec<u8>);
}

struct ReadStruct<R: Reader> {
    real_reader: R,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    original_bytes: usize,
    bytes_read: usize,
    instant: Instant,
    sys: System,
    elapsed_millisecs: u128,
    free_memory: u64,
    cancel_flag: *const AtomicBool,
}

impl<T: Reader> ReadStruct<T> {
    fn new(
        real_reader: T,
        progress_fn: ProgressFn,
        main_klass: *mut c_void,
        original_bytes: usize,
        elapsed_millisecs: u64,
        free_memory: u64,
        cancel_flag: *const AtomicBool,
    ) -> Self {
        Self {
            real_reader,
            progress_fn,
            main_klass,
            original_bytes,
            bytes_read: 0,
            instant: Instant::now(),
            sys: System::new_all(),
            elapsed_millisecs: elapsed_millisecs as u128,
            free_memory,
            cancel_flag,
        }
    }

    fn report(&mut self) -> Result<(), NBTError<ReadError>> {
        if (self.instant).elapsed().as_millis() >= (self.elapsed_millisecs) {
            if unsafe { cancel_flag_is_cancelled(self.cancel_flag) == 1 } {
                return Err(NBTError::new(ReadError::Custom(
                    i18n("Operation canceled!").to_string(),
                )));
            }
            if finish_oom(&mut self.sys, self.free_memory).is_err() {
                return Err(NBTError::new(ReadError::Custom(
                    i18n("Out of memory!").to_string(),
                )));
            };
            show_progress(
                self.progress_fn,
                self.main_klass,
                (self.bytes_read * 100 / self.original_bytes) as c_int,
                i18n("Loading NBT"),
                "",
            );
            self.instant = Instant::now();
        }
        Ok(())
    }
}

impl<T: Reader + 'static> Reader for ReadStruct<T> {
    fn i16<R: Read>(&mut self, reader: &mut R) -> Res<i16> {
        let ret = self.real_reader.i16(reader);
        self.bytes_read += size_of::<i16>();
        self.report()?;
        ret
    }

    fn i32<R: Read>(&mut self, reader: &mut R) -> Res<i32> {
        let ret = self.real_reader.i32(reader);
        self.bytes_read += size_of::<i32>();
        self.report()?;
        ret
    }

    fn i64<R: Read>(&mut self, reader: &mut R) -> Res<i64> {
        let ret = self.real_reader.i64(reader);
        self.bytes_read += size_of::<i64>();
        self.report()?;
        ret
    }

    fn f32<R: Read>(&mut self, reader: &mut R) -> Res<f32> {
        let ret = self.real_reader.f32(reader);
        self.bytes_read += size_of::<f32>();
        self.report()?;
        ret
    }

    fn f64<R: Read>(&mut self, reader: &mut R) -> Res<f64> {
        let ret = self.real_reader.f64(reader);
        self.bytes_read += size_of::<f64>();
        self.report()?;
        ret
    }

    fn u8<R: Read>(&mut self, reader: &mut R) -> Res<u8> {
        let ret = self.real_reader.u8(reader);
        self.bytes_read += size_of::<u8>();
        self.report()?;
        ret
    }

    fn string<R: Read>(&mut self, reader: &mut R) -> Res<String> {
        let len = self.i16(reader)?;
        let len: usize = len.try_into().map_err(|_| {
            NBTError::new(ReadError::SeqLengthViolation(i16::MAX as usize, len as i32))
        })?;

        let mut str_buf = Vec::with_capacity(len.min(1024));
        for i in 0..len {
            str_buf.push(
                self.u8(reader)
                    .map_err(|err| err.prepend(PathPart::Element(i)))?,
            );
        }

        if self.real_reader.type_id() == BigEndian.type_id() {
            let string = from_java_cesu8(&str_buf)
                .map_err(|e| NBTError::new(ReadError::Custom(e.to_string())))?;
            return Ok(string.to_string());
        }

        String::from_utf8(str_buf).map_err(|err| NBTError::new(ReadError::from(err)))
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_object_n(
    bytes: *mut Vec<u8>,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    cancel_flag: *const AtomicBool,
    object: *mut *mut NBTRoot,
    elapsed_millisecs: u64,
    free_memory: u64,
) -> *const c_char {
    if object.is_null() {
        return string_to_ptr_fail_to_null(i18n("Region value not provided"));
    }
    /* We try triple times */
    let mut error_string = vec![];

    match nbt_create_real(
        bytes,
        progress_fn,
        main_klass,
        cancel_flag,
        elapsed_millisecs as u128,
        free_memory,
        BigEndian,
    ) {
        Ok(nbt) => {
            unsafe {
                *object = Box::into_raw(Box::new(nbt));
            }
            null()
        }
        Err(e) => {
            error_string.push(e.to_string());
            match nbt_create_real(
                bytes,
                progress_fn,
                main_klass,
                cancel_flag,
                elapsed_millisecs as u128,
                free_memory,
                LittleEndian,
            ) {
                Ok(nbt) => {
                    unsafe {
                        *object = Box::into_raw(Box::new(nbt));
                    }
                    string_to_ptr_fail_to_null(&get_final_error(error_string))
                }
                Err(e) => {
                    error_string.push(e.to_string());
                    match nbt_create_real(
                        bytes,
                        progress_fn,
                        main_klass,
                        cancel_flag,
                        elapsed_millisecs as u128,
                        free_memory,
                        NetworkLittleEndian,
                    ) {
                        Ok(nbt) => {
                            unsafe {
                                *object = Box::into_raw(Box::new(nbt));
                            }
                            string_to_ptr_fail_to_null(&get_final_error(error_string))
                        }
                        Err(e) => {
                            error_string.push(e.to_string());
                            string_to_ptr_fail_to_null(&get_final_error(error_string))
                        }
                    }
                }
            }
        }
    }
}

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

fn get_final_error(vec: Vec<String>) -> String {
    let mut str = String::new();
    let mut i = 0;
    while i < vec.len() {
        if i == 0 {
            let temp_str = gettext_text(i18n("BigEndian Try's Error Message: "));
            str.push_str(&temp_str);
            str.push_str(&vec[0]);
        }
        if i == 1 {
            let temp_enter = "\n";
            let temp_str = gettext_text(i18n("LittleEndian Try's Error Message: "));
            str.push_str(temp_enter);
            str.push_str(&temp_str);
            str.push_str(&vec[1]);
        }
        if i == 1 {
            let temp_enter = "\n";
            let temp_str = gettext_text(i18n("NetworkLittleEndian Try's Error Message: "));
            str.push_str(temp_enter);
            str.push_str(&temp_str);
            str.push_str(&vec[2]);
        }
        i += 1;
    }
    str
}

fn nbt_create_real<R: Reader + 'static>(
    bytes: *mut Vec<u8>,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    cancel_flag: *const AtomicBool,
    elapsed_millisecs: u128,
    free_memory: u64,
    real_reader: R,
) -> Res<NBTRoot> {
    let uncompressed_bytes = unsafe { &*bytes };
    let reader = ReadStruct::new(
        real_reader,
        progress_fn,
        main_klass,
        uncompressed_bytes.len(),
        elapsed_millisecs as u64,
        free_memory,
        cancel_flag,
    );
    let nbt = NBTRoot::read(uncompressed_bytes.as_slice(), reader)?;

    show_progress(
        progress_fn,
        main_klass,
        100,
        i18n("Reading NBT finish."),
        &String::new(),
    );
    Ok(nbt)
}

#[unsafe(no_mangle)]
pub extern "C" fn file_to_nbt_vec(
    filename: *const c_char,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    fail_message: *mut *mut c_char,
    elapsed_millisecs: u64,
    free_memory: u64,
) -> *mut Vec<(String, TreeValue)> {
    let mut failed = 0;
    let vector = unsafe {
        file_try_uncompress(
            filename,
            progress_fn,
            main_klass,
            &mut failed as *mut c_int,
            null(),
            elapsed_millisecs,
            free_memory,
        )
    };
    if failed == 1 {
        let fail_vector = unsafe { Box::from_raw(vector) };
        let fail_str = unsafe { CString::from_vec_unchecked(*fail_vector).into_raw() };
        if !fail_message.is_null() {
            unsafe { *fail_message = fail_str };
        }
        null_mut()
    } else {
        let mut real_nbt: *mut NBTRoot = null_mut();
        let str = region_get_object_n(
            vector,
            progress_fn,
            main_klass,
            null(),
            &mut real_nbt as *mut *mut NBTRoot,
            elapsed_millisecs,
            free_memory,
        );
        if !real_nbt.is_null() {
            string_free(str.cast_mut());
            unsafe {
                let nbt = &*real_nbt;
                let str = nbt.tag_name.clone();
                let tree = le_nbt_to_tree_value(&nbt.data);
                Box::into_raw(Box::new(vec![(str, tree)]))
            }
        } else {
            unsafe {
                *fail_message = str.cast_mut();
            }
            null_mut()
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn object_free(object: *mut NBTRoot) {
    drop(Box::from(object));
}

#[unsafe(no_mangle)]
pub extern "C" fn tag_free(tag: *mut NBTTag) {
    drop(Box::from(tag));
}

fn le_nbt_compound_to_tree_value(compound: &Compound) -> TreeValue {
    let real_compound = &compound.0;
    let mut ret: Vec<(String, TreeValue)> = vec![];
    let mut i = 0;
    for child_node in real_compound {
        let tree_value = le_nbt_to_tree_value(&child_node.1);
        if child_node.0 == "Schematic" {
            println!("Schematic");
        }
        ret.push((child_node.0.clone(), tree_value));
        i += 1;
    }
    TreeValue::Compound(ret)
}

pub fn vec_u8_to_i8_safest(vec: Vec<u8>) -> Vec<i8> {
    let mut result = Vec::with_capacity(vec.len());
    for byte in vec {
        result.push(byte as i8);
    }
    result
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
