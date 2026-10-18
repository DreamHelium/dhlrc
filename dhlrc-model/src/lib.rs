/*! Reading Minecraft's block models.
 *
 * A block is drawn from data, not code: `blockstates/<block>.json` names the
 * model each state uses — or, for the `multipart` blocks, the pieces that are
 * drawn when their conditions hold — `models/<path>.json` holds the geometry
 * (a handful of boxes, with a texture per face) and the texture variables, and
 * a model may `parent` another to inherit from it. This crate turns those files
 * into resolved models and, from them, into the quads a renderer draws, read
 * through `dhlrc-resource`'s layer so a resource pack or a mod overrides
 * vanilla exactly as it does in game.
 *
 * It is its own crate so the geometry does not weigh on a frontend that never
 * draws a block, while both frontends still agree on what a block looks like.
 */

mod blockstate;
pub mod catalog;
pub mod direction;
pub mod mesh;
mod model;
mod resolve;

pub use blockstate::{BlockState, Condition, Multipart, Variant, properties_map, property_key};
pub use catalog::{BlockDef, catalog};
pub use direction::Direction;
pub use mesh::{Quad, quads};
pub use model::{Axis, Element, ElementRotation, Face, Model};
pub use resolve::{BlockModel, ResolvedModel, resolve, resolve_model, resolve_parts, resolve_state};

use std::fmt::{self, Display, Formatter};

use dhlrc_resource::Identifier;

/// Why a model could not be read.
#[derive(Debug)]
pub enum Error {
    /// The resource source has no such file.
    Missing(Identifier),
    /// The file is not the JSON it should be.
    Json(String),
    /// The file is JSON but not shaped like a model or a blockstate.
    Shape(String),
    /// A `parent` chain loops, or is deeper than the limit.
    Parent(String),
    /// The block draws nothing in its default state, because it is described
    /// with `multipart` or only draws when properties are set. Expected for
    /// some blocks, so a caller can treat it as "nothing to draw" rather than a
    /// failure.
    NoDefaultModel,
}

impl Display for Error {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Error::Missing(id) => write!(formatter, "no resource for {id}"),
            Error::Json(message) => write!(formatter, "not JSON: {message}"),
            Error::Shape(message) => write!(formatter, "unexpected shape: {message}"),
            Error::Parent(message) => write!(formatter, "bad parent chain: {message}"),
            Error::NoDefaultModel => {
                write!(formatter, "the default state draws no model")
            }
        }
    }
}

impl std::error::Error for Error {}
