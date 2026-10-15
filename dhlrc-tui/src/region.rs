/*! Loading a region file with no GUI.
 *
 * The Qt frontend's import pipeline (`DhLoadJob::start`) is reproduced here:
 * read and decompress the file through `libregion_rs`, decode it with the first
 * object codec that accepts it, then hand the object to a region module whose
 * `region_base_type ()` names that codec. Every value then comes back through
 * the same `libregion_rs` getters the Qt frontend uses, so a plugin cannot tell
 * the two apart.
 *
 * One ordering detail is load-bearing: the region modules import `region_new`
 * from `libregion_rs`, so that library is opened first and `RTLD_GLOBAL`. The
 * modules then resolve against the one copy the process is already using, and
 * a string allocated by a plugin is released by the same library that would
 * release it under the Qt frontend.
 */

use std::ffi::{CStr, CString, c_char, c_int, c_void};
use std::fmt::{self, Display, Formatter};
use std::path::{Path, PathBuf};
use std::ptr;

use dhlrc_core::modules::Modules;
use dhlrc_render::scene::{VoxelBlock, Voxels};
use dhlrc_resource::Identifier;

/// Why a file could not be turned into a region.
#[derive(Debug)]
pub struct Error(String);

impl Error {
    fn new(message: impl Into<String>) -> Self {
        Self(message.into())
    }
}

impl Display for Error {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        formatter.write_str(&self.0)
    }
}

impl std::error::Error for Error {}

/// The result of a load.
pub type Result<T> = std::result::Result<T, Error>;

/// Where the modules live, and which region inside the file to read.
pub struct Options<'a> {
    /// The directory of region modules (`region_module/`).
    pub region_dir: &'a Path,
    /// The directory of object codecs (`load_module/`).
    pub load_dir: &'a Path,
    /// The `libregion_rs` to open, or `None` to look beside the executable and
    /// the modules.
    pub region_lib: Option<&'a Path>,
    /// An index or a name for a multi-region file; the first region when unset.
    pub region: Option<&'a str>,
}

/// A loaded region, ready to be drawn.
pub struct Region {
    /// The module libraries stay open for as long as the region does; the
    /// region was allocated by one of them.
    _modules: Modules,
    lib: RegionLib,
    raw: *mut c_void,
    size: [i32; 3],
    data_version: u32,
    /// Every palette entry, resolved once: the renderer asks for an entry at
    /// most once, but reading one allocates a string that has to be freed, and
    /// doing that up front keeps the raw region out of the `Voxels` methods.
    palette: Vec<Option<VoxelBlock>>,
}

impl Region {
    /// Loads `file`, taking the region the options select.
    pub fn load(file: &Path, options: &Options<'_>) -> Result<Self> {
        let lib = RegionLib::open(options.region_lib, options.region_dir)?;
        /* Opened after `libregion_rs`, so their `region_new` binds to it. */
        let modules = Modules::load(options.region_dir, options.load_dir);
        /* The default directories sit next to the binary, which a `cargo run`
         * does not populate; saying which two came up empty is more use than
         * the decode failure that would follow. */
        if modules.region_count() == 0 && modules.load_count() == 0 {
            return Err(Error::new(format!(
                "no modules found in {} and {}; pass --region-dir/--load-dir",
                options.region_dir.display(),
                options.load_dir.display()
            )));
        }

        let helper = Helper::new(&lib);
        let bytes = lib.read(file, helper.pointer)?;
        let object = decode(&modules, &bytes, helper.pointer, &lib)?;
        let raw = create(
            &modules,
            &object,
            file,
            options.region,
            helper.pointer,
            &lib,
        )?;
        let (size, palette) = read_palette(&lib, raw);
        let data_version = unsafe { (lib.region_get_data_version)(raw) };

        Ok(Self {
            _modules: modules,
            lib,
            raw,
            size,
            data_version,
            palette,
        })
    }
}

impl Region {
    /// The data version the file was written for (`4189` for 1.21.4), which is
    /// what names the version whose client jar the models and textures are in.
    pub fn data_version(&self) -> u32 {
        self.data_version
    }
}

impl Drop for Region {
    fn drop(&mut self) {
        unsafe { (self.lib.region_free)(self.raw) };
    }
}

impl Voxels for Region {
    fn size(&self) -> [i32; 3] {
        self.size
    }

    fn palette_len(&self) -> usize {
        self.palette.len()
    }

    fn palette(&self, index: usize) -> Option<VoxelBlock> {
        self.palette.get(index).cloned().flatten()
    }

    fn block(&self, x: i32, y: i32, z: i32) -> Option<usize> {
        let [width, height, depth] = self.size;
        if x < 0 || y < 0 || z < 0 || x >= width || y >= height || z >= depth {
            return None;
        }
        /* The layout the whole project uses: `x + width * z + width * depth * y`. */
        let index = (x + width * z + width * depth * y) as usize;
        let id = unsafe { (self.lib.region_get_block_id_by_index)(self.raw, index) };
        Some(id as usize)
    }
}

/// The `libregion_rs` functions this module calls.
///
/// The crate is linked as a `cdylib` and the Qt frontend links it directly, so
/// every symbol is looked up here rather than declared `extern "C"`: that is
/// what lets the same file beside the executable be shared with the plugins.
struct RegionLib {
    /// Kept open for the run; the pointers below point into it.
    _library: Library,
    file_try_uncompress: FileTryUncompress,
    vec_free: VecFree,
    vec_to_cstr: VecToCStr,
    string_free: StringFree,
    helper_struct_new: HelperStructNew,
    helper_struct_free: HelperStructFree,
    region_get_x: RegionGetSize,
    region_get_y: RegionGetSize,
    region_get_z: RegionGetSize,
    region_get_palette_len: RegionGetPaletteLen,
    region_get_palette_id_name: RegionGetPaletteName,
    region_get_palette_property_len: RegionGetPropertyLen,
    region_get_palette_property_name: RegionGetPropertyPart,
    region_get_palette_property_data: RegionGetPropertyPart,
    region_get_block_id_by_index: RegionGetBlock,
    region_get_data_version: RegionGetDataVersion,
    region_free: RegionFree,
}

impl RegionLib {
    /// Opens the library the modules import `region_new` from.
    fn open(explicit: Option<&Path>, region_dir: &Path) -> Result<Self> {
        let path = find_library(explicit, region_dir)?;
        let library = open_process_wide(&path)
            .map_err(|error| Error::new(format!("could not load {}: {error}", path.display())))?;
        unsafe {
            Ok(Self {
                file_try_uncompress: symbol(&library, &path, b"file_try_uncompress\0")?,
                vec_free: symbol(&library, &path, b"vec_free\0")?,
                vec_to_cstr: symbol(&library, &path, b"vec_to_cstr\0")?,
                string_free: symbol(&library, &path, b"string_free\0")?,
                helper_struct_new: symbol(&library, &path, b"helper_struct_new\0")?,
                helper_struct_free: symbol(&library, &path, b"helper_struct_free\0")?,
                region_get_x: symbol(&library, &path, b"region_get_x\0")?,
                region_get_y: symbol(&library, &path, b"region_get_y\0")?,
                region_get_z: symbol(&library, &path, b"region_get_z\0")?,
                region_get_palette_len: { symbol(&library, &path, b"region_get_palette_len\0")? },
                region_get_palette_id_name: {
                    symbol(&library, &path, b"region_get_palette_id_name\0")?
                },
                region_get_palette_property_len: {
                    symbol(&library, &path, b"region_get_palette_property_len\0")?
                },
                region_get_palette_property_name: {
                    symbol(&library, &path, b"region_get_palette_property_name\0")?
                },
                region_get_palette_property_data: {
                    symbol(&library, &path, b"region_get_palette_property_data\0")?
                },
                region_get_block_id_by_index: {
                    symbol(&library, &path, b"region_get_block_id_by_index\0")?
                },
                region_get_data_version: {
                    symbol(&library, &path, b"region_get_data_version\0")?
                },
                region_free: symbol(&library, &path, b"region_free\0")?,
                _library: library,
            })
        }
    }

    /// Reads `file` into memory, transparently decompressing gzip and zlib.
    ///
    /// A failure is reported by the library as a vector holding the message;
    /// it is copied out and released, and a success is kept and released by the
    /// returned guard however the load ends.
    fn read(&self, file: &Path, helper: *mut c_void) -> Result<Bytes> {
        let name = CString::new(file.to_string_lossy().as_bytes())
            .map_err(|_| Error::new(format!("{} holds a NUL byte", file.display())))?;
        let mut failed: c_int = 0;
        let pointer = unsafe { (self.file_try_uncompress)(name.as_ptr(), helper, &mut failed) };
        if pointer.is_null() {
            return Err(Error::new(format!("{} could not be read", file.display())));
        }

        if failed != 0 {
            let message = unsafe { (self.vec_to_cstr)(pointer) };
            let text = unsafe { take_string(message, self.string_free) }
                .unwrap_or_else(|| format!("{} could not be read", file.display()));
            return Err(Error::new(text));
        }

        Ok(Bytes {
            pointer,
            free: self.vec_free,
        })
    }
}

/// A file's decompressed bytes, released with `vec_free` on every path.
struct Bytes {
    pointer: *mut c_void,
    free: VecFree,
}

impl Drop for Bytes {
    fn drop(&mut self) {
        unsafe { (self.free)(self.pointer) };
    }
}

/// The progress and cancel object a plugin reports through.
///
/// Nothing is reported and nothing can be cancelled: the progress function is
/// absent and the elapsed-time threshold is `u64::MAX`. A headless load has no
/// row to update, and the memory check is disabled by a limit of zero.
struct Helper {
    pointer: *mut c_void,
    free: HelperStructFree,
}

impl Helper {
    fn new(lib: &RegionLib) -> Self {
        let pointer =
            unsafe { (lib.helper_struct_new)(None, ptr::null_mut(), ptr::null(), u64::MAX, 0) };
        Self {
            pointer,
            free: lib.helper_struct_free,
        }
    }
}

impl Drop for Helper {
    fn drop(&mut self) {
        unsafe { (self.free)(self.pointer) };
    }
}

/// The object a codec decoded the file into, released with that codec's own
/// `object_free`.
struct Object {
    pointer: *mut c_void,
    free: ObjectFree,
    /// The `object_base_type ()` of the codec, which a region module has to
    /// name to be offered the object.
    base_type: String,
}

impl Drop for Object {
    fn drop(&mut self) {
        unsafe { (self.free)(self.pointer) };
    }
}

/// Decodes `bytes` with the first object codec that accepts them.
fn decode(
    modules: &Modules,
    bytes: &Bytes,
    helper: *mut c_void,
    lib: &RegionLib,
) -> Result<Object> {
    let mut errors: Vec<String> = Vec::new();
    for index in 0..modules.load_count() {
        let get_object =
            unsafe { function::<GetObject>(modules.load_symbol(index, "region_get_object")) };
        let object_free =
            unsafe { function::<ObjectFree>(modules.load_symbol(index, "object_free")) };
        let (Some(get_object), Some(object_free)) = (get_object, object_free) else {
            continue;
        };

        let mut object: *mut c_void = ptr::null_mut();
        /* A null options pointer means "try every encoding", exactly as the Qt
         * loader does when strict matching is off. */
        let error = unsafe { get_object(bytes.pointer, &mut object, helper, ptr::null()) };
        if error.is_null() {
            if object.is_null() {
                continue;
            }
            let base_type = codec_name(modules, index).unwrap_or_default();
            return Ok(Object {
                pointer: object,
                free: object_free,
                base_type,
            });
        }

        let message = unsafe { take_string(error, lib.string_free) };
        errors.push(match codec_name(modules, index) {
            Some(name) => format!("{name}: {}", message.unwrap_or_default()),
            None => message.unwrap_or_else(|| "the codec failed".to_owned()),
        });
    }

    if errors.is_empty() {
        return Err(Error::new(
            "no object codec is installed (load_module/ is empty)",
        ));
    }
    Err(Error::new(errors.join("; ")))
}

/// Creates one region from `object`.
///
/// The module the file's suffix names is the format the file claims to be, so
/// it is the only one tried; a file with no suffix, or one no module claims, is
/// offered to every module that understands the codec, in the order the core
/// loaded them.
fn create(
    modules: &Modules,
    object: &Object,
    file: &Path,
    requested: Option<&str>,
    helper: *mut c_void,
    lib: &RegionLib,
) -> Result<*mut c_void> {
    let mut errors: Vec<String> = Vec::new();
    let mut candidates = candidates(modules, &object.base_type, file);
    if candidates.is_empty() {
        return Err(Error::new(format!(
            "no region module reads the codec \"{}\"",
            object.base_type
        )));
    }
    /* The file's suffix named a module: that is the format the file claims to
     * be, so it is the only one tried — the Qt loader's default, which does not
     * fall back to another format after the named one fails. */
    if candidates
        .first()
        .is_some_and(|&index| claims_suffix(modules, index, file))
    {
        candidates.truncate(1);
    }

    for index in candidates {
        let Some(info) = modules.region_info(index) else {
            continue;
        };
        let type_name = info
            .type_name
            .as_ref()
            .map(|name| name.to_string_lossy().into_owned())
            .unwrap_or_else(|| index.to_string());

        let result = if info.is_multi {
            create_multi(modules, index, object.pointer, requested, helper, lib)
        } else if requested.is_some() {
            Err(Error::new(
                "the file holds a single region, so --region does not apply",
            ))
        } else {
            create_single(modules, index, object.pointer, helper, lib)
        };

        match result {
            Ok(region) => return Ok(region),
            Err(error) => errors.push(format!("{type_name}: {error}")),
        }
    }

    Err(Error::new(errors.join("; ")))
}

/// Creates the region of a single-region module.
fn create_single(
    modules: &Modules,
    index: usize,
    object: *mut c_void,
    helper: *mut c_void,
    lib: &RegionLib,
) -> Result<*mut c_void> {
    let Some(load) = (unsafe {
        function::<CreateSingle>(modules.region_symbol(index, "region_create_from_file"))
    }) else {
        return Err(Error::new("region_create_from_file () is missing"));
    };

    let mut region: *mut c_void = ptr::null_mut();
    /* A null config: the importer has no per-plugin options to offer, so the
     * plugin's own defaults are the right ones. */
    let error = unsafe { load(object, &mut region, helper, ptr::null_mut()) };
    region_from(error, region, lib)
}

/// Creates the selected region of a multi-region module.
fn create_multi(
    modules: &Modules,
    index: usize,
    object: *mut c_void,
    requested: Option<&str>,
    helper: *mut c_void,
    lib: &RegionLib,
) -> Result<*mut c_void> {
    let Some(num) = (unsafe { function::<RegionNum>(modules.region_symbol(index, "region_num")) })
    else {
        return Err(Error::new("region_num () is missing"));
    };
    let Some(name_at) =
        (unsafe { function::<RegionName>(modules.region_symbol(index, "region_name_index")) })
    else {
        return Err(Error::new("region_name_index () is missing"));
    };
    let Some(load) = (unsafe {
        function::<CreateMulti>(modules.region_symbol(index, "region_create_from_file_as_index"))
    }) else {
        return Err(Error::new("region_create_from_file_as_index () is missing"));
    };

    let count = unsafe { num(object) };
    if count <= 0 {
        return Err(Error::new("the file holds no region"));
    }

    let names: Vec<String> = (0..count)
        .map(|at| unsafe { take_string(name_at(object, at), lib.string_free) }.unwrap_or_default())
        .collect();

    let chosen = match requested {
        None => 0,
        Some(value) => match value.parse::<usize>() {
            /* An index in range wins over a region that happens to be named
             * like a number, which is what `--region 0` should mean. */
            Ok(at) if at < count as usize => at,
            _ => names.iter().position(|name| name == value).ok_or_else(|| {
                Error::new(format!(
                    "no region is called \"{value}\" (the file holds {})",
                    list_names(&names)
                ))
            })?,
        },
    };

    let mut region: *mut c_void = ptr::null_mut();
    let error = unsafe { load(object, &mut region, chosen as i32, helper, ptr::null_mut()) };
    region_from(error, region, lib)
}

/// A module call's `const char *` result turned into a region or an error.
fn region_from(error: *const c_char, region: *mut c_void, lib: &RegionLib) -> Result<*mut c_void> {
    if !error.is_null() {
        return Err(Error::new(
            unsafe { take_string(error, lib.string_free) }
                .unwrap_or_else(|| "the module reported an error".to_owned()),
        ));
    }
    if region.is_null() {
        return Err(Error::new("the module returned no region"));
    }
    Ok(region)
}

/// The region modules that understand `base_type`, the one the file's suffix
/// names first. The sort is stable, so the modules that do not claim the suffix
/// keep the order the core loaded them in.
fn candidates(modules: &Modules, base_type: &str, file: &Path) -> Vec<usize> {
    let mut found: Vec<usize> = (0..modules.region_count())
        .filter(|&index| {
            modules
                .region_info(index)
                .and_then(|info| info.base_type.as_ref())
                .is_some_and(|base| base.to_string_lossy() == base_type)
        })
        .collect();

    found.sort_by_key(|&index| !claims_suffix(modules, index, file));
    found
}

/// Whether module `index` names the file's extension.
fn claims_suffix(modules: &Modules, index: usize, file: &Path) -> bool {
    let Some(extension) = file.extension().and_then(|value| value.to_str()) else {
        return false;
    };
    modules
        .region_info(index)
        .and_then(|info| info.file_suffix.as_ref())
        .is_some_and(|suffix| suffix.to_string_lossy() == extension)
}

/// A codec's `object_base_type ()`, or `None` when it does not declare one.
fn codec_name(modules: &Modules, index: usize) -> Option<String> {
    modules
        .load_info(index)
        .and_then(|info| info.base_type.as_ref())
        .map(|name| name.to_string_lossy().into_owned())
}

/// Reads a region's size and palette through `libregion_rs`, copying every
/// palette string out and freeing it as it goes.
fn read_palette(lib: &RegionLib, raw: *mut c_void) -> ([i32; 3], Vec<Option<VoxelBlock>>) {
    let size = [
        unsafe { (lib.region_get_x)(raw) },
        unsafe { (lib.region_get_y)(raw) },
        unsafe { (lib.region_get_z)(raw) },
    ];

    let len = unsafe { (lib.region_get_palette_len)(raw) };
    let mut palette = Vec::with_capacity(len);
    for index in 0..len {
        let name = unsafe {
            take_string(
                (lib.region_get_palette_id_name)(raw, index),
                lib.string_free,
            )
        };
        /* An entry with no name draws nothing, like air. */
        let Some(name) = name.filter(|name| !name.is_empty()) else {
            palette.push(None);
            continue;
        };

        let property_len = unsafe { (lib.region_get_palette_property_len)(raw, index) };
        let mut properties = String::new();
        for at in 0..property_len {
            let key = unsafe {
                take_string(
                    (lib.region_get_palette_property_name)(raw, index, at),
                    lib.string_free,
                )
            };
            let value = unsafe {
                take_string(
                    (lib.region_get_palette_property_data)(raw, index, at),
                    lib.string_free,
                )
            };
            if let (Some(key), Some(value)) = (key, value) {
                if !properties.is_empty() {
                    properties.push(',');
                }
                properties.push_str(&key);
                properties.push('=');
                properties.push_str(&value);
            }
        }

        palette.push(Some(VoxelBlock {
            name: Identifier::parse(&name),
            properties,
        }));
    }

    (size, palette)
}

/// A name list for a "no such region" message, capped so a huge file does not
/// print a thousand names.
fn list_names(names: &[String]) -> String {
    let shown: Vec<&str> = names.iter().take(8).map(String::as_str).collect();
    if names.len() > shown.len() {
        format!("{}, …", shown.join(", "))
    } else {
        shown.join(", ")
    }
}

/// Copies a string a module handed out and frees it the way `string_free ()`
/// does. A null pointer is `None`, which several getters use for "not set".
unsafe fn take_string(pointer: *const c_char, free: StringFree) -> Option<String> {
    if pointer.is_null() {
        return None;
    }
    let value = unsafe { CStr::from_ptr(pointer) }
        .to_string_lossy()
        .into_owned();
    unsafe { free(pointer as *mut c_char) };
    Some(value)
}

/// A symbol address as the function it points at. The module libraries stay
/// open for the whole load, so the address stays valid.
unsafe fn function<F>(address: *mut c_void) -> Option<F> {
    if address.is_null() {
        None
    } else {
        Some(unsafe { std::mem::transmute_copy::<*mut c_void, F>(&address) })
    }
}

/// One symbol of `library`, copied out as a plain function pointer.
unsafe fn symbol<F: Copy>(library: &Library, path: &Path, name: &[u8]) -> Result<F> {
    let symbol = unsafe { library.get::<F>(name) }.map_err(|error| {
        Error::new(format!(
            "{}: missing symbol {} ({error})",
            path.display(),
            String::from_utf8_lossy(&name[..name.len().saturating_sub(1)])
        ))
    })?;
    Ok(*symbol)
}

/// The file name `libregion_rs` has on this platform.
fn library_name() -> &'static str {
    if cfg!(target_os = "windows") {
        "region_rs.dll"
    } else if cfg!(target_os = "macos") {
        "libregion_rs.dylib"
    } else {
        "libregion_rs.so"
    }
}

/// Where `libregion_rs` is: the path the user gave, or the first of the places
/// the frontends keep it — next to the executable, then beside the region
/// modules (the layout CMake stages), then the current directory.
fn find_library(explicit: Option<&Path>, region_dir: &Path) -> Result<PathBuf> {
    if let Some(path) = explicit {
        return path
            .is_file()
            .then(|| path.to_path_buf())
            .ok_or_else(|| Error::new(format!("{} is not a file", path.display())));
    }

    let name = library_name();
    let mut candidates: Vec<PathBuf> = Vec::new();
    if let Some(exe) = std::env::current_exe()
        .ok()
        .and_then(|exe| exe.parent().map(Path::to_path_buf))
    {
        candidates.push(exe.join(name));
    }
    if let Some(parent) = region_dir.parent() {
        candidates.push(parent.join(name));
    }
    candidates.push(PathBuf::from(name));

    candidates
        .into_iter()
        .find(|path| path.is_file())
        .ok_or_else(|| Error::new(format!("could not find {name}; pass --region-lib")))
}

#[cfg(unix)]
type Library = libloading::os::unix::Library;

#[cfg(not(unix))]
type Library = libloading::Library;

/// Opens `path` so the modules loaded afterwards resolve their own
/// `region_new` against this copy.
#[cfg(unix)]
fn open_process_wide(path: &Path) -> std::result::Result<Library, libloading::Error> {
    use libloading::os::unix::{RTLD_GLOBAL, RTLD_NOW};
    unsafe { Library::open(Some(path), RTLD_NOW | RTLD_GLOBAL) }
}

/// On Windows an already-loaded DLL is found by name and one beside the
/// executable needs no path, so a plain load reaches the same copy.
#[cfg(not(unix))]
fn open_process_wide(path: &Path) -> std::result::Result<Library, libloading::Error> {
    unsafe { Library::new(path) }
}

type FileTryUncompress =
    unsafe extern "C" fn(*const c_char, *mut c_void, *mut c_int) -> *mut c_void;
type VecFree = unsafe extern "C" fn(*mut c_void);
type VecToCStr = unsafe extern "C" fn(*mut c_void) -> *mut c_char;
type StringFree = unsafe extern "C" fn(*mut c_char);
type ProgressFn = Option<extern "C" fn(*mut c_void, c_int, *const c_char, *const c_char)>;
type HelperStructNew =
    unsafe extern "C" fn(ProgressFn, *mut c_void, *const c_void, u64, u64) -> *mut c_void;
type HelperStructFree = unsafe extern "C" fn(*mut c_void);
type RegionGetSize = unsafe extern "C" fn(*mut c_void) -> i32;
type RegionGetPaletteLen = unsafe extern "C" fn(*mut c_void) -> usize;
type RegionGetPaletteName = unsafe extern "C" fn(*mut c_void, usize) -> *const c_char;
type RegionGetPropertyLen = unsafe extern "C" fn(*mut c_void, usize) -> usize;
type RegionGetPropertyPart = unsafe extern "C" fn(*mut c_void, usize, usize) -> *const c_char;
type RegionGetBlock = unsafe extern "C" fn(*mut c_void, usize) -> u32;
type RegionGetDataVersion = unsafe extern "C" fn(*mut c_void) -> u32;
type RegionFree = unsafe extern "C" fn(*mut c_void);
type GetObject = unsafe extern "C" fn(
    *mut c_void,
    *mut *mut c_void,
    *mut c_void,
    *const c_void,
) -> *const c_char;
type ObjectFree = unsafe extern "C" fn(*mut c_void);
type CreateSingle =
    unsafe extern "C" fn(*mut c_void, *mut *mut c_void, *mut c_void, *mut c_void) -> *const c_char;
type RegionNum = unsafe extern "C" fn(*mut c_void) -> i32;
type RegionName = unsafe extern "C" fn(*mut c_void, i32) -> *const c_char;
type CreateMulti = unsafe extern "C" fn(
    *mut c_void,
    *mut *mut c_void,
    i32,
    *mut c_void,
    *mut c_void,
) -> *const c_char;
