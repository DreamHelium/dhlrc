/*! The pieces a frontend knows about, turned into a `ResourceSet`.
 *
 * Two frontends should not each decide *where* resources come from, so the
 * order lives here and only the facts are passed in: the cache directory the
 * downloader filled, the version whose client jar to use, and any resource
 * packs or mod jars.
 */

use std::path::PathBuf;

use super::{ObjectSource, ResourceSet, ZipSource, cache};

/// Where the resources of one environment are, and in which order they win.
#[derive(Debug, Clone, Default)]
pub struct ResourceEnvironment {
    /// The configured cache directory; the downloader's files live under it.
    pub cache_dir: PathBuf,
    /// The version id whose client jar to use (`1.21.4`), when known.
    pub version: Option<String>,
    /// Resource packs and mod jars, highest priority first.
    pub packs: Vec<PathBuf>,
}

impl ResourceEnvironment {
    /// Stacks the sources in the order a lookup should try them:
    ///
    /// 1. the resource packs and mod jars, so they override vanilla;
    /// 2. the version's client jar, the game's own files;
    /// 3. the downloaded object store, which is where the non-English language
    ///    files are (the jar carries only `en_us`).
    ///
    /// A source that is absent (no pack, no jar downloaded, no index) is simply
    /// left out, so building the set never fails.
    pub fn resource_set(&self) -> ResourceSet {
        let mut set = ResourceSet::new();

        for pack in &self.packs {
            if let Some(source) = ZipSource::open(pack.clone()) {
                set.push(source);
            }
        }

        if let Some(version) = &self.version {
            if let Some(source) = ZipSource::open(cache::client_jar(&self.cache_dir, version)) {
                set.push(source);
            }
        }

        if let Some(source) = ObjectSource::from_index_dir(
            cache::objects(&self.cache_dir),
            cache::asset_json(&self.cache_dir),
        ) {
            set.push(source);
        }

        set
    }
}

#[cfg(test)]
mod tests {
    use super::ResourceEnvironment;

    #[test]
    fn an_empty_environment_stacks_nothing_and_does_not_fail() {
        let set = ResourceEnvironment::default().resource_set();
        assert!(set.is_empty());
    }
}
