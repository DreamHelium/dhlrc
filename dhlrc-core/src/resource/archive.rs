/*! A resource source backed by a zip file.
 *
 * A version's client jar, a resource pack and a mod's jar are all zips, and all
 * three address a resource by its path inside the archive: `minecraft:
 * textures/block/stone.png` is the entry `assets/minecraft/textures/block/
 * stone.png`. That is the same layout a resource pack has unpacked on disk, so
 * this and the directory source agree on names.
 *
 * The archive is opened once and kept; entry lookups take a lock because
 * reading one needs `&mut`. Resources are small and read once, so the simple
 * thing is good enough.
 */

use std::fs::File;
use std::io::Read;
use std::path::{Path, PathBuf};
use std::sync::Mutex;

use zip::ZipArchive;

use super::{Identifier, Source};

/// A [`Source`] reading out of a zip archive.
pub struct ZipSource {
    path: PathBuf,
    archive: Mutex<ZipArchive<File>>,
}

impl ZipSource {
    /// Opens `path` as a zip. `None` when it is missing or is not a zip.
    pub fn open(path: impl Into<PathBuf>) -> Option<Self> {
        let path = path.into();
        let archive = ZipArchive::new(File::open(&path).ok()?).ok()?;
        Some(Self {
            path,
            archive: Mutex::new(archive),
        })
    }

    /// The file this source reads from.
    pub fn path(&self) -> &Path {
        &self.path
    }

    /// The bytes of the entry `name`, or `None` when the archive has no such
    /// entry.
    pub fn read(&self, name: &str) -> Option<Vec<u8>> {
        let mut archive = self.archive.lock().ok()?;
        let mut entry = archive.by_name(name).ok()?;
        let mut bytes = Vec::with_capacity(entry.size() as usize);
        entry.read_to_end(&mut bytes).ok()?;
        Some(bytes)
    }

    /// Every entry name in the archive, in the order the zip lists them.
    pub fn names(&self) -> Vec<String> {
        match self.archive.lock() {
            Ok(archive) => archive.file_names().map(str::to_string).collect(),
            Err(_) => Vec::new(),
        }
    }
}

impl Source for ZipSource {
    fn get(&self, id: &Identifier) -> Option<Vec<u8>> {
        self.read(&id.asset_path())
    }
}

#[cfg(test)]
mod tests {
    use super::ZipSource;
    use crate::resource::{Identifier, Source};
    use std::fs::File;
    use std::io::Write;

    #[test]
    fn a_zip_source_reads_pack_entries() {
        let path = std::env::temp_dir().join(format!("dhlrc-zipsource-{}.zip", std::process::id()));
        {
            let mut zip = zip::ZipWriter::new(File::create(&path).unwrap());
            let options = zip::write::SimpleFileOptions::default()
                .compression_method(zip::CompressionMethod::Stored);
            zip.start_file("assets/minecraft/textures/block/stone.png", options)
                .unwrap();
            zip.write_all(b"PNG").unwrap();
            zip.finish().unwrap();
        }

        let source = ZipSource::open(&path).expect("the zip opens");
        let stone = Identifier::parse("minecraft:textures/block/stone.png");
        assert_eq!(source.get(&stone).as_deref(), Some(&b"PNG"[..]));

        let dirt = Identifier::parse("minecraft:textures/block/dirt.png");
        assert!(source.get(&dirt).is_none());
        assert_eq!(source.names().len(), 1);

        assert!(ZipSource::open(path.with_extension("missing")).is_none());
        std::fs::remove_file(&path).ok();
    }
}
