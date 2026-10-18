/*! The blocks a version has, as a frontend's chooser needs them.
 *
 * The list is read out of the resources the core is configured with — a
 * version's client jar, and whatever packs sit on top of it — and cached, since
 * a chooser asks for it once and then walks it. Everything is kept as C strings
 * so the pointers handed to a C caller stay put for as long as the list does.
 */

use std::ffi::CString;
use std::sync::Arc;

use dhlrc_model::BlockDef;

/// One block, ready to hand to C.
pub struct BlockEntry {
    pub name: CString,
    /// Each property's name, with every value it takes.
    pub properties: Vec<(CString, Vec<CString>)>,
}

/// The whole catalog, built once per resource configuration.
pub struct Catalog {
    pub blocks: Vec<BlockEntry>,
}

impl Catalog {
    /// Reads the catalog out of `source`.
    pub fn build(source: &dyn dhlrc_resource::Source) -> Arc<Self> {
        let blocks = dhlrc_model::catalog(source);
        Arc::new(Self {
            blocks: blocks.iter().map(entry_of).collect(),
        })
    }
}

fn entry_of(block: &BlockDef) -> BlockEntry {
    BlockEntry {
        name: cstring(&block.name),
        properties: block
            .properties
            .iter()
            .map(|(name, values)| {
                (
                    cstring(name),
                    values.iter().map(|value| cstring(value)).collect(),
                )
            })
            .collect(),
    }
}

/// A string for C. A name with a NUL in it cannot come out of JSON, but there
/// is no reason to panic over one that somehow did.
fn cstring(text: &str) -> CString {
    CString::new(text).unwrap_or_default()
}
