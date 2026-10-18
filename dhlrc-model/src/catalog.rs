/*! The blocks a version has, and the properties each one takes.
 *
 * A block chooser needs this before it can offer anything. The block names come
 * from the blockstates a version carries (`assets/<namespace>/blockstates/
 * *.json`), and the properties each block takes — with every value they take —
 * are read out of the variant keys and the multipart conditions in those same
 * files.
 *
 * That is not the game's own block registry, but it is what the version's files
 * say, and this is what will be drawn: a value no file ever writes is a value
 * with no model behind it.
 */

use std::collections::{BTreeMap, BTreeSet};

use dhlrc_resource::Source;

use crate::blockstate::{self, BlockState, Condition};

/// One block of a version and the properties it takes.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct BlockDef {
    /// The block id, `minecraft:oak_stairs`.
    pub name: String,
    /// Each property its states name, with every value they use, in a readable
    /// order: numbers by value, everything else alphabetically.
    pub properties: Vec<(String, Vec<String>)>,
}

/// Every block the resources have a blockstate for, sorted by name.
///
/// A blockstate that will not parse is left out rather than failing the whole
/// list: one broken pack file should not cost the chooser every block.
pub fn catalog(source: &dyn Source) -> Vec<BlockDef> {
    let mut blocks = Vec::new();
    for id in source.list("blockstates/") {
        let Some(path) = id.path.strip_prefix("blockstates/") else {
            continue;
        };
        let Some(name) = path.strip_suffix(".json") else {
            continue;
        };
        let Some(text) = source
            .get(&id)
            .and_then(|bytes| String::from_utf8(bytes).ok())
        else {
            continue;
        };
        let Ok(state) = blockstate::parse(&text) else {
            continue;
        };
        blocks.push(BlockDef {
            name: format!("{}:{}", id.namespace, name),
            properties: properties_of(&state),
        });
    }
    blocks.sort_by(|a, b| a.name.cmp(&b.name));
    blocks
}

/// The properties `state` names, each with every value it uses.
fn properties_of(state: &BlockState) -> Vec<(String, Vec<String>)> {
    let mut values: BTreeMap<String, BTreeSet<String>> = BTreeMap::new();
    for key in state.variants.keys() {
        for part in key.split(',') {
            let Some((name, value)) = part.split_once('=') else {
                continue;
            };
            values
                .entry(name.trim().to_owned())
                .or_default()
                .insert(value.trim().to_owned());
        }
    }
    for piece in &state.multipart {
        if let Some(condition) = &piece.when {
            collect(condition, &mut values);
        }
    }
    values
        .into_iter()
        .map(|(name, values)| (name, ordered(values)))
        .collect()
}

fn collect(condition: &Condition, values: &mut BTreeMap<String, BTreeSet<String>>) {
    match condition {
        Condition::Property(name, used) => {
            let entry = values.entry(name.clone()).or_default();
            for value in used {
                entry.insert(value.clone());
            }
        }
        Condition::All(inner) | Condition::Any(inner) => {
            for one in inner {
                collect(one, values);
            }
        }
    }
}

/// The values of a property in a readable order: a numbered property (`level`
/// runs 0..15) reads by number, not with `10` before `2`.
fn ordered(values: BTreeSet<String>) -> Vec<String> {
    let mut out: Vec<String> = values.into_iter().collect();
    if let Some(numbers) = out
        .iter()
        .map(|value| value.parse::<i64>().ok())
        .collect::<Option<Vec<i64>>>()
    {
        out.sort_by_key(|value| value.parse::<i64>().unwrap_or(0));
        let _ = numbers;
    }
    out
}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;

    use dhlrc_resource::{Identifier, Source};

    use super::{BlockDef, catalog};

    /// A source of the named files, which can list them too.
    #[derive(Default)]
    struct Files(BTreeMap<String, Vec<u8>>);

    impl Files {
        fn with(mut self, id: &str, json: &str) -> Self {
            self.0.insert(id.to_owned(), json.as_bytes().to_vec());
            self
        }
    }

    impl Source for Files {
        fn get(&self, id: &Identifier) -> Option<Vec<u8>> {
            self.0.get(&id.to_string()).cloned()
        }

        fn list(&self, prefix: &str) -> Vec<Identifier> {
            let mut found: Vec<Identifier> = self
                .0
                .keys()
                .map(|key| Identifier::parse(key))
                .filter(|id| id.path.starts_with(prefix))
                .collect();
            found.sort_by(|a, b| a.path.cmp(&b.path));
            found
        }
    }

    #[test]
    fn the_catalog_lists_blocks_and_their_property_values() {
        let files = Files::default()
            /* A block with variants: the values are the ones its keys name. */
            .with(
                "minecraft:blockstates/oak_stairs.json",
                r#"{"variants":{
                    "facing=east,half=bottom,shape=straight,waterlogged=false":{"model":"minecraft:block/oak_stairs"},
                    "facing=west,half=top,shape=outer_left,waterlogged=true":{"model":"minecraft:block/oak_stairs"}}}"#,
            )
            /* A multipart block: the values come out of the conditions. */
            .with(
                "minecraft:blockstates/glass_pane.json",
                r#"{"multipart":[
                    {"apply":{"model":"minecraft:block/glass_pane_post"}},
                    {"when":{"north":"true"},"apply":{"model":"minecraft:block/glass_pane_side"}}]}"#,
            )
            /* A numbered property keeps its own order. */
            .with(
                "minecraft:blockstates/composter.json",
                r#"{"variants":{
                    "level=0":{"model":"minecraft:block/composter"},
                    "level=2":{"model":"minecraft:block/composter"},
                    "level=10":{"model":"minecraft:block/composter"}}}"#,
            )
            /* A broken file is skipped, not fatal. */
            .with("minecraft:blockstates/broken.json", "{");

        let blocks = catalog(&files);
        let names: Vec<&str> = blocks.iter().map(|block| block.name.as_str()).collect();
        assert_eq!(
            names,
            [
                "minecraft:composter",
                "minecraft:glass_pane",
                "minecraft:oak_stairs"
            ]
        );

        let stairs = &blocks[2].properties;
        assert_eq!(
            stairs,
            &[
                ("facing".to_owned(), vec!["east".to_owned(), "west".to_owned()]),
                ("half".to_owned(), vec!["bottom".to_owned(), "top".to_owned()]),
                (
                    "shape".to_owned(),
                    vec!["outer_left".to_owned(), "straight".to_owned()]
                ),
                (
                    "waterlogged".to_owned(),
                    vec!["false".to_owned(), "true".to_owned()]
                ),
            ]
        );

        assert_eq!(
            blocks[1].properties,
            [("north".to_owned(), vec!["true".to_owned()])]
        );
        assert_eq!(
            blocks[0].properties,
            [(
                "level".to_owned(),
                vec!["0".to_owned(), "2".to_owned(), "10".to_owned()]
            )]
        );
    }

    #[test]
    fn an_empty_set_gives_an_empty_catalog() {
        assert!(catalog(&Files::default()).is_empty());
        let _: BlockDef;
    }
}
