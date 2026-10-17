/*! The data files dhlrc carries inside itself.
 *
 * Embedding them means neither frontend has to locate a data directory, and
 * both read the same bytes: the Qt GUI asks through the C ABI
 * (`dhlrc_core_bundled_resource`), the terminal frontend calls [`get`]
 * directly. Keeping them here, rather than in a Qt `.qrc`, is what makes them
 * readable from the TUI at all.
 *
 * They are the data the *core* needs: the block-to-item table behind the
 * material list, and the `data version -> version id` table used to choose an
 * asset index. Purely presentational assets (icons) stay with the GUI.
 */

/// The `data version -> version id` table, used to pick a version's asset index.
const DATA_VERSION_CSV: &str = include_str!("../data/data_version.csv");
/// The block-to-item mapping the material list is built from. Schema 2: exact
/// tables first, then ordered pattern rules (one `*` wildcard) for whole
/// families such as wall signs and potted plants. `scripts/gen_handle_block.py`
/// validates it against real client jars — run its `check` mode after editing.
const HANDLE_BLOCK_JSON: &str = include_str!("../data/handle_block.json");

/// Every bundled file, by the name it is looked up with.
const FILES: &[(&str, &str)] = &[
    ("data_version.csv", DATA_VERSION_CSV),
    ("handle_block.json", HANDLE_BLOCK_JSON),
];

/// The bytes of the bundled file `name`, or `None` when there is no such file.
pub fn get(name: &str) -> Option<&'static [u8]> {
    FILES
        .iter()
        .find(|(file, _)| *file == name)
        .map(|(_, text)| text.as_bytes())
}

/// The names of every bundled file.
pub fn names() -> impl Iterator<Item = &'static str> {
    FILES.iter().map(|(name, _)| *name)
}

/// The `data version -> version id` table.
pub fn data_version_csv() -> &'static str {
    DATA_VERSION_CSV
}

/// The block-to-item mapping table.
pub fn handle_block_json() -> &'static str {
    HANDLE_BLOCK_JSON
}

#[cfg(test)]
mod tests {
    use super::{get, handle_block_json, names};

    #[test]
    fn files_are_reachable_by_name() {
        assert!(get("data_version.csv").is_some());
        assert!(get("handle_block.json").is_some());
        assert!(get("nope").is_none());
        assert_eq!(names().count(), 2);
    }

    #[test]
    fn the_block_table_is_schema_3() {
        /* A dependency-free sanity check that the right file was embedded, plus
         * the shape the material-list code relies on: the three exact tables,
         * the ordered pattern rules, the one-block-to-many-items rules and the
         * items that put down two block positions. */
        let value: serde_json::Value =
            serde_json::from_str(handle_block_json()).expect("bundled handle_block.json is valid JSON");
        assert_eq!(value["schema"], 3);
        for key in [
            "ignore_block",
            "replace_single_block",
            "replace_multi_block",
            "pattern_rules",
            "units_rules",
            "pair_items",
        ] {
            assert!(value.get(key).is_some(), "handle_block.json has no {key}");
        }
        let rules = value["pattern_rules"].as_array().expect("pattern_rules is an array");
        assert!(!rules.is_empty(), "pattern_rules is empty");
        for rule in rules {
            for key in ["match", "action", "to"] {
                assert!(rule.get(key).is_some(), "pattern rule {rule} has no {key}");
            }
            let pattern = rule["match"].as_str().expect("match is a string");
            assert_eq!(
                pattern.matches('*').count(),
                1,
                "pattern {pattern} must contain exactly one wildcard"
            );
        }
        let units = value["units_rules"].as_array().expect("units_rules is an array");
        assert!(!units.is_empty(), "units_rules is empty");
        for rule in units {
            for key in ["match", "property"] {
                assert!(rule.get(key).is_some(), "units rule {rule} has no {key}");
            }
        }
        let pairs = value["pair_items"].as_array().expect("pair_items is an array");
        assert!(!pairs.is_empty(), "pair_items is empty");
    }

    #[test]
    fn the_block_table_drops_pre_flattening_ids() {
        /* 1.13 flattened the block ids; `flowing_water` and friends no longer
         * exist, so a table that still names them is stale. */
        let table = handle_block_json();
        for legacy in ["minecraft:flowing_water", "minecraft:flowing_lava"] {
            assert!(!table.contains(legacy), "handle_block.json still names {legacy}");
        }
    }
}
