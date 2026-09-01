use cesu8::from_java_cesu8;
use common_rs::{
    helper_struct::HelperStruct,
    i18n::i18n,
    util::{cstr_to_str, show_progress, string_to_ptr_fail_to_null},
};
use gettextrs::gettext;
use std::{
    any::Any,
    error::Error,
    ffi::{c_char, c_int},
    io::prelude::Read,
    ptr::null,
    time::Instant,
};
use sysinfo::System;
use zuri_nbt::{
    NBTRoot, NBTTag,
    encoding::{BigEndian, LittleEndian, NetworkLittleEndian},
    err::{NBTError, PathPart, ReadError},
    reader::{Reader, Res},
};

struct ReadStruct<R: Reader> {
    real_reader: R,
    original_bytes: usize,
    bytes_read: usize,
    instant: Instant,
    sys: System,
    helper_struct: HelperStruct,
}

impl<T: Reader> ReadStruct<T> {
    fn new(real_reader: T, original_bytes: usize, helper_struct: &HelperStruct) -> Self {
        Self {
            real_reader,
            original_bytes,
            bytes_read: 0,
            instant: Instant::now(),
            sys: System::new_all(),
            helper_struct: helper_struct.clone(),
        }
    }

    fn report(&mut self) -> Result<(), NBTError<ReadError>> {
        self.helper_struct
            .progress(
                &mut self.sys,
                &mut self.instant,
                (self.bytes_read * 100 / self.original_bytes) as c_int,
                i18n("Loading NBT"),
                i18n("Operation canceled!"),
            )
            .or_else(|e| Err(NBTError::new(ReadError::Custom(e.to_string()))))
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
pub extern "C" fn region_get_object(
    bytes: *mut Vec<u8>,
    object: *mut *mut NBTRoot,
    helper_struct: *mut HelperStruct,
) -> *const c_char {
    if object.is_null() {
        return string_to_ptr_fail_to_null(i18n("Region value not provided"));
    }
    /* We try triple times */
    let mut error_string = vec![];

    unsafe {
        match nbt_create_real(bytes, &*helper_struct, BigEndian) {
            Ok(nbt) => {
                *object = Box::into_raw(Box::new(nbt));
                null()
            }
            Err(e) => {
                error_string.push(e.to_string());
                match nbt_create_real(bytes, &*helper_struct, LittleEndian) {
                    Ok(nbt) => {
                        *object = Box::into_raw(Box::new(nbt));
                        string_to_ptr_fail_to_null(&get_final_error(error_string))
                    }
                    Err(e) => {
                        error_string.push(e.to_string());
                        match nbt_create_real(bytes, &*helper_struct, NetworkLittleEndian) {
                            Ok(nbt) => {
                                *object = Box::into_raw(Box::new(nbt));
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
    helper_struct: &HelperStruct,
    real_reader: R,
) -> Res<NBTRoot> {
    let uncompressed_bytes = unsafe { &*bytes };
    let reader = ReadStruct::new(real_reader, uncompressed_bytes.len(), helper_struct);
    let nbt = NBTRoot::read(uncompressed_bytes.as_slice(), reader)?;

    show_progress(
        helper_struct.progress_fn,
        helper_struct.main_klass,
        100,
        i18n("Reading NBT finish."),
        &String::new(),
    );
    Ok(nbt)
}

#[unsafe(no_mangle)]
pub extern "C" fn object_free(object: *mut NBTRoot) {
    drop(Box::from(object));
}

#[unsafe(no_mangle)]
pub extern "C" fn tag_free(tag: *mut NBTTag) {
    drop(Box::from(tag));
}

#[unsafe(no_mangle)]
pub extern "C" fn nbt_type_to_int(nbt: *const NBTTag) -> c_int {
    unsafe {
        match *nbt {
            NBTTag::Byte(_) => 1,
            NBTTag::Short(_) => 2,
            NBTTag::Int(_) => 3,
            NBTTag::Long(_) => 4,
            NBTTag::Float(_) => 5,
            NBTTag::Double(_) => 6,
            NBTTag::String(_) => 7,
            NBTTag::Compound(_) => 12,
            NBTTag::List(_) => 11,
            NBTTag::ByteArray(_) => 8,
            NBTTag::IntArray(_) => 9,
            NBTTag::LongArray(_) => 10,
        }
    }
}
