use common_rs::{ProgressFn, helper_struct::HelperStruct};
use std::{ffi::c_void, sync::atomic::AtomicBool};

#[unsafe(no_mangle)]
pub extern "C" fn helper_struct_new(
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    cancel_flag: *const AtomicBool,
    elapsed_millisecs: u64,
    free_memory: u64,
) -> *mut HelperStruct {
    Box::into_raw(Box::new(HelperStruct::new(
        progress_fn,
        main_klass,
        cancel_flag,
        elapsed_millisecs,
        free_memory,
    )))
}

#[unsafe(no_mangle)]
pub extern "C" fn helper_struct_free(helper_struct: *mut HelperStruct) {
    unsafe {
        drop(Box::from_raw(helper_struct));
    }
}
