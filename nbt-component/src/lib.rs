use cesu8::from_java_cesu8;
use common_rs::{
    helper_struct::HelperStruct,
    i18n::i18n,
    util::{show_progress, string_to_ptr_fail_to_null},
};
use formatx::formatx;
use gettextrs::gettext;
use std::{
    any::Any,
    ffi::{c_char, c_int},
    io::prelude::Read,
    ptr::null,
    time::Instant,
};
use sysinfo::System;
use zuri_nbt::{
    NBTRoot,
    encoding::{BigEndian, LittleEndian, NetworkLittleEndian},
    err::{NBTError, PathPart, ReadError},
    reader::{Reader, Res},
};

/* The NBT encodings a decode can be pinned to. Kept in sync with the
 * `ObjectEncoding` values in `src/loadmodule.h`. */
pub const OBJECT_ENCODING_ANY: c_int = 0;
pub const OBJECT_ENCODING_BIG_ENDIAN: c_int = 1;
pub const OBJECT_ENCODING_LITTLE_ENDIAN: c_int = 2;
pub const OBJECT_ENCODING_NETWORK_LITTLE_ENDIAN: c_int = 3;

/* Optional settings for `region_get_object`. Mirrors `ObjectLoadOptions` in
 * `src/loadmodule.h`. */
#[repr(C)]
pub struct ObjectLoadOptions {
    /* Non-zero: the file must decode as `encoding`. */
    pub strict: c_int,
    /* One of the OBJECT_ENCODING_* values; the encoding `strict` requires and
     * the one tried first. Read only when `strict`. */
    pub encoding: c_int,
}

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
                i18n("The loading operation is cancelled."),
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

/* Runs one encoding over the bytes. `None` means the encoding is not one this
 * codec knows. */
fn decode_once(
    bytes: *mut Vec<u8>,
    helper_struct: *mut HelperStruct,
    encoding: c_int,
) -> Option<Res<NBTRoot>> {
    unsafe {
        match encoding {
            OBJECT_ENCODING_BIG_ENDIAN => Some(nbt_create_real(bytes, &*helper_struct, BigEndian)),
            OBJECT_ENCODING_LITTLE_ENDIAN => {
                Some(nbt_create_real(bytes, &*helper_struct, LittleEndian))
            }
            OBJECT_ENCODING_NETWORK_LITTLE_ENDIAN => {
                Some(nbt_create_real(bytes, &*helper_struct, NetworkLittleEndian))
            }
            _ => None,
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn region_get_object(
    bytes: *mut Vec<u8>,
    object: *mut *mut NBTRoot,
    helper_struct: *mut HelperStruct,
    options: *const ObjectLoadOptions,
) -> *const c_char {
    if object.is_null() {
        return string_to_ptr_fail_to_null(i18n("The region value is not provided."));
    }

    /* `options` is optional. The encoding is never pinned up front: every
     * supported one is tried and the first that parses wins, and the encoding
     * that won is the one the object was read as. A decode that succeeds after
     * an earlier attempt failed is a plain success: the caller gets `null` and
     * an object, never an error, or the host would reject a file that was in
     * fact read. The failed attempts are collected only so that, if nothing
     * parses, the caller can see what was tried.
     *
     * `strict` is checked against the encoding that actually parsed, not by
     * restricting which ones are tried. The requested encoding is tried first,
     * so a file that is in it succeeds as before; a file in another encoding is
     * still read far enough to name the encoding it really is, and the caller is
     * told both what was found and what was expected. */
    let all = [
        OBJECT_ENCODING_BIG_ENDIAN,
        OBJECT_ENCODING_LITTLE_ENDIAN,
        OBJECT_ENCODING_NETWORK_LITTLE_ENDIAN,
    ];
    let expected = match unsafe { options.as_ref() } {
        Some(o) if o.strict != 0 => Some(o.encoding),
        _ => None,
    };
    if let Some(want) = expected {
        if !all.contains(&want) {
            return string_to_ptr_fail_to_null(i18n("Unknown object encoding."));
        }
    }

    /* The requested encoding goes first, so a file that is in it is accepted
     * without trying anything else; the rest follow, so a mismatching file can
     * be named. */
    let order: Vec<c_int> = match expected {
        Some(want) => std::iter::once(want)
            .chain(all.iter().copied().filter(|e| *e != want))
            .collect(),
        None => all.to_vec(),
    };

    let mut failures: Vec<(c_int, String)> = vec![];
    for encoding in order {
        match decode_once(bytes, helper_struct, encoding) {
            Some(Ok(nbt)) => {
                /* The encoding that parsed, compared to the one that was
                 * required. */
                if let Some(want) = expected {
                    if encoding != want {
                        return string_to_ptr_fail_to_null(&encoding_mismatch(encoding, want));
                    }
                }
                unsafe {
                    *object = Box::into_raw(Box::new(nbt));
                }
                return null();
            }
            Some(Err(e)) => failures.push((encoding, e.to_string())),
            None => {}
        }
    }

    string_to_ptr_fail_to_null(&get_final_error(failures))
}

#[unsafe(no_mangle)]
pub extern "C" fn object_base_type() -> *const c_char {
    /* The name a region plugin asks for through `region_base_type()`. */
    string_to_ptr_fail_to_null("NBT")
}

pub fn gettext_text(str: &str) -> String {
    gettext(str)
}

/* The name of an encoding, matching the labels in `encoding_error_label`. */
fn encoding_name(encoding: c_int) -> &'static str {
    match encoding {
        OBJECT_ENCODING_BIG_ENDIAN => "BigEndian",
        OBJECT_ENCODING_LITTLE_ENDIAN => "LittleEndian",
        OBJECT_ENCODING_NETWORK_LITTLE_ENDIAN => "NetworkLittleEndian",
        _ => "Unknown",
    }
}

/* The error for a strict decode: the file parsed as `found`, but `expected` was
 * the encoding the caller required. */
fn encoding_mismatch(found: c_int, expected: c_int) -> String {
    formatx!(
        gettext_text(i18n(
            "The file is in the {} encoding, but the {} encoding is required."
        )),
        encoding_name(found),
        encoding_name(expected)
    )
    .unwrap_or_default()
}

/* The label that introduces one failed attempt. The encoding is carried with
 * each failure, so every attempted encoding is labelled with its own name
 * instead of always "BigEndian". */
fn encoding_error_label(encoding: c_int) -> &'static str {
    match encoding {
        OBJECT_ENCODING_BIG_ENDIAN => "BigEndian Try's Error Message: ",
        OBJECT_ENCODING_LITTLE_ENDIAN => "LittleEndian Try's Error Message: ",
        OBJECT_ENCODING_NETWORK_LITTLE_ENDIAN => "NetworkLittleEndian Try's Error Message: ",
        _ => "Unknown encoding's Error Message: ",
    }
}

/* The message returned only when *no* encoding parsed: it lists why each was
 * rejected, so the user can see what was tried. A decode that succeeds is a
 * plain success and never reaches this. */
fn get_final_error(failures: Vec<(c_int, String)>) -> String {
    let mut str = String::new();
    for (i, (encoding, message)) in failures.iter().enumerate() {
        if i > 0 {
            str.push('\n');
        }
        str.push_str(&gettext_text(encoding_error_label(*encoding)));
        str.push_str(message);
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
        i18n("Reading NBT finished."),
        &String::new(),
    );
    Ok(nbt)
}

#[unsafe(no_mangle)]
pub extern "C" fn object_free(object: *mut NBTRoot) {
    drop(Box::from(object));
}
