/*! The colour a face's `tintindex` multiplies its texture by.
 *
 * Grass, leaves and water are not painted into their textures: the game
 * multiplies a grey-green picture by a biome colour when it draws them, so the
 * same texture serves every biome. A block that does this asks for it with
 * `tintindex` in its model, and *which* colour that index names is decided by
 * the block, not by the index alone: on `grass_block` index 0 is the grass
 * colour, on `oak_leaves` it is the foliage colour, and on `water` it is the
 * water colour.
 *
 * A structure carries no biome, so the colour is the one the game uses for the
 * plains biome, read from the pack's own colormap when it has one.
 */

use dhlrc_resource::{Identifier, Source};

use crate::texture::Texture;

/// The colour provider a face's tint asks for.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Tint {
    Grass,
    Foliage,
    Water,
    /// Redstone dust and wire, whose colour follows its power.
    Redstone,
}

/// The plains biome, whose colour a structure without one is drawn with.
const TEMPERATURE: f32 = 0.8;
const DOWNFALL: f32 = 0.4;
/// What is used when the pack has no colormap of its own.
const PLAINS_GRASS: [u8; 3] = [0x91, 0xBD, 0x59];
const PLAINS_FOLIAGE: [u8; 3] = [0x77, 0xAB, 0x2F];
const WATER: [u8; 3] = [0x3F, 0x76, 0xE4];
const REDSTONE: [u8; 3] = [0xB3, 0x00, 0x00];

impl Tint {
    /// The tint `index` means on `block`, or `None` for a face the block does
    /// not tint.
    pub fn of(block: &Identifier, index: u32) -> Option<Tint> {
        /* Every block that tints at all uses index 0 or 1; a model with a
         * larger index belongs to a block that does not, and is drawn plain. */
        if index > 1 {
            return None;
        }
        let name = block.path.as_str();
        Some(match name {
            "water" | "bubble_column" | "water_cauldron" => Tint::Water,
            "redstone_wire" => Tint::Redstone,
            _ if name.ends_with("_leaves") => Tint::Foliage,
            _ => Tint::Grass,
        })
    }
}

/// The pack's grass and foliage colormaps, if it has any.
pub struct Colormaps {
    grass: Option<Texture>,
    foliage: Option<Texture>,
}

impl Colormaps {
    /// Reads the colormaps through `source`. A pack without them is not an
    /// error: the plains colours stand in.
    pub fn load(source: &dyn Source) -> Self {
        Self {
            grass: read(source, "minecraft:textures/colormap/grass.png"),
            foliage: read(source, "minecraft:textures/colormap/foliage.png"),
        }
    }

    /// The RGB a tint multiplies a texture by.
    pub fn color(&self, tint: Tint) -> [u8; 3] {
        match tint {
            Tint::Grass => self
                .grass
                .as_ref()
                .map(|map| sample(map))
                .unwrap_or(PLAINS_GRASS),
            Tint::Foliage => self
                .foliage
                .as_ref()
                .map(|map| sample(map))
                .unwrap_or(PLAINS_FOLIAGE),
            Tint::Water => WATER,
            Tint::Redstone => REDSTONE,
        }
    }
}

fn read(source: &dyn Source, id: &str) -> Option<Texture> {
    let bytes = source.get(&Identifier::parse(id))?;
    Texture::decode(&bytes, false).ok()
}

/// The colour of the plains biome: temperature and downfall pick a pixel of the
/// colormap, the way the game's `GrassColor.get` does.
fn sample(map: &Texture) -> [u8; 3] {
    let x = ((1.0 - TEMPERATURE) * 255.0).round();
    let y = ((1.0 - DOWNFALL * TEMPERATURE) * 255.0).round();
    let u = (x + 0.5) / 256.0;
    let v = (y + 0.5) / 256.0;
    let pixel = map.sample(u, v);
    [pixel[0], pixel[1], pixel[2]]
}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;

    use dhlrc_resource::{Identifier, Source};

    use super::{Colormaps, Tint};

    /// A source holding the named files as raw bytes.
    #[derive(Default)]
    struct Files(BTreeMap<String, Vec<u8>>);

    impl Source for Files {
        fn get(&self, id: &Identifier) -> Option<Vec<u8>> {
            self.0.get(&id.to_string()).cloned()
        }
    }

    /// A 256 by 256 colormap that is one colour everywhere.
    fn colormap(color: [u8; 4]) -> Vec<u8> {
        let mut bytes = Vec::new();
        {
            let mut encoder = png::Encoder::new(&mut bytes, 256, 256);
            encoder.set_color(png::ColorType::Rgba);
            encoder.set_depth(png::BitDepth::Eight);
            let mut writer = encoder.write_header().unwrap();
            writer
                .write_image_data(&color.repeat(256 * 256))
                .unwrap();
        }
        bytes
    }

    #[test]
    fn the_block_decides_what_an_index_means() {
        assert_eq!(
            Tint::of(&Identifier::parse("minecraft:grass_block"), 0),
            Some(Tint::Grass)
        );
        assert_eq!(
            Tint::of(&Identifier::parse("minecraft:oak_leaves"), 0),
            Some(Tint::Foliage)
        );
        assert_eq!(
            Tint::of(&Identifier::parse("minecraft:water"), 0),
            Some(Tint::Water)
        );
        /* A block that tints without naming a colour of its own is grass-like:
         * a face only asks at all because its model carries a `tintindex`. */
        assert_eq!(
            Tint::of(&Identifier::parse("minecraft:sugar_cane"), 0),
            Some(Tint::Grass)
        );
        assert_eq!(Tint::of(&Identifier::parse("minecraft:stone"), 2), None);
    }

    #[test]
    fn a_pack_s_colormap_is_used_and_a_missing_one_falls_back() {
        let empty = Colormaps::load(&Files::default());
        assert_eq!(empty.color(Tint::Grass), super::PLAINS_GRASS);

        let mut files = Files::default();
        files.0.insert(
            "minecraft:textures/colormap/grass.png".to_owned(),
            colormap([10, 20, 30, 255]),
        );
        let maps = Colormaps::load(&files);
        assert_eq!(maps.color(Tint::Grass), [10, 20, 30]);
        assert_eq!(maps.color(Tint::Foliage), super::PLAINS_FOLIAGE);
        assert_eq!(maps.color(Tint::Water), super::WATER);
    }
}
