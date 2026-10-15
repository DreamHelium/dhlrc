/*! A namespaced resource path, as Minecraft addresses everything.
 *
 * Every block, texture, model and language entry is named `namespace:path`
 * (`minecraft:textures/block/stone.png`). Mods use their own namespace, so
 * nothing here may assume `minecraft` beyond the defaulting rule.
 */

use std::fmt::{self, Display, Formatter};

/// The namespace an id without one belongs to, as in Minecraft.
pub const DEFAULT_NAMESPACE: &str = "minecraft";

/// A `namespace:path` pair.
#[derive(Debug, Clone, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub struct Identifier {
    pub namespace: String,
    pub path: String,
}

impl Identifier {
    pub fn new(namespace: impl Into<String>, path: impl Into<String>) -> Self {
        Self {
            namespace: namespace.into(),
            path: path.into(),
        }
    }

    /// Parses `namespace:path`; a value without a colon is taken to be in
    /// [`DEFAULT_NAMESPACE`].
    pub fn parse(value: &str) -> Self {
        match value.split_once(':') {
            Some((namespace, path)) => Self::new(namespace, path),
            None => Self::new(DEFAULT_NAMESPACE, value),
        }
    }

    /// Where the file lives inside a resource pack: `assets/<namespace>/<path>`.
    pub fn asset_path(&self) -> String {
        format!("assets/{}/{}", self.namespace, self.path)
    }
}

impl Display for Identifier {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        write!(f, "{}:{}", self.namespace, self.path)
    }
}

#[cfg(test)]
mod tests {
    use super::Identifier;

    #[test]
    fn a_bare_path_gets_the_default_namespace() {
        assert_eq!(
            Identifier::parse("block/stone"),
            Identifier::new("minecraft", "block/stone")
        );
    }

    #[test]
    fn an_explicit_namespace_is_kept() {
        let id = Identifier::parse("create:block/cogwheel");
        assert_eq!(id.namespace, "create");
        assert_eq!(id.path, "block/cogwheel");
        assert_eq!(id.to_string(), "create:block/cogwheel");
    }

    #[test]
    fn asset_path_uses_the_pack_layout() {
        assert_eq!(
            Identifier::parse("minecraft:textures/block/stone.png").asset_path(),
            "assets/minecraft/textures/block/stone.png"
        );
    }
}
