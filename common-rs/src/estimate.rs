/*! The optional `region_estimate` ABI shared by the region modules and the
 * core, and the file reading it needs.
 *
 * A region module normally receives an already decoded object from an object
 * codec. An estimate, however, is asked for a *file name*, so the module has to
 * do the decompression and NBT decoding itself. Both live here instead of in
 * every module: the format-specific part stays in the module, the plain
 * "open, decompress, parse NBT" part is the same for all of them.
 *
 * The struct matches `DhlrcEstimate` in `dhlrc-core/include/dhlrc_core.h` and
 * is passed across the C ABI, so it must stay `#[repr(C)]` and free of Rust
 * types. `-1` means "the format does not know".
 */

use flate2::read::{GzDecoder, ZlibDecoder};
use std::ffi::{CStr, c_char};
use std::fs;
use std::io::Read;
use zuri_nbt::NBTRoot;
use zuri_nbt::encoding::{BigEndian, LittleEndian, NetworkLittleEndian};

/// What a region module can say about a file without building a region.
///
/// Mirrors `DhlrcEstimate` in `dhlrc-core/include/dhlrc_core.h`. The field
/// order is chosen so the C and Rust layouts have no padding differences: the
/// three `int64_t`s come first, then the smaller fields.
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct DhlrcEstimate {
    /// Regions in the file: 1 for a single-region format.
    pub regions: i64,
    /// Blocks that will be loaded, or the volume when the format only gives
    /// that; `-1` when the module cannot tell. See `blocks_exact`.
    pub blocks: i64,
    /// Bytes after decompression, or `-1` when unknown.
    pub uncompressed_bytes: i64,
    /// The representative region extent (`Width`, `Height`, `Length`); zero
    /// for a multi-region file whose regions have different sizes.
    pub size_x: i32,
    pub size_y: i32,
    pub size_z: i32,
    /// Non-zero when `blocks` is the format's own set-block count (and not
    /// merely the bounding-box volume).
    pub blocks_exact: i32,
    /// Non-zero when the other fields were filled in. The function also
    /// returns a status, but the struct is checked by callers that only have
    /// the pointer.
    pub ok: i32,
}

impl DhlrcEstimate {
    /// The value for "no estimate": every optional number is `-1`.
    pub const fn unknown() -> Self {
        Self {
            regions: -1,
            blocks: -1,
            uncompressed_bytes: -1,
            size_x: 0,
            size_y: 0,
            size_z: 0,
            blocks_exact: 0,
            ok: 0,
        }
    }
}

impl Default for DhlrcEstimate {
    fn default() -> Self {
        Self::unknown()
    }
}

/// Reads `filename`, transparently decompressing gzip or zlib, and decodes the
/// NBT in it the way an object codec would (big endian first, then the other
/// encodings). Returns the tree and the number of bytes after decompression.
///
/// This is what a module's `region_estimate` normally starts with; the format
/// parsing that follows is the module's own.
pub fn read_region_file(filename: *const c_char) -> Option<(NBTRoot, u64)> {
    if filename.is_null() {
        return None;
    }
    let name = unsafe { CStr::from_ptr(filename) }.to_str().ok()?;
    let file_data = fs::read(name).ok()?;
    let (bytes, uncompressed_bytes) = uncompress(file_data)?;
    let nbt = NBTRoot::read(bytes.as_slice(), BigEndian)
        .or_else(|_| NBTRoot::read(bytes.as_slice(), LittleEndian))
        .or_else(|_| NBTRoot::read(bytes.as_slice(), NetworkLittleEndian))
        .ok()?;
    Some((nbt, uncompressed_bytes))
}

/// The file bytes after decompression and how many there are. The detection
/// matches `file_try_uncompress` in `region-rs`, so a file that loads also
/// estimates.
fn uncompress(mut file_data: Vec<u8>) -> Option<(Vec<u8>, u64)> {
    if file_data.len() > 2 && file_data[0] == 0x1f && file_data[1] == 0x8b {
        let mut bytes = Vec::new();
        GzDecoder::new(file_data.as_slice())
            .read_to_end(&mut bytes)
            .ok()?;
        let length = bytes.len() as u64;
        Some((bytes, length))
    } else if file_data.len() > 1 && file_data[0] == 0x78 {
        let mut bytes = Vec::new();
        ZlibDecoder::new(file_data.as_slice())
            .read_to_end(&mut bytes)
            .ok()?;
        let length = bytes.len() as u64;
        Some((bytes, length))
    } else {
        let length = file_data.len() as u64;
        /* `file_data` is already the uncompressed form; no second copy. */
        file_data.shrink_to_fit();
        Some((file_data, length))
    }
}

/// The body of a module's `region_estimate` export: checks the pointers, reads
/// the file, lets `body` interpret the tree, and fills `out` on success.
///
/// Returns `0` when the estimate was written and `-1` when it was not (bad
/// argument, unreadable file, or `body` declining the format). On failure `out`
/// is filled with [`DhlrcEstimate::unknown`], so a caller that ignores the
/// status still sees a clean "no estimate".
pub fn estimate_region_file<F>(
    filename: *const c_char,
    out: *mut DhlrcEstimate,
    body: F,
) -> i32
where
    F: FnOnce(&NBTRoot, u64) -> Option<DhlrcEstimate>,
{
    if out.is_null() {
        return -1;
    }
    let estimate = read_region_file(filename)
        .and_then(|(nbt, bytes)| body(&nbt, bytes))
        .map(|mut estimate| {
            estimate.ok = 1;
            estimate
        });
    match estimate {
        Some(estimate) => {
            unsafe { *out = estimate };
            0
        }
        None => {
            unsafe { *out = DhlrcEstimate::unknown() };
            -1
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::CString;
    use std::io::Write;

    /// A small gzip'd big-endian NBT file with one string field.
    fn nbt_file(name: &str) -> std::path::PathBuf {
        use zuri_nbt::NBTTag;
        use zuri_nbt::tag::{Compound, String as NbtString};

        let directory = std::env::temp_dir().join(format!(
            "dhlrc-estimate-{}-{name}",
            std::process::id()
        ));
        fs::create_dir_all(&directory).unwrap();
        let path = directory.join("probe.nbt");

        let mut fields = std::collections::HashMap::new();
        fields.insert(
            "message".to_string(),
            NBTTag::String(NbtString("hello".to_string())),
        );
        let root = NBTRoot {
            tag_name: String::new(),
            data: NBTTag::Compound(Compound(fields)),
        };
        let mut bytes = Vec::new();
        root.write(&mut bytes, BigEndian).unwrap();

        let file = fs::File::create(&path).unwrap();
        let mut encoder =
            flate2::write::GzEncoder::new(file, flate2::Compression::fast());
        encoder.write_all(&bytes).unwrap();
        encoder.finish().unwrap();
        path
    }

    #[test]
    fn a_gzip_nbt_file_is_read_with_its_uncompressed_size() {
        let path = nbt_file("gzip");
        let name = CString::new(path.to_string_lossy().as_bytes()).unwrap();
        let (nbt, bytes) = read_region_file(name.as_ptr()).expect("the file should be read");
        assert!(bytes > 0);
        assert_eq!(
            nbt.data.view().at("message").string().unwrap(),
            "hello"
        );
        let _ = fs::remove_dir_all(path.parent().unwrap());
    }

    #[test]
    fn the_estimate_helper_fills_out_and_reports_a_decline() {
        let path = nbt_file("helper");
        let name = CString::new(path.to_string_lossy().as_bytes()).unwrap();
        let mut estimate = DhlrcEstimate::unknown();
        let status = estimate_region_file(name.as_ptr(), &mut estimate, |_, bytes| {
            Some(DhlrcEstimate {
                regions: 1,
                blocks: 10,
                uncompressed_bytes: bytes as i64,
                blocks_exact: 1,
                size_x: 2,
                size_y: 5,
                size_z: 1,
                ..DhlrcEstimate::unknown()
            })
        });
        assert_eq!(status, 0);
        assert_eq!(estimate.ok, 1);
        assert_eq!(estimate.blocks, 10);
        assert!(estimate.uncompressed_bytes > 0);

        let status = estimate_region_file(name.as_ptr(), &mut estimate, |_, _| None);
        assert_eq!(status, -1);
        assert_eq!(estimate.ok, 0);
        assert_eq!(estimate.blocks, -1);
        let _ = fs::remove_dir_all(path.parent().unwrap());
    }
}
