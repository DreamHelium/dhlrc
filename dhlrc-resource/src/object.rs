/*! The downloaded asset objects, resolved by name through the asset index.
 *
 * The launcher splits a version's assets in two: a jar with some files, and an
 * *index* (`assets/indexes/<id>.json`) mapping a resource name to a hash, with
 * the contents fetched one by one from Mojang's CDN into a store of hash-named
 * files. Language files are the case that matters here: the client jar carries
 * only `en_us`, every other locale comes this way.
 *
 * The index is keyed by the resource's path with `assets/` dropped
 * (`minecraft/lang/zh_cn.json`), so a lookup goes id -> hash -> file.
 */

use std::collections::HashMap;
use std::path::{Path, PathBuf};

use super::{Identifier, Source};

/// A [`Source`] over a downloaded object store.
pub struct ObjectSource {
    root: PathBuf,
    index: HashMap<String, String>,
}

impl ObjectSource {
    /// Builds a source over the files under `root`, resolving names through the
    /// asset index `index` (its JSON text). `None` when it lists nothing.
    pub fn new(root: impl Into<PathBuf>, index: &str) -> Option<Self> {
        let mut names = HashMap::new();
        merge_index(&mut names, index);
        Self::from_names(root.into(), names)
    }

    /// The same, reading the index from `index_path`.
    pub fn from_file(root: impl Into<PathBuf>, index_path: impl AsRef<Path>) -> Option<Self> {
        let text = std::fs::read_to_string(index_path).ok()?;
        Self::new(root, &text)
    }

    /// Reads every `*.json` under `index_dir` and merges them.
    ///
    /// The downloader keeps one index per version there, and a name can only
    /// mean one thing, so merging them is harmless and saves the caller having
    /// to know which file belongs to which version. `None` when the directory
    /// holds no usable index.
    pub fn from_index_dir(root: impl Into<PathBuf>, index_dir: impl AsRef<Path>) -> Option<Self> {
        let mut names = HashMap::new();
        for entry in std::fs::read_dir(index_dir.as_ref()).ok()?.flatten() {
            let path = entry.path();
            if path.extension().and_then(|kind| kind.to_str()) != Some("json") {
                continue;
            }
            if let Ok(text) = std::fs::read_to_string(&path) {
                merge_index(&mut names, &text);
            }
        }
        Self::from_names(root.into(), names)
    }

    fn from_names(root: PathBuf, index: HashMap<String, String>) -> Option<Self> {
        if index.is_empty() {
            return None;
        }
        Some(Self { root, index })
    }

    /// How many names the index knows.
    pub fn len(&self) -> usize {
        self.index.len()
    }

    pub fn is_empty(&self) -> bool {
        self.index.is_empty()
    }

    /// The hash of `id`, when the index lists it.
    pub fn hash(&self, id: &Identifier) -> Option<&str> {
        self.index
            .get(&format!("{}/{}", id.namespace, id.path))
            .map(String::as_str)
    }
}

/// Adds every `objects` entry of `index` (JSON) to `names`, ignoring anything
/// that does not parse.
fn merge_index(names: &mut HashMap<String, String>, index: &str) {
    let Ok(parsed) = serde_json::from_str::<serde_json::Value>(index) else {
        return;
    };
    let Some(objects) = parsed.get("objects").and_then(|value| value.as_object()) else {
        return;
    };
    for (name, entry) in objects {
        if let Some(hash) = entry.get("hash").and_then(|value| value.as_str()) {
            names.insert(name.clone(), hash.to_owned());
        }
    }
}

impl Source for ObjectSource {
    fn get(&self, id: &Identifier) -> Option<Vec<u8>> {
        let hash = self.hash(id)?;
        /* The project's own downloader writes a flat store (`object/<hash>`);
         * the launcher's layout adds a two-character subdirectory. Try both. */
        if let Ok(bytes) = std::fs::read(self.root.join(hash)) {
            return Some(bytes);
        }
        let prefix = hash.get(..2)?;
        std::fs::read(self.root.join(prefix).join(hash)).ok()
    }
}

#[cfg(test)]
mod tests {
    use super::ObjectSource;
    use crate::{Identifier, Source};

    const INDEX: &str = r#"{
        "objects": {
            "minecraft/lang/en_us.json": { "hash": "aa11", "size": 3 },
            "minecraft/lang/zh_cn.json": { "hash": "bb22", "size": 3 }
        }
    }"#;

    #[test]
    fn an_object_source_resolves_a_name_through_the_index() {
        let root = std::env::temp_dir().join(format!("dhlrc-objects-{}", std::process::id()));
        std::fs::create_dir_all(&root).unwrap();
        /* The flat layout the project's downloader writes. */
        std::fs::write(root.join("bb22"), b"ZH").unwrap();
        /* The launcher's two-character layout, for the other entry. */
        std::fs::create_dir_all(root.join("aa")).unwrap();
        std::fs::write(root.join("aa").join("aa11"), b"EN").unwrap();

        let source = ObjectSource::new(&root, INDEX).expect("the index parses");
        assert_eq!(
            source
                .get(&Identifier::parse("minecraft:lang/zh_cn.json"))
                .as_deref(),
            Some(&b"ZH"[..])
        );
        assert_eq!(
            source
                .get(&Identifier::parse("minecraft:lang/en_us.json"))
                .as_deref(),
            Some(&b"EN"[..])
        );
        assert!(
            source
                .get(&Identifier::parse("minecraft:lang/fr_fr.json"))
                .is_none()
        );
        assert!(
            source
                .get(&Identifier::parse("create:lang/zh_cn.json"))
                .is_none()
        );

        std::fs::remove_dir_all(&root).ok();
    }

    #[test]
    fn an_index_directory_contributes_every_index_in_it() {
        let root = std::env::temp_dir().join(format!("dhlrc-objectsdir-{}", std::process::id()));
        let indexes = root.join("asset_json");
        std::fs::create_dir_all(&indexes).unwrap();
        std::fs::write(indexes.join("one.json"), INDEX).unwrap();
        /* A file that is not an index must not upset the merge. */
        std::fs::write(indexes.join("notes.txt"), "not json").unwrap();
        std::fs::create_dir_all(root.join("object")).unwrap();
        std::fs::write(root.join("object").join("bb22"), b"ZH").unwrap();

        let source = ObjectSource::from_index_dir(root.join("object"), &indexes)
            .expect("the index directory is read");
        assert_eq!(
            source
                .get(&Identifier::parse("minecraft:lang/zh_cn.json"))
                .as_deref(),
            Some(&b"ZH"[..])
        );

        std::fs::remove_dir_all(&root).ok();
    }
}
