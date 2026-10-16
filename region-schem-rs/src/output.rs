//! Writing a region out as a `.schem` file.

use crate::config::OutputConfig;
use crate::schem::{DEFAULT_VERSION, Schematic, write_schematic};
use common_rs::helper_struct::HelperStruct;
use common_rs::i18n::i18n;
use common_rs::my_error::MyError;
use common_rs::region::Region;
use common_rs::util::{string_to_ptr_fail_to_null, vec_try_compress_real};
use std::error::Error;
use std::ffi::{CStr, c_char};
use std::fs::File;
use std::io::Write;
use std::path::Path;
use std::ptr::null;
use zuri_nbt::encoding::BigEndian;
use zurinbt_common::gettext_text;

/// Writes `schematic` to `path` at the given specification version: the NBT,
/// gzipped, which is what the format mandates.
pub fn write_file(
    schematic: &Schematic,
    version: i32,
    path: &Path,
    helper_struct: &HelperStruct,
) -> Result<(), Box<dyn Error>> {
    let nbt = write_schematic(schematic, version, helper_struct)?;
    let mut buf = vec![];
    nbt.write(&mut buf, BigEndian::default())?;
    /* A `.schem` file is gzipped NBT, not zlib. */
    let compressed = vec_try_compress_real(Box::into_raw(Box::new(buf)), helper_struct, false)?;

    let mut file = File::create(path)?;
    file.write_all(&compressed)?;
    Ok(())
}

fn region_save_internal(
    region: *mut Region,
    filename: *const c_char,
    output_config: *mut OutputConfig,
    helper_struct: &HelperStruct,
) -> Result<(), Box<dyn Error>> {
    if region.is_null() || filename.is_null() {
        return Err(Box::new(MyError {
            msg: gettext_text(i18n("Null pointer detected")),
        }));
    }
    /* A null config means the host did not resolve the options (a dialog that
     * was answered with "use the defaults", for instance); the plugin's own
     * default is used then. */
    let requested = if output_config.is_null() {
        DEFAULT_VERSION
    } else {
        unsafe { (*output_config).version }
    };

    let real_region = unsafe { &*region };
    let schematic = Schematic::from_region(real_region);
    /* The option's default keeps the version the file was read from, so a
     * version 3 file is not silently downgraded. */
    let version = if requested == 0 {
        schematic.version_or_default()
    } else {
        requested
    };
    let path = unsafe { CStr::from_ptr(filename) }.to_str()?;
    write_file(&schematic, version, Path::new(path), helper_struct)
}

#[unsafe(no_mangle)]
pub extern "C" fn region_save(
    region: *mut Region,
    filename: *const c_char,
    output_config: *mut OutputConfig,
    helper_struct: *mut HelperStruct,
) -> *const c_char {
    match region_save_internal(region, filename, output_config, unsafe {
        &*helper_struct
    }) {
        Ok(()) => null(),
        Err(e) => string_to_ptr_fail_to_null(&e.to_string()),
    }
}

#[cfg(test)]
mod tests
{
    use super::*;
    use crate::schem::read_schematic;
    use common_rs::region::{BaseData, Palette};
    use flate2::read::GzDecoder;
    use std::io::Read;
    use time::UtcDateTime;
    use zuri_nbt::NBTRoot;

    fn helper() -> HelperStruct {
        HelperStruct::new(None, std::ptr::null_mut(), std::ptr::null(), u64::MAX, 0)
    }

    fn palette(name: &str, property: &[(&str, &str)]) -> Palette {
        Palette {
            id_name: name.to_string(),
            property: property
                .iter()
                .map(|(name, value)| (name.to_string(), value.to_string()))
                .collect(),
        }
    }

    /// A region as the program would hold one after reading a file.
    fn region() -> Region {
        Region {
            data_version: 4189,
            base_data: BaseData {
                create_time: UtcDateTime::from_unix_timestamp(1_700_000_000).unwrap(),
                modify_time: UtcDateTime::from_unix_timestamp(1_700_000_000).unwrap(),
                description: "a house".to_string(),
                author: "tester".to_string(),
                name: "house".to_string(),
                region_name: "main".to_string(),
            },
            region_size: (2, 1, 2),
            region_offset: (1, 2, 3),
            block_array: vec![1, 0, 0, 1],
            block_entity_array: vec![],
            entity_array: vec![],
            palette_array: vec![
                palette("minecraft:air", &[]),
                palette("minecraft:oak_planks", &[("variant", "oak")]),
            ],
            biome_array: vec![],
            biome_palette_array: vec![],
            format_data: None,
            format_version: 3,
        }
    }

    fn temp_path(tag: &str) -> std::path::PathBuf {
        let dir = std::env::temp_dir().join(format!(
            "dhlrc-schem-{tag}-{}",
            std::process::id()
        ));
        std::fs::create_dir_all(&dir).unwrap();
        dir
    }

    #[test]
    fn a_region_writes_a_file_that_reads_back() {
        let dir = temp_path("save");
        let path = dir.join("round_trip.schem");

        let schematic = Schematic::from_region(&region());
        /* The region says it came from a version 3 file, and the option's
         * default keeps that instead of imposing version 2. */
        assert_eq!(schematic.version_or_default(), 3);
        write_file(&schematic, 3, &path, &helper()).unwrap();

        let bytes = std::fs::read(&path).unwrap();
        /* The format mandates gzip, so the file has to start with its magic. */
        assert_eq!(&bytes[..2], &[0x1f, 0x8b]);

        let mut raw = vec![];
        GzDecoder::new(&bytes[..]).read_to_end(&mut raw).unwrap();
        let nbt = NBTRoot::read(&raw[..], BigEndian::default()).unwrap();
        let read = read_schematic(&nbt, true, &helper()).unwrap();

        assert_eq!(read.version, 3);
        assert_eq!(read.size, (2, 1, 2));
        assert_eq!(read.offset, (1, 2, 3));
        assert_eq!(read.data_version, 4189);
        assert_eq!(read.blocks, vec![1, 0, 0, 1]);
        assert_eq!(read.palette.len(), 2);
        assert_eq!(read.palette[1].id_name, "minecraft:oak_planks");
        assert_eq!(
            read.palette[1].property,
            vec![("variant".to_string(), "oak".to_string())]
        );

        /* The metadata the region model keeps comes back out of the file. */
        let fields = read.metadata_fields();
        assert_eq!(fields.name.as_deref(), Some("house"));
        assert_eq!(fields.author.as_deref(), Some("tester"));
        assert_eq!(fields.description.as_deref(), Some("a house"));
        assert_eq!(fields.date, Some(1_700_000_000_000));

        std::fs::remove_dir_all(&dir).ok();
    }

    #[test]
    fn the_requested_version_wins_over_the_source_one() {
        let dir = temp_path("save-v2");
        let path = dir.join("version_2.schem");

        let schematic = Schematic::from_region(&region());
        write_file(&schematic, 2, &path, &helper()).unwrap();

        let bytes = std::fs::read(&path).unwrap();
        let mut raw = vec![];
        GzDecoder::new(&bytes[..]).read_to_end(&mut raw).unwrap();
        let nbt = NBTRoot::read(&raw[..], BigEndian::default()).unwrap();
        let read = read_schematic(&nbt, true, &helper()).unwrap();
        assert_eq!(read.version, 2);
        assert_eq!(read.blocks, vec![1, 0, 0, 1]);

        std::fs::remove_dir_all(&dir).ok();
    }

    #[test]
    fn a_region_without_a_size_is_refused() {
        let mut region = region();
        region.region_size = (0, 0, 0);
        let schematic = Schematic::from_region(&region);
        let dir = temp_path("save-empty");
        assert!(write_file(&schematic, 2, &dir.join("empty.schem"), &helper()).is_err());
        std::fs::remove_dir_all(&dir).ok();
    }
}
