use std::ffi::c_int;
use std::ptr::null;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};

#[unsafe(no_mangle)]
pub extern "C" fn cancel_flag_new() -> *const AtomicBool {
    Arc::into_raw(Arc::new(AtomicBool::new(false)))
}

#[unsafe(no_mangle)]
pub extern "C" fn cancel_flag_is_cancelled(ptr: *const AtomicBool) -> c_int {
    if ptr.is_null() {
        return 0;
    }
    let arc = unsafe { Arc::from_raw(ptr) };
    let ret = arc.load(Ordering::SeqCst);
    let _ = Arc::into_raw(arc);
    ret as c_int
}

#[unsafe(no_mangle)]
pub extern "C" fn cancel_flag_cancel(ptr: *const AtomicBool) {
    if ptr.is_null() {
        return;
    }
    let arc = unsafe { Arc::from_raw(ptr) };
    arc.store(true, Ordering::SeqCst);
    let _ = Arc::into_raw(arc);
}

#[unsafe(no_mangle)]
pub extern "C" fn cancel_flag_destroy(ptr: *const AtomicBool) {
    if !ptr.is_null() {
        unsafe {
            Arc::from_raw(ptr);
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn cancel_flag_clone(ptr: *const AtomicBool) -> *const AtomicBool {
    if ptr.is_null() {
        return null();
    }
    let arc = unsafe { Arc::from_raw(ptr) };
    let cloned_arc = arc.clone();
    let _ = Arc::into_raw(arc);
    Arc::into_raw(cloned_arc)
}