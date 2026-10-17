/*! The block-to-item rules behind the material list.
 *
 * A structure stores blocks; a player has to bring items. The two are not the
 * same, in three different ways:
 *
 * - a block can be built from something else (a wall sign from a sign, a crop
 *   from its seeds) — the exact tables and [`Rule::Pattern`] rows;
 * - one block can stand for several items (a double slab is two slabs, a snow
 *   block with eight layers is eight snow) — [`Rules::units_for`];
 * - one item can put down several blocks (a door, a bed and a tall plant are
 *   two block positions each, a big dripleaf a stem plus a head) —
 *   [`Rules::is_pair_item`].
 *
 * Getting any of these wrong makes the list silently wrong rather than empty,
 * which is why the table is validated against real client jars by
 * `scripts/gen_handle_block.py` and the schema is checked here.
 *
 * [`bundled::handle_block_json`]: crate::bundled::handle_block_json
 */

use serde_json::Value;
use std::collections::{HashMap, HashSet};

/// What a block turns into once the table has had its say.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Resolution {
    /// Nothing a player prepares: air, fire, a portal, a moving piston.
    Ignore,
    /// These items, where `rule` says which part of the table decided it.
    Items { items: Vec<String>, rule: Rule },
}

/// Which part of the table decided a block's items, so a frontend can explain
/// the number it shows.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Rule {
    /// The block id is the item id; the table said nothing about it.
    Identity,
    /// A row of `replace_single_block` or `replace_multi_block`.
    Exact,
    /// A `pattern_rules` entry, named by its `match` string.
    Pattern(String),
}

impl Default for Rule {
    fn default() -> Self {
        Rule::Identity
    }
}

impl Rule {
    /// The label the material list carries into its JSON.
    pub fn label(&self) -> String {
        match self {
            Rule::Identity => "identity".to_string(),
            Rule::Exact => "exact".to_string(),
            Rule::Pattern(pattern) => format!("pattern:{pattern}"),
        }
    }
}

/// The parsed table.
pub struct Rules {
    ignore: HashSet<String>,
    single: HashMap<String, String>,
    multi: HashMap<String, Vec<String>>,
    patterns: Vec<PatternRule>,
    units: Vec<UnitsRule>,
    pair_items: Vec<String>,
}

struct PatternRule {
    prefix: String,
    suffix: String,
    template: Vec<String>,
    ignore: bool,
    source: String,
}

/// A block state property that changes how many items one block stands for.
struct UnitsRule {
    pattern: String,
    prefix: String,
    suffix: String,
    property: String,
    /// When set, the rule only applies to this property value.
    value: Option<String>,
    /// When set, the item count is this; otherwise it is read from the property.
    units: Option<u64>,
}

impl Rules {
    /// Parses the embedded table. `None` when the file is not the schema this
    /// code understands, so a broken embed fails loudly in the caller instead
    /// of silently counting every block as itself.
    pub fn from_json(text: &str) -> Option<Self> {
        let value: Value = serde_json::from_str(text).ok()?;
        if value.get("schema")?.as_u64()? != 3 {
            return None;
        }
        let ignore = string_set(value.get("ignore_block")?)?;
        let single = string_map(value.get("replace_single_block")?)?;
        let multi = string_list_map(value.get("replace_multi_block")?)?;
        let patterns = value
            .get("pattern_rules")?
            .as_array()?
            .iter()
            .map(PatternRule::from_json)
            .collect::<Option<Vec<_>>>()?;
        let units = value
            .get("units_rules")?
            .as_array()?
            .iter()
            .map(UnitsRule::from_json)
            .collect::<Option<Vec<_>>>()?;
        let pair_items = string_set(value.get("pair_items")?)?
            .into_iter()
            .collect::<Vec<_>>();
        Some(Self {
            ignore,
            single,
            multi,
            patterns,
            units,
            pair_items,
        })
    }

    /// The items `block` turns into. `block` must already have its blockstate
    /// properties stripped; the region stores them apart from the name.
    pub fn resolve(&self, block: &str) -> Resolution {
        if self.ignore.contains(block) {
            return Resolution::Ignore;
        }
        if let Some(item) = self.single.get(block) {
            return Resolution::Items {
                items: vec![item.clone()],
                rule: Rule::Exact,
            };
        }
        if let Some(items) = self.multi.get(block) {
            return Resolution::Items {
                items: items.clone(),
                rule: Rule::Exact,
            };
        }
        for pattern in &self.patterns {
            let Some(captured) = strip(block, &pattern.prefix, &pattern.suffix) else {
                continue;
            };
            if captured.is_empty() {
                // In the table `*` means one or more characters.
                continue;
            }
            if pattern.ignore {
                return Resolution::Ignore;
            }
            let items = pattern
                .template
                .iter()
                .map(|template| template.replacen('*', captured, 1))
                .collect();
            return Resolution::Items {
                items,
                rule: Rule::Pattern(pattern.source.clone()),
            };
        }
        Resolution::Items {
            items: vec![block.to_string()],
            rule: Rule::Identity,
        }
    }

    /// How many items one `block` stands for, given its block state
    /// properties. `1` for almost everything; `2` for a double slab, the layer
    /// count for snow, the candle count for a candle block, and so on.
    pub fn units_for(&self, block: &str, properties: &[(String, String)]) -> u64 {
        for rule in &self.units {
            if !matches(&rule.pattern, &rule.prefix, &rule.suffix, block) {
                continue;
            }
            let property = properties
                .iter()
                .find(|(name, _)| *name == rule.property)
                .map(|(_, value)| value);
            if let Some(wanted) = &rule.value {
                if property != Some(wanted) {
                    continue;
                }
            }
            if let Some(units) = rule.units {
                return units.max(1);
            }
            if let Some(units) = property.and_then(|value| value.parse::<u64>().ok()) {
                return units.max(1);
            }
            // The rule matched but the state does not carry a usable count.
            return 1;
        }
        1
    }

    /// Whether one of this item puts down two block positions, so a count of
    /// block positions has to be halved (rounded up) to get the item count.
    pub fn is_pair_item(&self, item: &str) -> bool {
        self.pair_items.iter().any(|pattern| match split(pattern) {
            Some((prefix, suffix)) => matches(pattern, prefix, suffix, item),
            // An entry without a wildcard is an exact id.
            None => pattern == item,
        })
    }
}

impl PatternRule {
    fn from_json(rule: &Value) -> Option<Self> {
        let matches = rule.get("match")?.as_str()?;
        let (prefix, suffix) = split(matches)?;
        let action = rule.get("action")?.as_str()?;
        let ignore = match action {
            "ignore" => true,
            "single" | "multi" => false,
            _ => return None,
        };
        let template = match rule.get("to") {
            Some(Value::String(text)) => vec![text.clone()],
            Some(Value::Array(items)) => items
                .iter()
                .map(|item| item.as_str().map(str::to_string))
                .collect::<Option<Vec<_>>>()?,
            _ => return None,
        };
        if template.iter().filter(|text| text.contains('*')).count() > 1 {
            return None;
        }
        Some(Self {
            prefix: prefix.to_string(),
            suffix: suffix.to_string(),
            template,
            ignore,
            source: matches.to_string(),
        })
    }
}

impl UnitsRule {
    fn from_json(rule: &Value) -> Option<Self> {
        let pattern = rule.get("match")?.as_str()?;
        // The rule may name one block exactly, or a family with one wildcard.
        let (prefix, suffix) = split(pattern).unwrap_or(("", ""));
        let property = rule.get("property")?.as_str()?.to_string();
        let value = match rule.get("value") {
            Some(Value::String(text)) => Some(text.clone()),
            Some(Value::Number(number)) => Some(number.to_string()),
            Some(_) => return None,
            None => None,
        };
        let units = match rule.get("units") {
            Some(Value::Number(number)) => Some(number.as_u64()?),
            Some(_) => return None,
            None => None,
        };
        Some(Self {
            pattern: pattern.to_string(),
            prefix: prefix.to_string(),
            suffix: suffix.to_string(),
            property,
            value,
            units,
        })
    }
}

/// Splits a pattern into the text around its one `*`.
fn split(pattern: &str) -> Option<(&str, &str)> {
    let (prefix, suffix) = pattern.split_once('*')?;
    // Schema 3 allows exactly one wildcard.
    if suffix.contains('*') {
        return None;
    }
    Some((prefix, suffix))
}

fn matches(pattern: &str, prefix: &str, suffix: &str, value: &str) -> bool {
    if !pattern.contains('*') {
        return value == pattern;
    }
    value
        .strip_prefix(prefix)
        .and_then(|rest| rest.strip_suffix(suffix))
        .is_some_and(|captured| !captured.is_empty())
}

fn strip<'a>(value: &'a str, prefix: &str, suffix: &str) -> Option<&'a str> {
    value.strip_prefix(prefix)?.strip_suffix(suffix)
}

fn string_set(value: &Value) -> Option<HashSet<String>> {
    Some(
        value
            .as_array()?
            .iter()
            .map(|item| item.as_str().map(str::to_string))
            .collect::<Option<HashSet<_>>>()?,
    )
}

fn string_map(value: &Value) -> Option<HashMap<String, String>> {
    Some(
        value
            .as_object()?
            .iter()
            .map(|(key, item)| Some((key.clone(), item.as_str()?.to_string())))
            .collect::<Option<HashMap<_, _>>>()?,
    )
}

fn string_list_map(value: &Value) -> Option<HashMap<String, Vec<String>>> {
    Some(
        value
            .as_object()?
            .iter()
            .map(|(key, item)| {
                let list = item
                    .as_array()?
                    .iter()
                    .map(|entry| entry.as_str().map(str::to_string))
                    .collect::<Option<Vec<_>>>()?;
                Some((key.clone(), list))
            })
            .collect::<Option<HashMap<_, _>>>()?,
    )
}

#[cfg(test)]
mod tests {
    use super::{Resolution, Rule, Rules};
    use crate::bundled::handle_block_json;

    fn rules() -> Rules {
        Rules::from_json(handle_block_json()).expect("the embedded table is schema 3")
    }

    fn items(block: &str) -> (Vec<String>, Rule) {
        match rules().resolve(block) {
            Resolution::Items { items, rule } => (items, rule),
            Resolution::Ignore => panic!("{block} is ignored, not turned into items"),
        }
    }

    fn state(pairs: &[(&str, &str)]) -> Vec<(String, String)> {
        pairs
            .iter()
            .map(|(name, value)| (name.to_string(), value.to_string()))
            .collect()
    }

    #[test]
    fn a_block_the_table_does_not_mention_is_its_own_item() {
        let (found, rule) = items("minecraft:oak_stairs");
        assert_eq!(found, ["minecraft:oak_stairs"]);
        assert_eq!(rule, Rule::Identity);
    }

    #[test]
    fn exact_rows_replace_a_block_with_its_item() {
        assert_eq!(items("minecraft:wheat").0, ["minecraft:wheat_seeds"]);
        let (found, rule) = items("minecraft:candle_cake");
        assert_eq!(found, ["minecraft:cake", "minecraft:candle"]);
        assert_eq!(rule, Rule::Exact);
    }

    #[test]
    fn patterns_cover_whole_families() {
        let (found, rule) = items("minecraft:pale_oak_wall_sign");
        assert_eq!(found, ["minecraft:pale_oak_sign"]);
        assert_eq!(rule, Rule::Pattern("minecraft:*_wall_sign".to_string()));

        let (found, _) = items("minecraft:potted_dead_bush");
        assert_eq!(found, ["minecraft:flower_pot", "minecraft:dead_bush"]);

        let (found, _) = items("minecraft:water_cauldron");
        assert_eq!(found, ["minecraft:cauldron", "minecraft:water_bucket"]);
    }

    #[test]
    fn an_exact_row_wins_over_a_pattern() {
        // `potted_*` would give `azalea_bush`, which is not an item.
        let (found, rule) = items("minecraft:potted_azalea_bush");
        assert_eq!(found, ["minecraft:flower_pot", "minecraft:azalea"]);
        assert_eq!(rule, Rule::Exact);
    }

    #[test]
    fn ignored_blocks_are_not_items() {
        assert_eq!(rules().resolve("minecraft:air"), Resolution::Ignore);
        assert_eq!(rules().resolve("minecraft:end_portal"), Resolution::Ignore);
    }

    #[test]
    fn a_state_property_can_mean_several_items_per_block() {
        let rules = rules();
        assert_eq!(rules.units_for("minecraft:stone", &[]), 1);
        assert_eq!(
            rules.units_for("minecraft:oak_slab", &state(&[("type", "bottom")])),
            1
        );
        assert_eq!(
            rules.units_for("minecraft:oak_slab", &state(&[("type", "double")])),
            2
        );
        assert_eq!(
            rules.units_for("minecraft:snow", &state(&[("layers", "8")])),
            8
        );
        assert_eq!(
            rules.units_for("minecraft:white_candle", &state(&[("candles", "4")])),
            4
        );
        assert_eq!(
            rules.units_for("minecraft:sea_pickle", &state(&[("pickles", "3")])),
            3
        );
        // A matching block without the property still yields one.
        assert_eq!(rules.units_for("minecraft:snow", &[]), 1);
    }

    #[test]
    fn some_items_put_down_two_blocks() {
        let rules = rules();
        assert!(rules.is_pair_item("minecraft:oak_door"));
        assert!(rules.is_pair_item("minecraft:red_bed"));
        assert!(rules.is_pair_item("minecraft:big_dripleaf"));
        assert!(!rules.is_pair_item("minecraft:oak_stairs"));
        assert!(!rules.is_pair_item("minecraft:oak_trapdoor"));
    }

    #[test]
    fn a_broken_table_is_rejected_instead_of_guessed_around() {
        assert!(Rules::from_json("{}").is_none());
        assert!(Rules::from_json(r#"{"schema": 2}"#).is_none());
        assert!(
            Rules::from_json(r#"{"schema": 3, "ignore_block": [], "replace_single_block": {}, "replace_multi_block": {}, "pattern_rules": [], "units_rules": [], "pair_items": []}"#).is_some()
        );
        assert!(
            Rules::from_json(r#"{"schema": 3, "ignore_block": [], "replace_single_block": {}, "replace_multi_block": {}, "pattern_rules": [{"match": "a*b*c", "action": "single", "to": "x"}], "units_rules": [], "pair_items": []}"#).is_none()
        );
    }
}
