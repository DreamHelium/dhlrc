/*! Where dhlrc gets its resource files from.
 *
 * Two quite different things live under "resource" here:
 *
 * - the files dhlrc ships inside itself (see [`bundled`]), and
 * - Minecraft's own assets, addressed by a namespaced path ([`Identifier`]).
 *
 * Both frontends share this layer: the Qt GUI reaches it through the C ABI,
 * the terminal frontend uses it directly. Keeping it in the core is what lets a
 * bundled file be read without a toolkit — a Qt `.qrc` would only be readable
 * from Qt.
 */

pub mod archive;
pub mod bundled;
pub mod cache;
pub mod environment;
pub mod identifier;
pub mod object;
pub mod set;
pub mod translate;

pub use archive::ZipSource;
pub use environment::ResourceEnvironment;
pub use identifier::Identifier;
pub use object::ObjectSource;
pub use set::ResourceSet;

use std::path::PathBuf;

/// One place a resource can be read from.
///
/// Several are asked in turn, so a resource pack (or a mod's own files) can sit
/// in front of the vanilla ones: the first source that has an id wins. A source
/// lives behind the core singleton, so it has to be shareable across threads.
pub trait Source: Send + Sync {
    /// The bytes of `id`, or `None` when this source does not have it.
    fn get(&self, id: &Identifier) -> Option<Vec<u8>>;
}

/// A directory laid out like a resource pack (or an unpacked client jar):
/// resources are the files under `assets/<namespace>/…`.
pub struct DirSource {
    root: PathBuf,
}

impl DirSource {
    pub fn new(root: impl Into<PathBuf>) -> Self {
        Self { root: root.into() }
    }
}

impl Source for DirSource {
    fn get(&self, id: &Identifier) -> Option<Vec<u8>> {
        std::fs::read(self.root.join(id.asset_path())).ok()
    }
}

#[cfg(test)]
mod tests {
    use super::{DirSource, Identifier, Source};

    #[test]
    fn a_directory_source_reads_the_pack_layout() {
        let root = std::env::temp_dir().join(format!("dhlrc-dirsource-{}", std::process::id()));
        let file = root.join("assets/minecraft/textures/block/stone.png");
        std::fs::create_dir_all(file.parent().unwrap()).unwrap();
        std::fs::write(&file, b"PNG").unwrap();

        let source = DirSource::new(&root);
        let stone = Identifier::parse("minecraft:textures/block/stone.png");
        assert_eq!(source.get(&stone).as_deref(), Some(&b"PNG"[..]));

        let dirt = Identifier::parse("minecraft:textures/block/dirt.png");
        assert!(source.get(&dirt).is_none());

        std::fs::remove_dir_all(&root).ok();
    }
}
