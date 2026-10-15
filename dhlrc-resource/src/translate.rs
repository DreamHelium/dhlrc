/*! Turning a block or item id into its localized name.
 *
 * A Minecraft language file is a flat JSON map from a key to text, and a block's
 * key is `block.<namespace>.<path>` (`block.minecraft.oak_log`). The file for a
 * language is `assets/<namespace>/lang/<locale>.json`, so the lookup goes
 * through the resource set — which is why a modded block resolves out of the
 * mod's own jar, while a vanilla one comes from the downloaded index even
 * though the client jar only carries `en_us`.
 */

use super::{Identifier, ResourceSet};

/// The localized name of `name` (`minecraft:oak_log`) in `locale` (`zh_cn`).
/// `None` when no language file has it.
pub fn translate(resources: &ResourceSet, locale: &str, name: &str) -> Option<String> {
    let id = Identifier::parse(name);
    let block = format!("block.{}.{}", id.namespace, id.path);
    let item = format!("item.{}.{}", id.namespace, id.path);

    /* The block's own namespace first, then vanilla: a mod's file holds its own
     * keys, and a few mods add keys under `minecraft`. */
    let mut namespaces = vec![id.namespace.clone()];
    if id.namespace != "minecraft" {
        namespaces.push("minecraft".to_owned());
    }

    for namespace in namespaces {
        let file = Identifier::new(namespace, format!("lang/{locale}.json"));
        let Some(json) = resources.get_json(&file) else {
            continue;
        };
        for key in [&block, &item] {
            if let Some(value) = json.get(key).and_then(|value| value.as_str()) {
                return Some(value.to_owned());
            }
        }
    }
    None
}

#[cfg(test)]
mod tests {
    use super::translate;
    use crate::{DirSource, ResourceSet};

    /// A pack whose `zh_cn` file names a vanilla block and a modded one.
    ///
    /// Each test gets its own directory: they run on threads of one process, so
    /// a name made of the process id alone would have one test truncating the
    /// file another is reading.
    fn resources(tag: &str) -> ResourceSet {
        let root = std::env::temp_dir().join(format!(
            "dhlrc-translate-{}-{tag}",
            std::process::id()
        ));
        let minecraft = root.join("assets/minecraft/lang/zh_cn.json");
        std::fs::create_dir_all(minecraft.parent().unwrap()).unwrap();
        std::fs::write(&minecraft, r#"{"block.minecraft.oak_log":"橡木原木"}"#).unwrap();
        let modded = root.join("assets/examplemod/lang/zh_cn.json");
        std::fs::create_dir_all(modded.parent().unwrap()).unwrap();
        std::fs::write(&modded, r#"{"block.examplemod.cogwheel":"齿轮"}"#).unwrap();

        let mut set = ResourceSet::new();
        set.push(DirSource::new(&root));
        set
    }

    #[test]
    fn a_vanilla_block_is_named_from_the_language_file() {
        let set = resources("vanilla");
        assert_eq!(
            translate(&set, "zh_cn", "minecraft:oak_log").as_deref(),
            Some("橡木原木")
        );
    }

    #[test]
    fn a_modded_block_is_named_from_its_own_namespace() {
        let set = resources("modded");
        assert_eq!(
            translate(&set, "zh_cn", "examplemod:cogwheel").as_deref(),
            Some("齿轮")
        );
    }

    #[test]
    fn an_unknown_block_or_locale_has_no_name() {
        let set = resources("unknown");
        assert!(translate(&set, "zh_cn", "minecraft:dirt").is_none());
        assert!(translate(&set, "fr_fr", "minecraft:oak_log").is_none());
    }
}
