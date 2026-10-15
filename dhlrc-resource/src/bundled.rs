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
/// The block-to-item mapping the material list is built from.
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
    fn the_block_table_mentions_ignored_blocks() {
        /* A dependency-free sanity check that the right file was embedded. */
        assert!(handle_block_json().contains("ignore_block"));
    }
}
