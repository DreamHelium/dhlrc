/*! Loading the plugin libraries the application runs on.
 *
 * Two kinds live next to the executable and are discovered at startup:
 *
 * - the *region modules* in `region_module/`, which import and export one file
 *   format, and
 * - the *object codecs* in `load_module/`, which turn a file's bytes into an
 *   object the region modules then read.
 *
 * Both are ordinary dynamic libraries with a fixed symbol set (see
 * `dev-doc/plugin.md` and `dev-doc/load_module.md`). This module opens each one
 * once, keeps it loaded and reads its metadata, so a frontend only has to ask
 * for the list and the symbols it wants.
 *
 * A `const char *` a module hands out is a `CString` on the process-wide
 * allocator, so it is freed here the same way `string_free ()` frees one.
 */

use libloading::{Library, Symbol};
use std::ffi::{CStr, CString, c_char, c_void};
use std::path::{Path, PathBuf};
use std::ptr;

/// What a frontend can learn about one module. Every string is owned by the
/// entry and stays valid for as long as the core keeps it; `None` means the
/// module does not declare that value (the symbol is missing or returned null).
pub struct ModuleInfo {
    /// The library this module was opened from.
    pub file: CString,
    /// The unique format name (`region_type ()`); `None` for an object codec.
    pub type_name: Option<CString>,
    /// The file suffix without the dot (`region_file_suffix ()`), if any.
    pub file_suffix: Option<CString>,
    /// The object codec this plugin expects, or what a codec produces
    /// (`region_base_type ()` / `object_base_type ()`).
    pub base_type: Option<CString>,
    /// The (already translated) file dialog filter (`region_file_type ()`).
    pub filter: Option<CString>,
    /// Whether the plugin describes a multi-region format.
    pub is_multi: bool,
    /// The NBT encoding the format requires, as an `ObjectEncoding` value.
    pub encoding: i32,
}

struct Entry {
    /// Kept alive so the resolved symbols stay valid.
    library: Library,
    info: ModuleInfo,
}

/// Every module the core has loaded. They live together so the libraries stay
/// open for as long as a frontend might call into them.
pub struct Modules {
    region: Vec<Entry>,
    load: Vec<Entry>,
}

impl Modules {
    /// Opens every library in `region_dir` and `load_dir`. A directory that is
    /// missing is simply empty. A library that will not open, or that does not
    /// carry the symbol set for its kind, is reported on stderr and skipped.
    pub fn load(region_dir: &Path, load_dir: &Path) -> Self {
        Self {
            region: load_region_modules(region_dir),
            load: load_object_modules(load_dir),
        }
    }

    pub fn region_count(&self) -> usize {
        self.region.len()
    }
    pub fn load_count(&self) -> usize {
        self.load.len()
    }

    pub fn region_info(&self, index: usize) -> Option<&ModuleInfo> {
        self.region.get(index).map(|entry| &entry.info)
    }
    pub fn load_info(&self, index: usize) -> Option<&ModuleInfo> {
        self.load.get(index).map(|entry| &entry.info)
    }

    /// The address of a symbol in region module `index`, or null.
    pub fn region_symbol(&self, index: usize, name: &str) -> *mut c_void {
        match self.region.get(index) {
            Some(entry) => symbol_ptr(&entry.library, name),
            None => ptr::null_mut(),
        }
    }

    /// The address of a symbol in object codec `index`, or null.
    pub fn load_symbol(&self, index: usize, name: &str) -> *mut c_void {
        match self.load.get(index) {
            Some(entry) => symbol_ptr(&entry.library, name),
            None => ptr::null_mut(),
        }
    }
}

/// The two directories next to the running executable.
pub fn default_dirs() -> (PathBuf, PathBuf) {
    let base = std::env::current_exe()
        .ok()
        .and_then(|exe| exe.parent().map(Path::to_path_buf))
        .unwrap_or_else(|| PathBuf::from("."));
    (base.join("region_module"), base.join("load_module"))
}

fn load_region_modules(dir: &Path) -> Vec<Entry> {
    let mut entries = Vec::new();
    for file in library_files(dir) {
        let library = match open_library(&file) {
            Ok(library) => library,
            Err(error) => {
                eprintln!(
                    "dhlrc: could not load region module {}: {error}",
                    file.display()
                );
                continue;
            }
        };
        let Some(type_name) = read_name(&library, b"region_type") else {
            eprintln!(
                "dhlrc: ignoring region module {}: region_type () is missing",
                file.display()
            );
            continue;
        };
        let info = ModuleInfo {
            file: path_cstring(&file),
            type_name: Some(c_cstring(&type_name)),
            file_suffix: read_name(&library, b"region_file_suffix").map(|s| c_cstring(&s)),
            base_type: read_name(&library, b"region_base_type").map(|s| c_cstring(&s)),
            filter: read_name(&library, b"region_file_type").map(|s| c_cstring(&s)),
            is_multi: read_int(&library, b"region_is_multi").unwrap_or(0) != 0,
            encoding: normalise_encoding(read_int(&library, b"region_nbt_encoding").unwrap_or(0)),
        };
        entries.push(Entry { library, info });
    }
    entries
}

fn load_object_modules(dir: &Path) -> Vec<Entry> {
    let mut entries = Vec::new();
    for file in library_files(dir) {
        let library = match open_library(&file) {
            Ok(library) => library,
            Err(error) => {
                eprintln!(
                    "dhlrc: could not load load module {}: {error}",
                    file.display()
                );
                continue;
            }
        };
        let Some(base_type) = read_name(&library, b"object_base_type") else {
            eprintln!(
                "dhlrc: ignoring load module {}: object_base_type () is missing",
                file.display()
            );
            continue;
        };
        if !has_symbol(&library, b"region_get_object") || !has_symbol(&library, b"object_free") {
            eprintln!(
                "dhlrc: ignoring load module {}: region_get_object () or object_free () is missing",
                file.display()
            );
            continue;
        }
        let info = ModuleInfo {
            file: path_cstring(&file),
            type_name: None,
            file_suffix: None,
            base_type: Some(c_cstring(&base_type)),
            filter: None,
            is_multi: false,
            encoding: 0,
        };
        entries.push(Entry { library, info });
    }
    entries
}

/// The files inside `dir`, sorted so the load order does not depend on the
/// filesystem. A missing directory is empty.
fn library_files(dir: &Path) -> Vec<PathBuf> {
    let Ok(entries) = std::fs::read_dir(dir) else {
        return Vec::new();
    };
    let mut files: Vec<PathBuf> = entries
        .filter_map(Result::ok)
        .map(|entry| entry.path())
        .filter(|path| path.is_file())
        .collect();
    files.sort();
    files
}

fn open_library(file: &Path) -> Result<Library, libloading::Error> {
    unsafe { Library::new(file) }
}

/// Whether `library` has a symbol named `name` (without a trailing NUL).
fn has_symbol(library: &Library, name: &[u8]) -> bool {
    type AnyFn = unsafe extern "C" fn();
    unsafe { library.get::<AnyFn>(name) }.is_ok()
}

/// Calls `name () -> const char *` and copies and frees the result.
fn read_name(library: &Library, name: &[u8]) -> Option<String> {
    type NameFn = unsafe extern "C" fn() -> *const c_char;
    let function: Symbol<NameFn> = unsafe { library.get(name) }.ok()?;
    unsafe { take_cstring(function()) }
}

/// Calls `name () -> int32_t`.
fn read_int(library: &Library, name: &[u8]) -> Option<i32> {
    type IntFn = unsafe extern "C" fn() -> i32;
    let function: Symbol<IntFn> = unsafe { library.get(name) }.ok()?;
    Some(unsafe { function() })
}

/// The address of a symbol, as an opaque pointer a frontend casts back to the
/// function's own type.
fn symbol_ptr(library: &Library, name: &str) -> *mut c_void {
    let Ok(name) = CString::new(name) else {
        return ptr::null_mut();
    };
    type AnyFn = unsafe extern "C" fn();
    match unsafe { library.get::<AnyFn>(name.as_bytes()) } {
        Ok(function) => *function as *const () as *mut c_void,
        Err(_) => ptr::null_mut(),
    }
}

/// Copies a string a module handed out and frees it, the way `string_free ()`
/// does. The allocation is on the process-wide allocator the module used.
unsafe fn take_cstring(pointer: *const c_char) -> Option<String> {
    if pointer.is_null() {
        return None;
    }
    let value = unsafe { CStr::from_ptr(pointer) }
        .to_string_lossy()
        .into_owned();
    drop(unsafe { CString::from_raw(pointer as *mut c_char) });
    Some(value)
}

fn c_cstring(value: &str) -> CString {
    CString::new(value).unwrap_or_default()
}

fn path_cstring(path: &Path) -> CString {
    CString::new(path.to_string_lossy().as_bytes()).unwrap_or_default()
}

/// Anything that is not one of the known encodings is treated as "any", so a
/// broken plugin cannot pin decoding to an encoding nothing recognises.
fn normalise_encoding(value: i32) -> i32 {
    match value {
        1 | 2 | 3 => value,
        _ => 0,
    }
}
