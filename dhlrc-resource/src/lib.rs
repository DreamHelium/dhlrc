/*! Where dhlrc gets its resource files from.
 *
 * Two quite different things live here:
 *
 * - the files dhlrc ships inside itself (see [`bundled`]), and
 * - Minecraft's own assets, addressed by a namespaced path ([`Identifier`]).
 *
 * Every reader of a resource comes through this layer, and they do not all
 * come through the core: `dhlrc-core` addresses the language files and the
 * client jar, `dhlrc-model` the blockstates and models, `dhlrc-render` the
 * textures. Keeping it its own crate is what lets the renderer read packs and
 * the client jar without linking the core — and so without the core's whole C
 * API ending up inside the library the Qt view draws through.
 *
 * A resource is asked for by [`Identifier`] and answered by a [`Source`]; the
 * sources are stacked in a [`ResourceSet`], so a resource pack overrides
 * vanilla exactly as it does in game.
 */

pub mod archive;
pub mod bundled;
pub mod cache;
pub mod environment;
pub mod handle_block;
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

    /// Every resource this source holds under `prefix`, a resource path such as
    /// `blockstates/` — what a chooser needs in order to know what a version
    /// has. A source that can only answer by name returns nothing; one backed
    /// by a zip or a directory can look through what it holds.
    fn list(&self, _prefix: &str) -> Vec<Identifier> {
        Vec::new()
    }
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

    fn list(&self, prefix: &str) -> Vec<Identifier> {
        /* An unpacked pack or jar: walk `assets/<namespace>/<prefix>` and turn
         * every file found there into an id. */
        let assets = self.root.join("assets");
        let Ok(namespaces) = std::fs::read_dir(&assets) else {
            return Vec::new();
        };
        let mut found = Vec::new();
        for namespace in namespaces.flatten() {
            let namespace_name = namespace.file_name().to_string_lossy().to_string();
            let Ok(entries) = std::fs::read_dir(namespace.path().join(prefix)) else {
                continue;
            };
            for entry in entries.flatten() {
                if !entry.path().is_file() {
                    continue;
                }
                let path = format!("{prefix}{}", entry.file_name().to_string_lossy());
                found.push(Identifier::new(namespace_name.clone(), path));
            }
        }
        found.sort_by(|a, b| (&a.namespace, &a.path).cmp(&(&b.namespace, &b.path)));
        found
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
