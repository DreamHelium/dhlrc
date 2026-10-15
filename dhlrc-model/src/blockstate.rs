/*! A block's states: which model each combination of properties draws.
 *
 * `blockstates/<block>.json` does it one of two ways. `variants` maps a
 * property string (`facing=north,half=top`) to one model, or to several with
 * weights. `multipart` instead lists pieces, each drawn when its `when`
 * condition holds — a fence's post and one side per neighbour, a wall's
 * height per side, a vine's face per face. Both are read here; which of them a
 * block uses is the block's business, and a few packs use both.
 */

use std::collections::BTreeMap;

use serde_json::Value;

use crate::Error;

/// One model a state draws, with the rotation the blockstate asks for.
#[derive(Debug, Clone, PartialEq, Eq, Default)]
pub struct Variant {
    /// The model id (`minecraft:block/stone`).
    pub model: String,
    /// Rotation about x, in degrees (0, 90, 180 or 270).
    pub x: i32,
    /// Rotation about y, in degrees.
    pub y: i32,
    /// Whether the UVs turn with the block instead of staying axis-aligned.
    pub uvlock: bool,
}

/// One `multipart` piece: what it draws, and when.
#[derive(Debug, Clone)]
pub struct Multipart {
    /// The properties that have to hold for the piece to be drawn, or `None`
    /// for a piece that is always drawn (a fence's post).
    pub when: Option<Condition>,
    /// The models drawn, each with its own rotation.
    pub apply: Vec<Variant>,
}

/// A `when` condition, as `multipart` writes it.
#[derive(Debug, Clone)]
pub enum Condition {
    /// One property has to be one of these values.
    Property(String, Vec<String>),
    /// Every one of these has to hold.
    All(Vec<Condition>),
    /// Any one of these has to hold (`OR`).
    Any(Vec<Condition>),
}

impl Condition {
    /// Whether the condition holds for a state's properties.
    pub fn matches(&self, properties: &BTreeMap<&str, &str>) -> bool {
        match self {
            Condition::Property(name, values) => properties
                .get(name.as_str())
                .is_some_and(|value| values.iter().any(|wanted| wanted == value)),
            Condition::All(conditions) => conditions.iter().all(|one| one.matches(properties)),
            Condition::Any(conditions) => conditions.iter().any(|one| one.matches(properties)),
        }
    }
}

/// A block's states.
#[derive(Debug, Clone, Default)]
pub struct BlockState {
    /// Property string to the models drawn for it. `""` is the state with no
    /// properties set, which is what a block placed with none uses.
    pub variants: BTreeMap<String, Vec<Variant>>,
    /// The `multipart` pieces, in the order the file lists them.
    pub multipart: Vec<Multipart>,
}

impl BlockState {
    /// The models drawn for the state with no properties.
    pub fn default_variant(&self) -> &[Variant] {
        self.variant("")
    }

    /// The models drawn for the state whose properties are `properties`
    /// (`facing=north,half=top`); the default state when it has none.
    ///
    /// A key only names the properties it cares about: `oak_slab`'s states are
    /// keyed `type=bottom`, `type=top` and `type=double`, while the block also
    /// has a `waterlogged` property that its model does not depend on. So a key
    /// matches when every property it names has the value the state has —
    /// extra properties on the state are ignored, and when several keys match
    /// the most specific one wins. The state with no properties is what the
    /// keys of a block whose model never changes are written as, so it matches
    /// everything and is used when nothing more specific does.
    pub fn variant(&self, properties: &str) -> &[Variant] {
        let wanted = properties_map(properties);
        let mut best: Option<(usize, &Vec<Variant>)> = None;
        for (key, variants) in &self.variants {
            let named = properties_map(key);
            let matches = named
                .iter()
                .all(|(name, value)| wanted.get(name) == Some(value));
            if !matches {
                continue;
            }
            if best.is_none_or(|(count, _)| named.len() > count) {
                best = Some((named.len(), variants));
            }
        }
        best.map(|(_, variants)| variants.as_slice())
            .unwrap_or_default()
    }
}

/// The property string a state is keyed by (`facing=north,half=top`), built
/// from the name/value pairs a region stores for a block.
pub fn property_key<'a>(properties: impl IntoIterator<Item = (&'a str, &'a str)>) -> String {
    let mut key = String::new();
    for (name, value) in properties {
        if !key.is_empty() {
            key.push(',');
        }
        key.push_str(name);
        key.push('=');
        key.push_str(value);
    }
    key
}

/// The properties of a state string (`facing=north,half=top`) as a map, so a
/// key can be matched however its properties are ordered and a `when` condition
/// can be asked about one of them.
pub fn properties_map(properties: &str) -> BTreeMap<&str, &str> {
    properties
        .split(',')
        .filter_map(|pair| pair.split_once('='))
        .collect()
}

pub(crate) fn parse(text: &str) -> Result<BlockState, Error> {
    let value: Value =
        serde_json::from_str(text).map_err(|error| Error::Json(error.to_string()))?;
    let object = value
        .as_object()
        .ok_or_else(|| Error::Shape("a blockstate is an object".to_owned()))?;

    let mut variants = BTreeMap::new();
    if let Some(entries) = object.get("variants") {
        let entries = entries
            .as_object()
            .ok_or_else(|| Error::Shape("`variants` is an object".to_owned()))?;
        for (properties, value) in entries {
            variants.insert(properties.clone(), variants_of(value)?);
        }
    }

    let multipart = match object.get("multipart") {
        Some(Value::Array(items)) => items
            .iter()
            .map(multipart_of)
            .collect::<Result<Vec<_>, _>>()?,
        Some(Value::Null) | None => Vec::new(),
        Some(_) => return Err(Error::Shape("`multipart` is an array".to_owned())),
    };

    Ok(BlockState {
        variants,
        multipart,
    })
}

/// One `multipart` entry.
fn multipart_of(value: &Value) -> Result<Multipart, Error> {
    let object = value
        .as_object()
        .ok_or_else(|| Error::Shape("a multipart entry is an object".to_owned()))?;
    let apply = object
        .get("apply")
        .ok_or_else(|| Error::Shape("a multipart entry has an `apply`".to_owned()))?;
    Ok(Multipart {
        when: object.get("when").map(condition_of).transpose()?,
        apply: variants_of(apply)?,
    })
}

/// A `when` condition. An object is a list of conditions that all have to
/// hold; `OR` and `AND` name a list of conditions instead.
fn condition_of(value: &Value) -> Result<Condition, Error> {
    let object = value
        .as_object()
        .ok_or_else(|| Error::Shape("a `when` is an object".to_owned()))?;

    let mut conditions = Vec::new();
    for (name, value) in object {
        conditions.push(match name.as_str() {
            "OR" => Condition::Any(condition_of_each(value)?),
            "AND" => Condition::All(condition_of_each(value)?),
            property => Condition::Property(property.to_owned(), values_of(value)),
        });
    }
    Ok(match conditions.len() {
        1 => conditions.remove(0),
        _ => Condition::All(conditions),
    })
}

/// The conditions of an `OR` / `AND` list.
fn condition_of_each(value: &Value) -> Result<Vec<Condition>, Error> {
    value
        .as_array()
        .ok_or_else(|| Error::Shape("`OR` and `AND` take an array".to_owned()))?
        .iter()
        .map(condition_of)
        .collect()
}

/// The values a property is allowed to have: `"a"`, `"a|b"` or `["a", "b"]`.
fn values_of(value: &Value) -> Vec<String> {
    match value {
        Value::Array(items) => items
            .iter()
            .filter_map(Value::as_str)
            .map(str::to_owned)
            .collect(),
        Value::String(text) => text.split('|').map(str::to_owned).collect(),
        other => vec![other.to_string()],
    }
}

/// A variant is one object, or an array of them when weighted.
fn variants_of(value: &Value) -> Result<Vec<Variant>, Error> {
    match value {
        Value::Array(items) => items.iter().map(variant_of).collect(),
        _ => Ok(vec![variant_of(value)?]),
    }
}

fn variant_of(value: &Value) -> Result<Variant, Error> {
    let object = value
        .as_object()
        .ok_or_else(|| Error::Shape("a variant is an object".to_owned()))?;
    let model = object
        .get("model")
        .and_then(Value::as_str)
        .ok_or_else(|| Error::Shape("a variant names a `model`".to_owned()))?;
    Ok(Variant {
        model: model.to_owned(),
        x: int(object.get("x")),
        y: int(object.get("y")),
        uvlock: object
            .get("uvlock")
            .and_then(Value::as_bool)
            .unwrap_or(false),
    })
}

fn int(value: Option<&Value>) -> i32 {
    value.and_then(Value::as_i64).unwrap_or(0) as i32
}

#[cfg(test)]
mod tests {
    use super::parse;

    #[test]
    fn a_variant_is_read_with_its_rotation() {
        let state = parse(
            r#"{"variants":{"":{"model":"minecraft:block/stone"},
                "facing=north":{"model":"minecraft:block/stairs","x":90,"y":180,"uvlock":true}}}"#,
        )
        .unwrap();

        assert_eq!(state.default_variant()[0].model, "minecraft:block/stone");

        let north = state.variant("facing=north");
        assert_eq!(north[0].model, "minecraft:block/stairs");
        assert_eq!(north[0].x, 90);
        assert_eq!(north[0].y, 180);
        assert!(north[0].uvlock);
    }

    #[test]
    fn an_unknown_state_falls_back_to_the_default() {
        let state = parse(r#"{"variants":{"":{"model":"minecraft:block/stone"}}}"#).unwrap();
        assert_eq!(state.variant("facing=up")[0].model, "minecraft:block/stone");
    }

    #[test]
    fn a_key_matches_a_state_that_has_more_properties() {
        /* `oak_slab`'s states are keyed `type=...`, while the block also has a
         * `waterlogged` property its model does not depend on: a key only has
         * to agree with the properties it names. */
        let state = parse(
            r#"{"variants":{
                "type=bottom":{"model":"minecraft:block/slab"},
                "type=top":{"model":"minecraft:block/slab_top"}}}"#,
        )
        .unwrap();
        assert_eq!(
            state.variant("type=bottom,waterlogged=false")[0].model,
            "minecraft:block/slab"
        );
        assert_eq!(
            state.variant("waterlogged=true,type=top")[0].model,
            "minecraft:block/slab_top"
        );
        assert!(state.variant("type=side").is_empty());
    }

    #[test]
    fn the_most_specific_matching_key_wins() {
        let state = parse(
            r#"{"variants":{
                "":{"model":"minecraft:block/off"},
                "powered=true":{"model":"minecraft:block/on"}}}"#,
        )
        .unwrap();
        assert_eq!(state.variant("powered=false")[0].model, "minecraft:block/off");
        assert_eq!(state.variant("powered=true")[0].model, "minecraft:block/on");
        assert_eq!(state.variant("")[0].model, "minecraft:block/off");
    }

    #[test]
    fn a_state_is_found_however_its_properties_are_ordered() {
        let state = parse(
            r#"{"variants":{"facing=north,half=top":{"model":"minecraft:block/x"}}}"#,
        )
        .unwrap();
        assert_eq!(state.variant("half=top,facing=north")[0].model, "minecraft:block/x");
    }

    #[test]
    fn a_key_is_built_from_the_pairs_in_order() {
        assert_eq!(
            super::property_key([("facing", "north"), ("half", "top")]),
            "facing=north,half=top"
        );
        assert_eq!(
            super::property_key(std::iter::empty::<(&str, &str)>()),
            ""
        );
    }

    #[test]
    fn a_weighted_variant_keeps_every_model() {
        let state = parse(
            r#"{"variants":{"":[{"model":"minecraft:block/a"},{"model":"minecraft:block/b"}]}}"#,
        )
        .unwrap();
        let models: Vec<&str> = state
            .default_variant()
            .iter()
            .map(|variant| variant.model.as_str())
            .collect();
        assert_eq!(models, ["minecraft:block/a", "minecraft:block/b"]);
    }

    #[test]
    fn a_multipart_state_has_no_variants() {
        let state = parse(r#"{"multipart":[{"apply":{"model":"minecraft:block/x"}}]}"#).unwrap();
        assert!(state.variants.is_empty());
        assert_eq!(state.multipart.len(), 1);
        assert!(state.multipart[0].when.is_none());
    }
}
