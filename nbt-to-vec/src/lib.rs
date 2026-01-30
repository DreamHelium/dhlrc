use crab_nbt_ext::{TreeValue, convert_nbt_to_vec, convert_vec_to_nbt, nbt_create_real};
use std::ffi::{CStr, c_char, c_int, c_void};
use std::fs::File;
use std::io::Write;
use std::ptr::{null, null_mut};
use std::sync::atomic::AtomicBool;

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

#[unsafe(no_mangle)]
pub extern "C" fn nbt_vec_to_file(vec: *const Vec<(String, TreeValue)>, filename: *const c_char, from_file : c_int) {
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
    unsafe {
        let vector = file_try_uncompress(
            filename,
            progress_fn,
            main_klass,
            &mut failed as *mut c_int,
            null(),
        );
        if failed == 1 {
            null_mut()
        } else {
            match nbt_create_real(vector, progress_fn, main_klass, null()) {
                Ok(nbt) => {
                    vec_free(vector);
                    let mid_val = convert_nbt_to_vec(&nbt.root_tag);
                    let tree_value = TreeValue::Compound(mid_val);
                    let nbt_string = nbt.name;
                    let ret = vec![(nbt_string, tree_value)];
                    Box::into_raw(Box::new(ret))
                }
                Err(_) => {
                    vec_free(vector);
                    null_mut()
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
