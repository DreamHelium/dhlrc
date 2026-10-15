/*! Folding a blockstate and its model's parents into one drawable model.
 *
 * A block id leads to a state file, the state names a model, and that model may
 * `parent` others all the way down to `block/block`. The rules, as in game:
 * a child's `elements` replace the parent's outright, its `textures` are merged
 * over the parent's name by name, and `ambientocclusion` is the child's when it
 * says so. What comes out is one model with every face's texture variable
 * resolvable to a real texture.
 */

use std::collections::BTreeMap;

use dhlrc_resource::{Identifier, Source};

use crate::{Element, Error, Variant, blockstate, model};

/// How deep a `parent` chain may go before it is treated as a loop.
const MAX_DEPTH: usize = 32;

/// A model with its parents folded in, as a renderer needs it.
#[derive(Debug, Clone, Default)]
pub struct ResolvedModel {
    /// Texture variables, each a `#name` or a texture id.
    pub textures: BTreeMap<String, String>,
    pub elements: Vec<Element>,
    pub ambient_occlusion: bool,
    /// The model ids read, the block's own first.
    pub chain: Vec<Identifier>,
}

impl ResolvedModel {
    /// The texture a face draws: follow `#variable` chains to a texture id.
    /// `None` when the chain leads nowhere, which the game draws as the
    /// missing-texture checker.
    pub fn texture(&self, reference: &str) -> Option<Identifier> {
        let mut name = reference;
        for _ in 0..MAX_DEPTH {
            let Some(variable) = name.strip_prefix('#') else {
                return Some(Identifier::parse(name));
            };
            name = self.textures.get(variable)?.as_str();
        }
        None
    }
}

/// The model a block draws in its default state, with the rotation it asks for.
#[derive(Debug, Clone)]
pub struct BlockModel {
    /// The rotation the blockstate asked for.
    pub variant: Variant,
    pub model: ResolvedModel,
}

/// Reads `block`'s default state and resolves the model it draws.
pub fn resolve(source: &dyn Source, block: &Identifier) -> Result<BlockModel, Error> {
    resolve_state(source, block, "")
}

/// Reads the state whose properties are `properties` (`facing=north,half=top`)
/// and resolves the model it draws. An empty string is the state with no
/// properties set, which is what a block placed with none draws.
///
/// This is the `variants` half of a blockstate. A block written with
/// `multipart` instead draws several models at once, so a caller that has to
/// draw everything should use [`resolve_parts`].
pub fn resolve_state(
    source: &dyn Source,
    block: &Identifier,
    properties: &str,
) -> Result<BlockModel, Error> {
    let state = load_state(source, block)?;
    let variant = state
        .variant(properties)
        .first()
        .ok_or(Error::NoDefaultModel)?
        .clone();
    let model = resolve_model(source, &Identifier::parse(&variant.model))?;
    Ok(BlockModel { variant, model })
}

/// Every model `block` draws in the state whose properties are `properties`,
/// in the order they are drawn.
///
/// A state names one variant — or several, when they are weighted — and a
/// `multipart` blockstate instead lists pieces whose `when` conditions are
/// asked in turn: a fence's post, then one side per neighbour it connects to.
/// `NoDefaultModel` means nothing matched, which is normal for a block whose
/// every state needs properties and for a `multipart` block with no
/// neighbours.
pub fn resolve_parts(
    source: &dyn Source,
    block: &Identifier,
    properties: &str,
) -> Result<Vec<BlockModel>, Error> {
    let state = load_state(source, block)?;
    /* A `variants` state is one model. It may name several with weights, which
     * the game picks between at random to vary a texture (`stone` does it); a
     * still picture takes the first rather than drawing them all on top of one
     * another. */
    let mut variants: Vec<Variant> = state
        .variant(properties)
        .first()
        .cloned()
        .into_iter()
        .collect();
    if !state.multipart.is_empty() {
        let values = blockstate::properties_map(properties);
        for part in &state.multipart {
            if part.when.as_ref().is_none_or(|when| when.matches(&values)) {
                variants.extend(part.apply.iter().cloned());
            }
        }
    }
    if variants.is_empty() {
        return Err(Error::NoDefaultModel);
    }

    let mut parts = Vec::with_capacity(variants.len());
    for variant in variants {
        let model = resolve_model(source, &Identifier::parse(&variant.model))?;
        parts.push(BlockModel { variant, model });
    }
    Ok(parts)
}

/// Reads and resolves one model by id (`minecraft:block/stone`).
pub fn resolve_model(source: &dyn Source, id: &Identifier) -> Result<ResolvedModel, Error> {
    /* Read the chain child first, so a loop or an over-long chain is caught
     * before anything is folded. */
    let mut chain = Vec::new();
    let mut current = id.clone();
    loop {
        if chain.len() >= MAX_DEPTH {
            return Err(Error::Parent(format!(
                "{id} parents more than {MAX_DEPTH} deep"
            )));
        }
        let model = load_model(source, &current)?;
        let parent = model.parent.clone();
        chain.push((current, model));
        match parent {
            Some(parent) => current = Identifier::parse(&parent),
            None => break,
        }
    }

    /* Fold root-first, so a child overrides its parent. */
    let mut resolved = ResolvedModel::default();
    for (_, model) in chain.iter().rev() {
        if let Some(elements) = &model.elements {
            resolved.elements = elements.clone();
        }
        if let Some(occlusion) = model.ambient_occlusion {
            resolved.ambient_occlusion = occlusion;
        }
        for (name, value) in &model.textures {
            resolved.textures.insert(name.clone(), value.clone());
        }
    }
    resolved.chain = chain.into_iter().map(|(id, _)| id).collect();
    Ok(resolved)
}

fn load_state(source: &dyn Source, block: &Identifier) -> Result<blockstate::BlockState, Error> {
    let asset = Identifier::new(
        block.namespace.clone(),
        format!("blockstates/{}.json", block.path),
    );
    blockstate::parse(&read(source, &asset)?)
}

fn load_model(source: &dyn Source, id: &Identifier) -> Result<model::Model, Error> {
    let asset = Identifier::new(id.namespace.clone(), format!("models/{}.json", id.path));
    model::parse(&read(source, &asset)?)
}

fn read(source: &dyn Source, id: &Identifier) -> Result<String, Error> {
    let bytes = source.get(id).ok_or_else(|| Error::Missing(id.clone()))?;
    String::from_utf8(bytes).map_err(|error| Error::Json(error.to_string()))
}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;

    use dhlrc_resource::{Identifier, Source};

    use super::{ResolvedModel, resolve, resolve_model, resolve_parts, resolve_state};
    use crate::Error;

    /// A resource source held in memory, keyed by `namespace:path`.
    #[derive(Default)]
    struct Files(BTreeMap<String, String>);

    impl Files {
        fn with(mut self, id: &str, text: &str) -> Self {
            self.0.insert(id.to_owned(), text.to_owned());
            self
        }
    }

    impl Source for Files {
        fn get(&self, id: &Identifier) -> Option<Vec<u8>> {
            self.0
                .get(&id.to_string())
                .map(|text| text.clone().into_bytes())
        }
    }

    /// Registers a model file under `models/<path>.json`.
    fn model_asset(files: Files, id: &str, text: &str) -> Files {
        let id = Identifier::parse(id);
        files.with(
            &Identifier::new(id.namespace, format!("models/{}.json", id.path)).to_string(),
            text,
        )
    }

    #[test]
    fn a_child_inherits_its_parent() {
        let files = model_asset(
            Files::default(),
            "minecraft:block/base",
            r#"{"textures":{"all":"minecraft:block/stone"},"elements":[{"from":[0,0,0],"to":[16,16,16],"faces":{}}]}"#,
        );
        let files = model_asset(
            files,
            "minecraft:block/stone",
            r##"{"parent":"minecraft:block/base","textures":{"particle":"#all"}}"##,
        );

        let model = resolve_model(&files, &Identifier::parse("minecraft:block/stone")).unwrap();
        assert_eq!(model.elements.len(), 1, "the parent's element is used");
        assert_eq!(model.textures["all"], "minecraft:block/stone");
        assert_eq!(model.textures["particle"], "#all");
    }

    #[test]
    fn a_child_overrides_textures_and_elements() {
        let files = model_asset(
            Files::default(),
            "minecraft:block/base",
            r#"{"textures":{"all":"minecraft:block/oak_planks"},"elements":[{"from":[0,0,0],"to":[16,16,16],"faces":{}}]}"#,
        );
        let files = model_asset(
            files,
            "minecraft:block/child",
            r#"{"parent":"minecraft:block/base","textures":{"all":"minecraft:block/spruce_planks"},
                "elements":[{"from":[0,0,0],"to":[16,8,16],"faces":{}}]}"#,
        );

        let model = resolve_model(&files, &Identifier::parse("minecraft:block/child")).unwrap();
        assert_eq!(model.textures["all"], "minecraft:block/spruce_planks");
        assert_eq!(model.elements.len(), 1);
        assert_eq!(model.elements[0].to, [16.0, 8.0, 16.0]);
    }

    #[test]
    fn a_texture_variable_follows_its_chain_to_an_id() {
        let files = model_asset(
            Files::default(),
            "minecraft:block/stone",
            r##"{"textures":{"all":"minecraft:block/stone","side":"#all"}}"##,
        );
        let model = resolve_model(&files, &Identifier::parse("minecraft:block/stone")).unwrap();
        assert_eq!(
            model.texture("#side").map(|id| id.to_string()).as_deref(),
            Some("minecraft:block/stone")
        );
    }

    #[test]
    fn a_dangling_variable_has_no_texture() {
        let model = ResolvedModel::default();
        assert!(model.texture("#missing").is_none());
        assert_eq!(
            model.texture("minecraft:block/stone").unwrap().path,
            "block/stone"
        );
    }

    #[test]
    fn a_parent_loop_is_reported() {
        let files = model_asset(
            Files::default(),
            "minecraft:block/a",
            r#"{"parent":"minecraft:block/b"}"#,
        );
        let files = model_asset(
            files,
            "minecraft:block/b",
            r#"{"parent":"minecraft:block/a"}"#,
        );
        assert!(matches!(
            resolve_model(&files, &Identifier::parse("minecraft:block/a")),
            Err(Error::Parent(_))
        ));
    }

    #[test]
    fn a_missing_file_is_reported() {
        let files = Files::default();
        assert!(matches!(
            resolve_model(&files, &Identifier::parse("minecraft:block/nope")),
            Err(Error::Missing(_))
        ));
    }

    #[test]
    fn a_block_leads_from_its_state_to_a_rotated_model() {
        let files = Files::default().with(
            "minecraft:blockstates/stone.json",
            r#"{"variants":{"":{"model":"minecraft:block/stone"}}}"#,
        );
        let files = model_asset(
            files,
            "minecraft:block/stone",
            r#"{"textures":{"all":"minecraft:block/stone"},"elements":[]}"#,
        );

        let block = resolve(&files, &Identifier::parse("minecraft:stone")).unwrap();
        assert_eq!(block.variant.model, "minecraft:block/stone");
        assert_eq!(block.model.chain.len(), 1);
        assert_eq!(block.model.chain[0].to_string(), "minecraft:block/stone");
    }

    #[test]
    fn a_state_is_chosen_by_the_block_s_properties() {
        /* A block whose states are all keyed by a property: the default state
         * draws nothing, the one the caller names draws its own model. */
        let files = Files::default().with(
            "minecraft:blockstates/stairs.json",
            r#"{"variants":{
                "facing=east,half=bottom":{"model":"minecraft:block/east"},
                "facing=north,half=bottom":{"model":"minecraft:block/north"}}}"#,
        );
        let files = model_asset(
            files,
            "minecraft:block/north",
            r#"{"textures":{"all":"minecraft:block/stone"},"elements":[]}"#,
        );
        let block = resolve(&files, &Identifier::parse("minecraft:stairs")).unwrap_err();
        assert!(matches!(block, Error::NoDefaultModel));

        let block = resolve_state(
            &files,
            &Identifier::parse("minecraft:stairs"),
            "facing=north,half=bottom",
        )
        .unwrap();
        assert_eq!(block.variant.model, "minecraft:block/north");
    }

    /// A source with a `multipart` fence and the four models it names.
    fn fence_source() -> Files {
        let files = Files::default().with(
            "minecraft:blockstates/fence.json",
            r#"{"multipart":[
                {"apply":{"model":"minecraft:block/post"}},
                {"when":{"north":"true"},"apply":{"model":"minecraft:block/side","uvlock":true}},
                {"when":{"OR":[{"east":"side|up"},{"west":"true"}]},
                 "apply":[{"model":"minecraft:block/a"},{"model":"minecraft:block/b"}]},
                {"when":{"AND":[{"north":"true"},{"south":"true"}]},
                 "apply":{"model":"minecraft:block/both"}}]}"#,
        );
        ["post", "side", "a", "b", "both"]
            .iter()
            .fold(files, |files, name| {
                model_asset(files, &format!("minecraft:block/{name}"), r#"{"textures":{}}"#)
            })
    }

    #[test]
    fn a_weighted_variant_is_drawn_once() {
        /* `stone` names four models with weights so the game can vary the
         * texture; a still picture takes one rather than stacking them. */
        let files = Files::default().with(
            "minecraft:blockstates/stone.json",
            r#"{"variants":{"":[
                {"model":"minecraft:block/stone"},
                {"model":"minecraft:block/stone_mirrored"},
                {"model":"minecraft:block/stone","y":180}]}}"#,
        );
        let files = ["stone", "stone_mirrored"].iter().fold(files, |files, name| {
            model_asset(files, &format!("minecraft:block/{name}"), r#"{"textures":{}}"#)
        });
        let parts = resolve_parts(&files, &Identifier::parse("minecraft:stone"), "").unwrap();
        assert_eq!(parts.len(), 1);
        assert_eq!(parts[0].variant.model, "minecraft:block/stone");
    }

    #[test]
    fn a_multipart_block_draws_the_pieces_whose_condition_holds() {
        let files = fence_source();
        let fence = Identifier::parse("minecraft:fence");

        /* Nothing beside it: only the piece with no `when` is drawn. */
        let parts = resolve_parts(&files, &fence, "north=false,east=none,west=false,south=false")
            .unwrap();
        assert_eq!(parts.len(), 1);
        assert_eq!(parts[0].variant.model, "minecraft:block/post");

        /* A neighbour to the north: the post and that side, `uvlock` and all. */
        let parts = resolve_parts(&files, &fence, "north=true,east=none,west=false,south=false")
            .unwrap();
        assert_eq!(parts.len(), 2);
        assert_eq!(parts[1].variant.model, "minecraft:block/side");
        assert!(parts[1].variant.uvlock);

        /* `OR` with an alternative value, and an `apply` list of two models. */
        let parts = resolve_parts(&files, &fence, "north=false,east=side,west=false,south=false")
            .unwrap();
        assert_eq!(parts.len(), 3);
        assert_eq!(parts[2].variant.model, "minecraft:block/b");

        /* `AND`: both properties have to hold. */
        let both = resolve_parts(&files, &fence, "north=true,east=none,west=false,south=true")
            .unwrap();
        assert_eq!(
            both.iter().map(|part| part.variant.model.as_str()).collect::<Vec<_>>(),
            [
                "minecraft:block/post",
                "minecraft:block/side",
                "minecraft:block/both"
            ]
        );
    }

    #[test]
    fn a_multipart_block_with_nothing_matching_draws_nothing() {
        let files = Files::default().with(
            "minecraft:blockstates/only.json",
            r#"{"multipart":[{"when":{"north":"true"},"apply":{"model":"minecraft:block/a"}}]}"#,
        );
        let files = model_asset(files, "minecraft:block/a", r#"{"textures":{}}"#);
        assert!(matches!(
            resolve_parts(&files, &Identifier::parse("minecraft:only"), "north=false"),
            Err(Error::NoDefaultModel)
        ));
    }
}
