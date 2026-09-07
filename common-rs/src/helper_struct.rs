use crate::{ProgressFn, my_error::MyError, util::finish_oom, util::show_progress};
use std::{
    error::Error,
    ffi::{c_int, c_void},
    sync::{
        Arc,
        atomic::{AtomicBool, Ordering},
    },
    time::Instant,
};
use sysinfo::System;

#[derive(Clone)]
pub struct HelperStruct {
    pub progress_fn: ProgressFn,
    pub main_klass: *mut c_void,
    cancel_flag: *const AtomicBool,
    pub elapsed_millisecs: u64,
    free_memory: u64,
}

fn cancel_flag_is_cancelled(ptr: *const AtomicBool) -> c_int {
    if ptr.is_null() {
        return 0;
    }
    let arc = unsafe { Arc::from_raw(ptr) };
    let ret = arc.load(Ordering::SeqCst);
    let _ = Arc::into_raw(arc);
    ret as c_int
}

impl HelperStruct {
    pub fn new(
        progress_fn: ProgressFn,
        main_klass: *mut c_void,
        cancel_flag: *const AtomicBool,
        elapsed_millisecs: u64,
        free_memory: u64,
    ) -> Self {
        Self {
            progress_fn,
            main_klass,
            cancel_flag,
            elapsed_millisecs,
            free_memory,
        }
    }

    pub fn progress(
        &self,
        sys: &mut System,
        instant: &mut Instant,
        percentage: c_int,
        str: &str,
        cancel_msg: &str,
    ) -> Result<(), Box<dyn Error>> {
        self.get_cancel_error(cancel_msg)?;
        if instant.elapsed().as_millis() >= self.elapsed_millisecs as u128 {
            self.instant_progress(sys, instant, percentage, str)?
        };
        Ok(())
    }

    pub fn instant_progress(
        &self,
        sys: &mut System,
        instant: &mut Instant,
        percentage: c_int,
        str: &str,
    ) -> Result<(), Box<dyn Error>> {
        finish_oom(sys, self.free_memory)?;
        show_progress(self.progress_fn, self.main_klass, percentage, str, "");
        *instant = Instant::now();
        Ok(())
    }

    pub fn get_cancel_error(&self, cancel_msg: &str) -> Result<(), Box<dyn Error>> {
        if cancel_flag_is_cancelled((self.cancel_flag)) == 1 {
            return Err(Box::new(MyError {
                msg: cancel_msg.to_string(),
            }));
        }
        Ok(())
    }
}
