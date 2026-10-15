/*! Where the downloaded files are kept, under the cache directory.
 *
 * One subdirectory per kind of file, so a reader finds what the downloader
 * wrote without either side guessing:
 *
 * - [`CLIENT_JAR`]: a version's client jar;
 * - [`RESOURCE_PACK`]: resource packs and mod jars;
 * - [`INDEX_JSON`]: a version's own json (the one naming its asset index);
 * - [`ASSET_JSON`]: the asset indexes;
 * - [`OBJECT`]: the asset objects, named by hash.
 *
 * `download` writes into these and `environment` reads from them, so the layout
 * only has to be right once.
 */

use std::path::{Path, PathBuf};

/// The subdirectory holding version client jars.
pub const CLIENT_JAR: &str = "client_jar";
/// The subdirectory holding resource packs and mod jars.
pub const RESOURCE_PACK: &str = "resource_pack";
/// The subdirectory holding the downloaded asset objects.
pub const OBJECT: &str = "object";
/// The subdirectory holding the downloaded asset indexes.
pub const ASSET_JSON: &str = "asset_json";
/// The subdirectory holding a version's own json (the one naming its asset index).
pub const INDEX_JSON: &str = "index_json";

/// The client jar of `version`, under `cache_dir`.
pub fn client_jar(cache_dir: &Path, version: &str) -> PathBuf {
    cache_dir.join(CLIENT_JAR).join(format!("{version}.jar"))
}

/// A resource pack (or mod jar) called `name`, under `cache_dir`.
pub fn resource_pack(cache_dir: &Path, name: &str) -> PathBuf {
    cache_dir.join(RESOURCE_PACK).join(name)
}

/// The downloaded asset objects, under `cache_dir`.
pub fn objects(cache_dir: &Path) -> PathBuf {
    cache_dir.join(OBJECT)
}

/// The downloaded asset indexes, under `cache_dir`.
pub fn asset_json(cache_dir: &Path) -> PathBuf {
    cache_dir.join(ASSET_JSON)
}

/// A version's own json (the one naming its asset index), under `cache_dir`.
pub fn index_json(cache_dir: &Path) -> PathBuf {
    cache_dir.join(INDEX_JSON)
}

#[cfg(test)]
mod tests {
    use super::{asset_json, client_jar, index_json, objects, resource_pack};
    use std::path::Path;

    #[test]
    fn each_kind_has_its_own_subdirectory() {
        let cache = Path::new("/tmp/cache");
        assert_eq!(
            client_jar(cache, "1.21.4"),
            Path::new("/tmp/cache/client_jar/1.21.4.jar")
        );
        assert_eq!(
            resource_pack(cache, "Faithful.zip"),
            Path::new("/tmp/cache/resource_pack/Faithful.zip")
        );
        assert_eq!(objects(cache), Path::new("/tmp/cache/object"));
        assert_eq!(asset_json(cache), Path::new("/tmp/cache/asset_json"));
        assert_eq!(index_json(cache), Path::new("/tmp/cache/index_json"));
    }
}
