/*! Exercises the C ABI from Rust, the way a non-Rust frontend would use it. */

use dhlrc_core::ffi::{
    DhlrcConfigView, DhlrcCore, dhlrc_config_view_free, dhlrc_core_apply, dhlrc_core_config_path,
    dhlrc_core_config_view, dhlrc_core_free, dhlrc_core_open, dhlrc_core_plugin_get_bool,
    dhlrc_core_plugin_set_bool, dhlrc_core_plugin_set_use_configured,
    dhlrc_core_plugin_use_configured, dhlrc_core_save, dhlrc_core_watch_start,
    dhlrc_core_watch_stop, dhlrc_string_free,
};
use std::ffi::{CStr, CString, c_char, c_void};
use std::path::PathBuf;
use std::ptr;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::mpsc;
use std::time::{Duration, SystemTime, UNIX_EPOCH};

fn temp_dir(tag: &str) -> PathBuf {
    let nanos = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_nanos();
    let dir = std::env::temp_dir().join(format!("dhlrc-core-ffi-{tag}-{nanos}"));
    std::fs::create_dir_all(&dir).unwrap();
    dir
}

#[test]
fn open_read_watch_round_trip() {
    let dir = temp_dir("roundtrip");
    let dir_c = CString::new(dir.to_string_lossy().to_string()).unwrap();

    // The notify callback counts through the user pointer, so no global state.
    extern "C" fn notify(
        user: *mut c_void,
        _level: i32,
        _event_id: *const c_char,
        _title: *const c_char,
        _text: *const c_char,
    ) {
        let count = unsafe { &*(user as *const AtomicUsize) };
        count.fetch_add(1, Ordering::Relaxed);
    }

    let count = Box::into_raw(Box::new(AtomicUsize::new(0)));
    let mut error: *mut c_char = ptr::null_mut();
    let core: *mut DhlrcCore = dhlrc_core_open(
        dir_c.as_ptr(),
        Some(notify),
        count as *mut c_void,
        &mut error,
    );
    assert!(!core.is_null(), "open should succeed");
    assert!(error.is_null(), "no error expected on a fresh open");
    // A fresh file is created, which is one notification.
    assert_eq!(unsafe { (*count).load(Ordering::Relaxed) }, 1);

    let path = unsafe { CStr::from_ptr(dhlrc_core_config_path(core)) }
        .to_string_lossy()
        .into_owned();
    assert!(path.ends_with("config.toml"), "path was {path}");

    let view: *mut DhlrcConfigView = dhlrc_core_config_view(core);
    assert!(!view.is_null());
    let snapshot = unsafe { &*view };
    assert_eq!(snapshot.memory_limit, 524_288_000);
    assert_eq!(snapshot.elapsed_milliseconds, 500);
    assert_eq!(snapshot.limit_unit, 0); // GiB
    assert_eq!(snapshot.default_show_option, 0); // Palette
    let base_name = unsafe { CStr::from_ptr(snapshot.base_name) }
        .to_string_lossy()
        .into_owned();
    assert_eq!(base_name, "Converted");
    dhlrc_config_view_free(view);

    // Watch: the callback runs on the watcher thread and just pings us.
    extern "C" fn on_change(user: *mut c_void) {
        let tx = unsafe { &*(user as *const mpsc::Sender<()>) };
        let _ = tx.send(());
    }
    let (tx, rx) = mpsc::channel::<()>();
    let tx_box = Box::into_raw(Box::new(tx));
    assert_eq!(
        dhlrc_core_watch_start(core, Some(on_change), tx_box as *mut c_void),
        0
    );

    // An edit made from outside.
    std::fs::write(
        dir.join("config.toml"),
        "version = 1\n\n[general]\nelapsed_milliseconds = 1234\n",
    )
    .unwrap();
    rx.recv_timeout(Duration::from_secs(5))
        .expect("the change should arrive");

    let view = dhlrc_core_config_view(core);
    assert!(!view.is_null());
    assert_eq!(unsafe { (*view).elapsed_milliseconds }, 1234);
    dhlrc_config_view_free(view);

    dhlrc_core_watch_stop(core);
    // Safe now that the watch thread has been joined.
    drop(unsafe { Box::from_raw(tx_box) });
    dhlrc_core_free(core);
    drop(unsafe { Box::from_raw(count) });
    let _ = std::fs::remove_dir_all(&dir);
}

#[test]
fn apply_and_save_round_trips() {
    let dir = temp_dir("apply");
    let dir_c = CString::new(dir.to_string_lossy().to_string()).unwrap();
    let mut error: *mut c_char = ptr::null_mut();
    let core = dhlrc_core_open(dir_c.as_ptr(), None, ptr::null_mut(), &mut error);
    assert!(!core.is_null());

    // A view-shaped set of values; NULL strings mean "unset".
    let mut values: DhlrcConfigView = unsafe { std::mem::zeroed() };
    values.memory_limit = 524_288_000;
    values.elapsed_milliseconds = 999;
    values.select_all_regions_in_loading = true;
    assert_eq!(dhlrc_core_apply(core, &values, &mut error), 0);
    assert_eq!(dhlrc_core_save(core, &mut error), 0);
    dhlrc_core_free(core);

    // Re-open: the saved values are read back.
    let core = dhlrc_core_open(dir_c.as_ptr(), None, ptr::null_mut(), &mut error);
    assert!(!core.is_null());
    let view = dhlrc_core_config_view(core);
    assert_eq!(unsafe { (*view).elapsed_milliseconds }, 999);
    assert!(unsafe { (*view).select_all_regions_in_loading });
    dhlrc_config_view_free(view);
    dhlrc_core_free(core);
    let _ = std::fs::remove_dir_all(&dir);
}

#[test]
fn plugin_options_round_trip() {
    let dir = temp_dir("plugin");
    let dir_c = CString::new(dir.to_string_lossy().to_string()).unwrap();
    let mut error: *mut c_char = ptr::null_mut();
    let core = dhlrc_core_open(dir_c.as_ptr(), None, ptr::null_mut(), &mut error);
    assert!(!core.is_null());

    let type_name = CString::new("nbt").unwrap();
    let key = CString::new("some_option").unwrap();
    let input = 0i32;
    let output = 1i32;

    // Nothing configured to start with.
    assert_eq!(
        dhlrc_core_plugin_use_configured(core, type_name.as_ptr(), input),
        0
    );
    let mut out = false;
    assert_eq!(
        dhlrc_core_plugin_get_bool(core, type_name.as_ptr(), input, key.as_ptr(), &mut out),
        0
    );

    dhlrc_core_plugin_set_use_configured(core, type_name.as_ptr(), input, true);
    dhlrc_core_plugin_set_bool(core, type_name.as_ptr(), input, key.as_ptr(), true);
    assert_eq!(
        dhlrc_core_plugin_use_configured(core, type_name.as_ptr(), input),
        1
    );
    assert_eq!(
        dhlrc_core_plugin_get_bool(core, type_name.as_ptr(), input, key.as_ptr(), &mut out),
        1
    );
    assert!(out);
    // The two directions are independent.
    assert_eq!(
        dhlrc_core_plugin_use_configured(core, type_name.as_ptr(), output),
        0
    );

    // Persist and re-open.
    assert_eq!(dhlrc_core_save(core, &mut error), 0);
    dhlrc_core_free(core);

    let core = dhlrc_core_open(dir_c.as_ptr(), None, ptr::null_mut(), &mut error);
    assert_eq!(
        dhlrc_core_plugin_use_configured(core, type_name.as_ptr(), input),
        1
    );
    assert_eq!(
        dhlrc_core_plugin_get_bool(core, type_name.as_ptr(), input, key.as_ptr(), &mut out),
        1
    );
    assert!(out);
    dhlrc_core_free(core);
    let _ = std::fs::remove_dir_all(&dir);
}

#[test]
fn an_unusable_directory_is_reported() {
    // Make a file, then ask for the config *inside* it, which cannot be created.
    let base = temp_dir("notdir");
    let blocker = base.join("config.toml");
    std::fs::write(&blocker, "version = 1\n").unwrap();
    let bad = blocker.join("nested");

    let bad_c = CString::new(bad.to_string_lossy().to_string()).unwrap();
    let mut error: *mut c_char = ptr::null_mut();
    let core = dhlrc_core_open(bad_c.as_ptr(), None, ptr::null_mut(), &mut error);
    assert!(core.is_null(), "open should fail");
    assert!(!error.is_null(), "an error message should be set");
    dhlrc_string_free(error);
    let _ = std::fs::remove_dir_all(&base);
}
