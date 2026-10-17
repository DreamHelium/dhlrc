/*! The material list: what a region's blocks ask the player to prepare.
 *
 * A structure stores block states, not shopping lists. This module turns one
 * into the other: it counts each palette entry once (not each block state, so
 * `oak_stairs[facing=north]` and `[facing=east]` add up), asks the rule table in
 * [`crate::resource::handle_block`] what each block turns into, and sums the
 * result per item.
 *
 * Two corrections happen on the way, and both matter because getting them wrong
 * is silent:
 *
 * - a block state can stand for several items (a double slab is two slabs, a
 *   snow block with eight layers is eight snow), so the count is multiplied by
 *   [`Rules::units_for`];
 * - one item can put down two block positions (a door, a bed, a tall plant, a
 *   big dripleaf), so an item in [`Rules::is_pair_item`] has its block count
 *   halved, rounded up.
 *
 * The result keeps its sources, so a frontend can explain a number instead of
 * just showing it: which blocks contributed, how many, and which part of the
 * table decided it. That is also why the palette is counted here rather than in
 * a frontend — reaching one block at a time through the C ABI would cost a call
 * per block.
 *
 * The JSON shape is:
 *
 * ```json
 * {
 *   "schema": 1,
 *   "blocks_total": 4096,
 *   "items": [
 *     {"id": "minecraft:oak_door", "count": 1, "blocks": 2, "pair": true,
 *      "sources": [{"block": "minecraft:oak_door", "blocks": 2, "rule": "identity"}]}
 *   ],
 *   "ignored": [{"block": "minecraft:air", "count": 4092}]
 * }
 * ```
 */

use crate::resource::bundled;
use crate::resource::handle_block::{Resolution, Rule, Rules};
use common_rs::region::Region;
use serde_json::{Value, json};
use std::collections::HashMap;
use std::sync::OnceLock;

/// The parsed rule table, built once from the embedded bytes.
fn rules() -> Option<&'static Rules> {
    static RULES: OnceLock<Option<Rules>> = OnceLock::new();
    RULES
        .get_or_init(|| Rules::from_json(bundled::handle_block_json()))
        .as_ref()
}

#[derive(Default)]
struct ItemEntry {
    /// Items needed, before the two-block correction.
    units: u64,
    /// Block positions that map to this item.
    blocks: u64,
    sources: HashMap<String, SourceEntry>,
}

#[derive(Default)]
struct SourceEntry {
    blocks: u64,
    rule: Rule,
}

/// Builds the material list of one region as JSON.
///
/// A table that fails to parse degrades to "every block is its own item", which
/// is the same answer the table gives for blocks it does not mention, so a
/// broken embed still produces a usable list instead of an error.
pub fn material_json(region: &Region) -> String {
    let rules = rules();

    // One pass over the block array, counting palette ids. The palette is small
    // (tens to hundreds of entries) even for a huge region.
    let mut counts = vec![0u64; region.palette_array.len()];
    for &id in &region.block_array {
        if let Some(count) = counts.get_mut(id as usize) {
            *count += 1;
        }
    }

    let mut items: HashMap<String, ItemEntry> = HashMap::new();
    let mut ignored: HashMap<String, u64> = HashMap::new();

    for (id, count) in counts.iter().enumerate() {
        if *count == 0 {
            continue;
        }
        // The palette stores the block id and its properties apart; strip the
        // properties anyway so a format that packs them into the name still
        // matches the rule table.
        let palette = &region.palette_array[id];
        let block = palette
            .id_name
            .split('[')
            .next()
            .unwrap_or("")
            .trim();
        if block.is_empty() {
            continue;
        }

        let resolution = match rules {
            Some(rules) => rules.resolve(block),
            None => Resolution::Items {
                items: vec![block.to_string()],
                rule: Rule::Identity,
            },
        };
        // A block state can stand for more than one item (double slab, snow
        // layers, candles); the property lives in the palette, not the name.
        let units = rules.map_or(1, |rules| rules.units_for(block, &palette.property));

        match resolution {
            Resolution::Ignore => {
                *ignored.entry(block.to_string()).or_default() += *count;
            }
            Resolution::Items {
                items: targets,
                rule,
            } => {
                for target in targets {
                    let entry = items.entry(target).or_default();
                    entry.units = entry
                        .units
                        .saturating_add(count.saturating_mul(units));
                    entry.blocks = entry.blocks.saturating_add(*count);
                    let source = entry.sources.entry(block.to_string()).or_default();
                    source.blocks = source.blocks.saturating_add(*count);
                    source.rule = rule.clone();
                }
            }
        }
    }

    let mut item_rows: Vec<Value> = items
        .into_iter()
        .map(|(id, entry)| {
            let pair = rules.is_some_and(|rules| rules.is_pair_item(&id));
            // A door, a bed and a tall plant are two block positions for one
            // item; round up so a lone half still needs one.
            let count = if pair {
                entry.units.div_ceil(2)
            } else {
                entry.units
            };
            let mut sources: Vec<(String, SourceEntry)> = entry.sources.into_iter().collect();
            sources.sort_by(|a, b| b.1.blocks.cmp(&a.1.blocks).then_with(|| a.0.cmp(&b.0)));
            json!({
                "id": id,
                "count": count,
                "blocks": entry.blocks,
                "pair": pair,
                "sources": sources
                    .into_iter()
                    .map(|(block, source)| json!({
                        "block": block,
                        "blocks": source.blocks,
                        "rule": source.rule.label(),
                    }))
                    .collect::<Vec<_>>(),
            })
        })
        .collect();
    item_rows.sort_by(|a, b| {
        let count_a = a["count"].as_u64().unwrap_or(0);
        let count_b = b["count"].as_u64().unwrap_or(0);
        count_b.cmp(&count_a).then_with(|| {
            a["id"]
                .as_str()
                .unwrap_or("")
                .cmp(b["id"].as_str().unwrap_or(""))
        })
    });

    let mut ignored_rows: Vec<(String, u64)> = ignored.into_iter().collect();
    ignored_rows.sort_by(|a, b| b.1.cmp(&a.1).then_with(|| a.0.cmp(&b.0)));

    json!({
        "schema": 1,
        "blocks_total": region.block_array.len() as u64,
        "items": item_rows,
        "ignored": ignored_rows
            .into_iter()
            .map(|(block, count)| json!({"block": block, "count": count}))
            .collect::<Vec<_>>(),
    })
    .to_string()
}

#[cfg(test)]
mod tests {
    use super::material_json;
    use common_rs::region::{BaseData, Palette, Region};
    use serde_json::Value;
    use time::UtcDateTime;

    fn region(palette: &[&str], blocks: &[u32]) -> Region {
        let states: Vec<(&str, &[(&str, &str)])> =
            palette.iter().map(|name| (*name, &[][..])).collect();
        region_states(&states, blocks)
    }

    /// Like [`region`], with the block state properties of each palette entry.
    fn region_states(states: &[(&str, &[(&str, &str)])], blocks: &[u32]) -> Region {
        let now = UtcDateTime::now();
        Region {
            data_version: 0,
            base_data: BaseData {
                create_time: now,
                modify_time: now,
                description: String::new(),
                author: String::new(),
                name: String::new(),
                region_name: String::new(),
            },
            region_size: (1, 1, 1),
            region_offset: (0, 0, 0),
            block_array: blocks.to_vec(),
            block_entity_array: vec![],
            entity_array: vec![],
            palette_array: states
                .iter()
                .map(|(name, properties)| Palette {
                    id_name: name.to_string(),
                    property: properties
                        .iter()
                        .map(|(key, value)| (key.to_string(), value.to_string()))
                        .collect(),
                })
                .collect(),
            biome_array: vec![],
            biome_palette_array: vec![],
            format_data: None,
            format_version: 0,
        }
    }

    fn item<'a>(list: &'a Value, id: &str) -> Option<&'a Value> {
        list["items"]
            .as_array()?
            .iter()
            .find(|item| item["id"] == id)
    }

    #[test]
    fn counts_are_per_item_with_their_sources() {
        let list: Value = serde_json::from_str(&material_json(&region(
            &[
                "minecraft:oak_stairs",
                "minecraft:air",
                "minecraft:acacia_wall_sign",
                "minecraft:white_candle_cake",
            ],
            // 2 stairs, 1 air, 1 wall sign, 2 candle cakes
            &[0, 0, 1, 2, 3, 3],
        )))
        .expect("the list is JSON");

        assert_eq!(list["blocks_total"], 6);
        assert_eq!(item(&list, "minecraft:oak_stairs").unwrap()["count"], 2);
        // A wall sign is built from the plain sign.
        assert_eq!(item(&list, "minecraft:acacia_sign").unwrap()["count"], 1);
        // A candle cake is two items, each counted once per block.
        assert_eq!(item(&list, "minecraft:cake").unwrap()["count"], 2);
        assert_eq!(item(&list, "minecraft:white_candle").unwrap()["count"], 2);
        // Air is not something anyone prepares.
        assert!(item(&list, "minecraft:air").is_none());
        assert_eq!(list["ignored"][0]["block"], "minecraft:air");
        assert_eq!(list["ignored"][0]["count"], 1);
    }

    #[test]
    fn a_source_explains_the_number() {
        let list: Value = serde_json::from_str(&material_json(&region(
            &["minecraft:acacia_wall_sign"],
            &[0],
        )))
        .expect("the list is JSON");
        let sign = item(&list, "minecraft:acacia_sign").unwrap();
        let source = &sign["sources"][0];
        assert_eq!(source["block"], "minecraft:acacia_wall_sign");
        assert_eq!(source["blocks"], 1);
        assert_eq!(source["rule"], "pattern:minecraft:*_wall_sign");
    }

    #[test]
    fn block_state_properties_do_not_split_a_block() {
        let list: Value = serde_json::from_str(&material_json(&region(
            &[
                "minecraft:oak_stairs[facing=north]",
                "minecraft:oak_stairs[facing=south]",
            ],
            &[0, 1, 1],
        )))
        .expect("the list is JSON");
        assert_eq!(item(&list, "minecraft:oak_stairs").unwrap()["count"], 3);
    }

    #[test]
    fn a_door_is_two_blocks_but_one_item() {
        // A door occupies a lower and an upper block position; one door item
        // places both.
        let list: Value = serde_json::from_str(&material_json(&region(
            &["minecraft:oak_door"],
            &[0, 0, 0, 0],
        )))
        .expect("the list is JSON");
        let door = item(&list, "minecraft:oak_door").unwrap();
        assert_eq!(door["count"], 2);
        assert_eq!(door["blocks"], 4);
        assert_eq!(door["pair"], true);
    }

    #[test]
    fn a_lone_half_still_needs_one_item() {
        let list: Value = serde_json::from_str(&material_json(&region(
            &["minecraft:oak_door"],
            &[0],
        )))
        .expect("the list is JSON");
        assert_eq!(item(&list, "minecraft:oak_door").unwrap()["count"], 1);
    }

    #[test]
    fn block_state_counts_become_item_counts() {
        // A double slab is two slabs in one block; four snow layers are four
        // snow items; a candle block with three candles is three candles.
        let list: Value = serde_json::from_str(&material_json(&region_states(
            &[
                ("minecraft:oak_slab", &[("type", "double")]),
                ("minecraft:snow", &[("layers", "4")]),
                ("minecraft:white_candle", &[("candles", "3"), ("lit", "false")]),
                ("minecraft:stone_slab", &[("type", "bottom")]),
            ],
            &[0, 1, 2, 3],
        )))
        .expect("the list is JSON");
        assert_eq!(item(&list, "minecraft:oak_slab").unwrap()["count"], 2);
        assert_eq!(item(&list, "minecraft:snow").unwrap()["count"], 4);
        assert_eq!(item(&list, "minecraft:white_candle").unwrap()["count"], 3);
        assert_eq!(item(&list, "minecraft:stone_slab").unwrap()["count"], 1);
    }

    #[test]
    fn a_trapped_chest_is_counted_like_any_other_block() {
        /* A chest is an ordinary block: only its icon needs the game's special
         * renderer, its count must still be there. */
        let list: Value = serde_json::from_str(&material_json(&region(
            &["minecraft:trapped_chest"],
            &[0, 0],
        )))
        .expect("the list is JSON");
        assert_eq!(
            item(&list, "minecraft:trapped_chest").unwrap()["count"],
            2
        );
    }

    #[test]
    fn an_out_of_range_palette_id_is_ignored() {
        let list: Value = serde_json::from_str(&material_json(&region(
            &["minecraft:stone"],
            &[0, 9],
        )))
        .expect("the list is JSON");
        assert_eq!(list["blocks_total"], 2);
        assert_eq!(item(&list, "minecraft:stone").unwrap()["count"], 1);
    }
}

