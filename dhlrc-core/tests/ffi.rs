/*! Exercises the C ABI from Rust, the way a non-Rust frontend would use it. */

use dhlrc_core::ffi::{
    DhlrcConfigView, DhlrcCore, dhlrc_config_view_free, dhlrc_core_apply, dhlrc_core_config_path,
    dhlrc_core_config_view, dhlrc_core_download_object, dhlrc_core_free, dhlrc_core_open,
    dhlrc_core_plugin_get_bool, dhlrc_core_plugin_set_bool, dhlrc_core_plugin_set_use_configured,
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
    assert_eq!(snapshot.memory_limit, 524_288_000); // bytes
    assert_eq!(snapshot.elapsed_milliseconds, 500);
    assert_eq!(snapshot.limit_unit, 1); // MiB
    assert_eq!(snapshot.default_show_option, 0); // Palette
    let base_name = unsafe { CStr::from_ptr(snapshot.base_name) }
        .to_string_lossy()
        .into_owned();
    assert_eq!(base_name, "Converted");
    // The cache directory is resolved in the core, so it is never empty even
    // though the freshly created file leaves it unset.
    let cache_directory = unsafe { CStr::from_ptr(snapshot.cache_directory) }
        .to_string_lossy()
        .into_owned();
    assert!(
        !cache_directory.is_empty(),
        "an unset cache directory should resolve to the platform default"
    );
    dhlrc_config_view_free(view);

    // Watch: the callback runs on the watcher thread and sends us the
    // notification that describes the change.
    extern "C" fn on_change(
        user: *mut c_void,
        level: i32,
        event_id: *const c_char,
        title: *const c_char,
        text: *const c_char,
    ) {
        let tx = unsafe { &*(user as *const mpsc::Sender<(i32, String, String, String)>) };
        let read = |pointer: *const c_char| {
            unsafe { CStr::from_ptr(pointer) }
                .to_string_lossy()
                .into_owned()
        };
        let _ = tx.send((level, read(event_id), read(title), read(text)));
    }
    let (tx, rx) = mpsc::channel::<(i32, String, String, String)>();
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
    let (level, event_id, title, text) = rx
        .recv_timeout(Duration::from_secs(5))
        .expect("the change should arrive");
    assert_eq!(level, 0, "an info notification");
    assert_eq!(event_id, "configReloaded");
    assert!(!title.is_empty());
    assert!(text.contains("config.toml"), "text was {text}");

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
    values.memory_limit = 734_003_200; // bytes: 700 MiB
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
    assert_eq!(unsafe { (*view).memory_limit }, 734_003_200);
    /* A view has no fractional part, so the core parks it in the smallest
     * unit. */
    assert_eq!(unsafe { (*view).limit_unit }, 3); // Bytes
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

    // Nothing configured yet. Reading follows the saved options without
    // asking — a load cannot know which plugin will read the file until it has
    // been decoded — while writing asks once per export, since the format was
    // picked on purpose.
    assert_eq!(
        dhlrc_core_plugin_use_configured(core, type_name.as_ptr(), input),
        1
    );
    // The switch still turns the question back on for reading.
    dhlrc_core_plugin_set_use_configured(core, type_name.as_ptr(), input, false);
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
fn an_older_configuration_stops_asking_before_reading() {
    let dir = temp_dir("migrate");
    // A version 1 file, as the previous build wrote it: every plugin's reading
    // options asked, because `use_configured` started false.
    std::fs::write(
        dir.join("config.toml"),
        "version = 1\n\n[plugins.nbt.input]\nuse_configured = false\n\n[plugins.nbt.output]\nuse_configured = false\n",
    )
    .unwrap();

    let dir_c = CString::new(dir.to_string_lossy().to_string()).unwrap();
    let mut error: *mut c_char = ptr::null_mut();
    let core = dhlrc_core_open(dir_c.as_ptr(), None, ptr::null_mut(), &mut error);
    assert!(!core.is_null());

    let type_name = CString::new("nbt").unwrap();
    let input = 0i32;
    let output = 1i32;
    // Reading: that old default becomes the new one, so a load no longer asks.
    assert_eq!(
        dhlrc_core_plugin_use_configured(core, type_name.as_ptr(), input),
        1
    );
    // Writing still asks, since the format is picked on purpose there.
    assert_eq!(
        dhlrc_core_plugin_use_configured(core, type_name.as_ptr(), output),
        0
    );

    // The migrated version is what gets written back.
    assert_eq!(dhlrc_core_save(core, &mut error), 0);
    let text = std::fs::read_to_string(dir.join("config.toml")).unwrap();
    assert!(text.contains("version = 4"), "config was {text}");

    dhlrc_core_free(core);
    let _ = std::fs::remove_dir_all(&dir);
}

#[test]
fn downloading_an_unknown_version_is_reported_without_the_network() {
    let dir = temp_dir("download");
    let dir_c = CString::new(dir.to_string_lossy().to_string()).unwrap();
    let mut error: *mut c_char = ptr::null_mut();
    let core = dhlrc_core_open(dir_c.as_ptr(), None, ptr::null_mut(), &mut error);
    assert!(!core.is_null());

    let object = CString::new("minecraft/lang/zh_cn.json").unwrap();
    /* Data version -1 is not in the bundled table, so this fails before any
     * request is made (no callbacks, which is also the NULL case). */
    let result = dhlrc_core_download_object(
        core,
        -1,
        object.as_ptr(),
        None,
        None,
        None,
        ptr::null_mut(),
        &mut error,
    );
    assert_eq!(result, -1);
    assert!(!error.is_null(), "an error message should be set");
    let message = unsafe { CStr::from_ptr(error) }
        .to_string_lossy()
        .into_owned();
    assert!(message.contains("-1"), "message was {message}");
    dhlrc_string_free(error);

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
