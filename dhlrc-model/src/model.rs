/*! A model file, before its parents are folded in.
 *
 * `models/<path>.json` is a list of boxes (in 0..16 model units, y up) with a
 * texture per face, plus the texture variables a face refers to. Everything but
 * `parent` may be absent, because a model often only overrides what it changes
 * and inherits the rest; folding that in is [`crate::resolve`]'s job.
 */

use std::collections::BTreeMap;

use serde_json::Value;

use crate::{Direction, Error};

/// A model file.
#[derive(Debug, Clone, Default)]
pub struct Model {
    /// The model this one inherits from, or `None`.
    pub parent: Option<String>,
    /// Texture variables, each a `#name` or a texture id
    /// (`minecraft:block/stone`).
    pub textures: BTreeMap<String, String>,
    /// The boxes drawn; `None` means the parent's are used unchanged.
    pub elements: Option<Vec<Element>>,
    /// Whether faces darken with the light level; `None` means the parent's.
    pub ambient_occlusion: Option<bool>,
    /// The raw `display` block, or `None`.
    pub display: Option<Value>,
}

/// One box of a model, in 0..16 model units.
#[derive(Debug, Clone)]
pub struct Element {
    pub from: [f32; 3],
    pub to: [f32; 3],
    pub rotation: Option<ElementRotation>,
    /// Whether the box is shaded by the direction its faces look.
    pub shade: bool,
    pub faces: BTreeMap<Direction, Face>,
}

/// A rotation applied to one element, about an axis through `origin`.
#[derive(Debug, Clone)]
pub struct ElementRotation {
    pub origin: [f32; 3],
    pub axis: Axis,
    /// In degrees; the game only uses multiples of 22.5.
    pub angle: f32,
    /// Whether the box grows along the axis it is not turned about, so corners
    /// keep their place.
    pub rescale: bool,
}

/// The axis a rotation or a face runs along.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Axis {
    X,
    Y,
    Z,
}

impl Axis {
    pub fn parse(name: &str) -> Option<Self> {
        Some(match name {
            "x" => Axis::X,
            "y" => Axis::Y,
            "z" => Axis::Z,
            _ => return None,
        })
    }
}

/// The texture and geometry one face of an element uses.
#[derive(Debug, Clone)]
pub struct Face {
    /// A `#variable`, or a texture id.
    pub texture: String,
    /// The UV rectangle in 0..16 model units, `[u1, v1, u2, v2]`; `None` means
    /// the face's own extent, the way the game defaults it.
    pub uv: Option<[f32; 4]>,
    /// The face this one is hidden behind, when a solid block is there.
    pub cullface: Option<Direction>,
    /// Rotation of the texture, in degrees (0, 90, 180 or 270).
    pub rotation: i32,
    /// The tint a block colormap (grass, leaves) applies, or `None`.
    pub tint: Option<u32>,
}

pub(crate) fn parse(text: &str) -> Result<Model, Error> {
    let value: Value =
        serde_json::from_str(text).map_err(|error| Error::Json(error.to_string()))?;
    let object = value
        .as_object()
        .ok_or_else(|| Error::Shape("a model is an object".to_owned()))?;

    Ok(Model {
        parent: object
            .get("parent")
            .and_then(Value::as_str)
            .map(str::to_owned),
        textures: textures(object.get("textures"))?,
        elements: match object.get("elements") {
            Some(Value::Array(items)) => Some(items.iter().map(element).collect::<Result<_, _>>()?),
            Some(Value::Null) | None => None,
            Some(_) => return Err(Error::Shape("`elements` is an array".to_owned())),
        },
        ambient_occlusion: object.get("ambientocclusion").and_then(Value::as_bool),
        display: object.get("display").cloned(),
    })
}

fn textures(value: Option<&Value>) -> Result<BTreeMap<String, String>, Error> {
    let mut textures = BTreeMap::new();
    let Some(value) = value else {
        return Ok(textures);
    };
    let object = value
        .as_object()
        .ok_or_else(|| Error::Shape("`textures` is an object".to_owned()))?;
    for (name, value) in object {
        if let Some(texture) = value.as_str() {
            textures.insert(name.clone(), texture.to_owned());
        }
    }
    Ok(textures)
}

fn element(value: &Value) -> Result<Element, Error> {
    let object = value
        .as_object()
        .ok_or_else(|| Error::Shape("an element is an object".to_owned()))?;

    let mut faces = BTreeMap::new();
    if let Some(entries) = object.get("faces") {
        let entries = entries
            .as_object()
            .ok_or_else(|| Error::Shape("`faces` is an object".to_owned()))?;
        for (name, value) in entries {
            let direction = Direction::parse(name)
                .ok_or_else(|| Error::Shape(format!("`{name}` is not a direction")))?;
            faces.insert(direction, face(value)?);
        }
    }

    Ok(Element {
        from: vec3(object.get("from"), [0.0; 3])?,
        to: vec3(object.get("to"), [16.0; 3])?,
        rotation: object.get("rotation").map(rotation).transpose()?,
        shade: object.get("shade").and_then(Value::as_bool).unwrap_or(true),
        faces,
    })
}

fn face(value: &Value) -> Result<Face, Error> {
    let object = value
        .as_object()
        .ok_or_else(|| Error::Shape("a face is an object".to_owned()))?;
    let texture = object
        .get("texture")
        .and_then(Value::as_str)
        .ok_or_else(|| Error::Shape("a face names a `texture`".to_owned()))?;

    let uv = match object.get("uv") {
        Some(Value::Array(items)) => {
            let mut corners = [0.0f32; 4];
            for (index, item) in items.iter().take(4).enumerate() {
                corners[index] = number(item);
            }
            Some(corners)
        }
        _ => None,
    };

    Ok(Face {
        texture: texture.to_owned(),
        uv,
        cullface: object
            .get("cullface")
            .and_then(Value::as_str)
            .and_then(Direction::parse),
        rotation: object.get("rotation").and_then(Value::as_i64).unwrap_or(0) as i32,
        tint: object
            .get("tintindex")
            .and_then(Value::as_i64)
            .map(|index| index.max(0) as u32),
    })
}

fn rotation(value: &Value) -> Result<ElementRotation, Error> {
    let object = value
        .as_object()
        .ok_or_else(|| Error::Shape("a rotation is an object".to_owned()))?;
    let axis = object
        .get("axis")
        .and_then(Value::as_str)
        .and_then(Axis::parse)
        .ok_or_else(|| Error::Shape("a rotation names an `axis`".to_owned()))?;
    Ok(ElementRotation {
        origin: vec3(object.get("origin"), [0.0; 3])?,
        axis,
        angle: object.get("angle").map(number).unwrap_or(0.0),
        rescale: object
            .get("rescale")
            .and_then(Value::as_bool)
            .unwrap_or(false),
    })
}

/// Three numbers, or `fallback` when they are absent.
fn vec3(value: Option<&Value>, fallback: [f32; 3]) -> Result<[f32; 3], Error> {
    let Some(value) = value else {
        return Ok(fallback);
    };
    let items = value
        .as_array()
        .ok_or_else(|| Error::Shape("a position is an array".to_owned()))?;
    if items.len() < 3 {
        return Err(Error::Shape("a position has three numbers".to_owned()));
    }
    Ok([number(&items[0]), number(&items[1]), number(&items[2])])
}

fn number(value: &Value) -> f32 {
    value.as_f64().unwrap_or(0.0) as f32
}

#[cfg(test)]
mod tests {
    use super::{Axis, parse};
    use crate::Direction;

    #[test]
    fn a_cube_is_read_with_its_faces() {
        let model = parse(
            r##"{"parent":"minecraft:block/cube_all",
                "textures":{"all":"minecraft:block/stone"},
                "elements":[{"from":[0,0,0],"to":[16,16,16],
                  "faces":{"north":{"texture":"#all","cullface":"north","tintindex":0}}}]}"##,
        )
        .unwrap();

        assert_eq!(model.parent.as_deref(), Some("minecraft:block/cube_all"));
        assert_eq!(model.textures["all"], "minecraft:block/stone");

        let element = &model.elements.as_ref().unwrap()[0];
        assert_eq!(element.from, [0.0, 0.0, 0.0]);
        assert_eq!(element.to, [16.0, 16.0, 16.0]);
        assert!(element.shade, "shade defaults to on");

        let face = &element.faces[&Direction::North];
        assert_eq!(face.texture, "#all");
        assert_eq!(face.cullface, Some(Direction::North));
        assert_eq!(face.tint, Some(0));
        assert!(face.uv.is_none());
    }

    #[test]
    fn an_element_rotation_is_read() {
        let model = parse(
            r#"{"elements":[{"from":[0,0,0],"to":[16,16,16],
                "rotation":{"origin":[8,8,8],"axis":"y","angle":45,"rescale":true},
                "faces":{}}]}"#,
        )
        .unwrap();
        let rotation = model.elements.as_ref().unwrap()[0]
            .rotation
            .as_ref()
            .unwrap();
        assert_eq!(rotation.axis, Axis::Y);
        assert_eq!(rotation.angle, 45.0);
        assert!(rotation.rescale);
    }

    #[test]
    fn a_model_with_only_a_parent_keeps_the_rest_unset() {
        let model = parse(r#"{"parent":"minecraft:block/block"}"#).unwrap();
        assert!(model.elements.is_none());
        assert!(model.textures.is_empty());
        assert!(model.parent.is_some());
    }
}
