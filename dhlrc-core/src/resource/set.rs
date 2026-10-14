/*! Several sources stacked in priority order.
 *
 * A name can live in more than one place, and which place wins is the whole
 * point of resource packs: a pack overrides the vanilla file it shadows. So the
 * sources are ordered and the first one that has an id answers.
 *
 * This is what turns the three places a vanilla or mod file can live into one
 * lookup: put the resource packs and mod jars first (so they override vanilla),
 * then the version's client jar, then the downloaded object store — the last
 * one being where the non-English language files are.
 */

use std::collections::HashMap;
use std::sync::Arc;
use std::sync::Mutex;

use serde_json::Value;

use super::{Identifier, Source};

/// An ordered stack of sources with a memo of every answer.
pub struct ResourceSet {
    sources: Vec<Box<dyn Source>>,
    cache: Mutex<HashMap<Identifier, Option<Vec<u8>>>>,
    parsed: Mutex<HashMap<Identifier, Option<Arc<Value>>>>,
}

impl ResourceSet {
    pub fn new() -> Self {
        Self {
            sources: Vec::new(),
            cache: Mutex::new(HashMap::new()),
            parsed: Mutex::new(HashMap::new()),
        }
    }

    /// Adds a source. Earlier sources win over later ones.
    pub fn push(&mut self, source: impl Source + 'static) -> &mut Self {
        self.sources.push(Box::new(source));
        self
    }

    /// How many sources are stacked.
    pub fn len(&self) -> usize {
        self.sources.len()
    }

    pub fn is_empty(&self) -> bool {
        self.sources.is_empty()
    }

    /// The bytes of `id` as text. The files that matter here (language, models)
    /// are UTF-8, and a malformed one should not bring the lookup down.
    pub fn get_str(&self, id: &Identifier) -> Option<String> {
        self.get(id)
            .map(|bytes| String::from_utf8_lossy(&bytes).into_owned())
    }

    /// The parsed JSON of `id`, memoised. A name is looked up in a language file
    /// that is both large and asked for once per block shown, so parsing it every
    /// time would dominate; the shared `Arc` also lets many callers hold the one
    /// parsed value.
    pub fn get_json(&self, id: &Identifier) -> Option<Arc<Value>> {
        if let Ok(parsed) = self.parsed.lock() {
            if let Some(hit) = parsed.get(id) {
                return hit.clone();
            }
        }

        let value = self
            .get(id)
            .and_then(|bytes| serde_json::from_slice::<Value>(&bytes).ok())
            .map(Arc::new);

        if let Ok(mut parsed) = self.parsed.lock() {
            parsed.insert(id.clone(), value.clone());
        }
        value
    }
}

impl Default for ResourceSet {
    fn default() -> Self {
        Self::new()
    }
}

impl Source for ResourceSet {
    fn get(&self, id: &Identifier) -> Option<Vec<u8>> {
        if let Ok(cache) = self.cache.lock() {
            if let Some(hit) = cache.get(id) {
                return hit.clone();
            }
        }

        let found = self.sources.iter().find_map(|source| source.get(id));

        /* A miss is remembered too, so an id nothing has is looked for once. */
        if let Ok(mut cache) = self.cache.lock() {
            cache.insert(id.clone(), found.clone());
        }
        found
    }
}

#[cfg(test)]
mod tests {
    use super::ResourceSet;
    use crate::resource::{Identifier, Source};
    use std::sync::Arc;
    use std::sync::atomic::{AtomicUsize, Ordering};

    /// A source that answers for `minecraft:x` and counts how often it is asked.
    struct Counting {
        name: &'static str,
        hits: Arc<AtomicUsize>,
    }

    impl Source for Counting {
        fn get(&self, id: &Identifier) -> Option<Vec<u8>> {
            self.hits.fetch_add(1, Ordering::Relaxed);
            (id.path == "x").then(|| self.name.as_bytes().to_vec())
        }
    }

    #[test]
    fn the_first_source_wins_and_answers_are_cached() {
        let first = Arc::new(AtomicUsize::new(0));
        let second = Arc::new(AtomicUsize::new(0));

        let mut set = ResourceSet::new();
        set.push(Counting {
            name: "first",
            hits: first.clone(),
        });
        set.push(Counting {
            name: "second",
            hits: second.clone(),
        });

        let id = Identifier::parse("minecraft:x");
        assert_eq!(set.get(&id).as_deref(), Some(&b"first"[..]));
        assert_eq!(first.load(Ordering::Relaxed), 1);
        /* The second source is never asked once the first has answered. */
        assert_eq!(second.load(Ordering::Relaxed), 0);

        /* Served from the memo, so nothing is asked again. */
        assert_eq!(set.get_str(&id).as_deref(), Some("first"));
        assert_eq!(first.load(Ordering::Relaxed), 1);

        /* A miss walks every source once, then is remembered. */
        let missing = Identifier::parse("minecraft:absent");
        assert!(set.get(&missing).is_none());
        assert_eq!(first.load(Ordering::Relaxed), 2);
        assert_eq!(second.load(Ordering::Relaxed), 1);
        assert!(set.get(&missing).is_none());
        assert_eq!(first.load(Ordering::Relaxed), 2);
    }

    /// A source that always answers with the same little JSON object.
    struct JsonSource {
        hits: Arc<AtomicUsize>,
    }

    impl Source for JsonSource {
        fn get(&self, _id: &Identifier) -> Option<Vec<u8>> {
            self.hits.fetch_add(1, Ordering::Relaxed);
            Some(br#"{"a":"b"}"#.to_vec())
        }
    }

    #[test]
    fn a_json_answer_is_parsed_once_and_shared() {
        let hits = Arc::new(AtomicUsize::new(0));
        let mut set = ResourceSet::new();
        set.push(JsonSource { hits: hits.clone() });

        let id = Identifier::parse("minecraft:x");
        let first = set.get_json(&id).unwrap();
        assert_eq!(first.get("a").and_then(|value| value.as_str()), Some("b"));

        /* The same parsed value comes back, without touching the source again. */
        let second = set.get_json(&id).unwrap();
        assert!(Arc::ptr_eq(&first, &second));
        assert_eq!(hits.load(Ordering::Relaxed), 1);
    }
}
