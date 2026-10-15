/*! Checks block models against a real client jar (or an unpacked resources
 * directory), and shows one block's quads.
 *
 * ```text
 * cargo run -p dhlrc-model --example bake -- <client.jar|assets dir> [block]
 * ```
 *
 * With no block it resolves every blockstate it can find and reports how many
 * failed, which is how the parsers are checked against the whole of vanilla.
 * With a block id (`minecraft:stone`, or `minecraft:oak_fence[north=true]` to
 * pick a state) it prints the resolved models and the quads built from them.
 */

use std::path::{Path, PathBuf};

use dhlrc_resource::{DirSource, Identifier, Source, ZipSource};
use dhlrc_model::{quads, resolve_parts};

fn main() {
    let mut arguments = std::env::args().skip(1);
    let Some(root) = arguments.next() else {
        eprintln!("usage: bake <client.jar|assets dir> [block]");
        std::process::exit(2);
    };
    let root = PathBuf::from(root);

    match arguments.next() {
        Some(block) => show(open(&root).as_ref(), &Identifier::parse(&block)),
        None => check(&root),
    }
}

/// The source to read from: a jar when `root` is one, a directory otherwise.
fn open(root: &Path) -> Box<dyn Source> {
    match ZipSource::open(root) {
        Some(source) => Box::new(source),
        None => Box::new(DirSource::new(root)),
    }
}

/// Resolves every blockstate found, reporting the ones that failed.
fn check(root: &Path) {
    let source = open(root);
    let blocks = if root.extension().and_then(|kind| kind.to_str()) == Some("jar") {
        ZipSource::open(root)
            .map(|zip| {
                zip.names()
                    .iter()
                    .filter_map(|name| block_of(name))
                    .collect()
            })
            .unwrap_or_default()
    } else {
        blockstates_under(&root.join("assets"))
    };
    report(source.as_ref(), &blocks);
}

fn report(source: &dyn Source, blocks: &[Identifier]) {
    let mut no_model = 0;
    let mut failures = Vec::new();
    for block in blocks {
        /* Every block is asked with no properties, which is what a region
         * whose palette entries carry none would ask for. A block whose every
         * state needs them draws nothing there, which is not a parse failure;
         * the renderer asks again with the properties the block really has. */
        match resolve_parts(source, block, "") {
            Ok(_) => {}
            Err(dhlrc_model::Error::NoDefaultModel) => no_model += 1,
            Err(error) => failures.push((block.to_string(), error.to_string())),
        }
    }
    println!(
        "{} blockstates: {} resolved (with no properties), {} need them, {} failed",
        blocks.len(),
        blocks.len() - no_model - failures.len(),
        no_model,
        failures.len()
    );
    for (block, error) in failures.iter().take(20) {
        println!("  {block}: {error}");
    }
}

/// Prints one block's resolved model and quads.
fn show(source: &dyn Source, block: &Identifier) {
    /* A `block[properties]` argument asks about one state rather than the
     * default one, which is how a `multipart` block is looked at. */
    let text = block.to_string();
    let (name, properties) = match text.split_once('[') {
        Some((name, properties)) => (name.to_owned(), properties.trim_end_matches(']').to_owned()),
        None => (text.clone(), String::new()),
    };
    let block = Identifier::parse(&name);

    let parts = match resolve_parts(source, &block, &properties) {
        Ok(parts) => parts,
        Err(error) => {
            eprintln!("{block}: {error}");
            return;
        }
    };

    println!("{block} -> {} model(s)", parts.len());
    for model in &parts {
        println!(
            "  {} (variant x={} y={} uvlock={})",
            model.variant.model, model.variant.x, model.variant.y, model.variant.uvlock
        );
        let chain: Vec<String> = model.model.chain.iter().map(ToString::to_string).collect();
        println!("  chain: {chain:?}");

        for (name, value) in &model.model.textures {
            let id = model
                .model
                .texture(&format!("#{name}"))
                .map(|id| id.to_string())
                .unwrap_or_else(|| "?".to_owned());
            println!("    #{name} = {value} -> {id}");
        }

        let quads = quads(model);
        println!("  {} quads", quads.len());
        for quad in &quads {
            println!(
                "    positions={:?} uv={:?} texture={:?} cull={:?} tint={:?}",
                quad.positions,
                quad.uv,
                quad.texture.as_ref().map(ToString::to_string),
                quad.cullface,
                quad.tint
            );
        }
    }
}

/// The block id an archive entry names, when it is a blockstate.
fn block_of(name: &str) -> Option<Identifier> {
    let rest = name.strip_prefix("assets/")?;
    let (namespace, rest) = rest.split_once('/')?;
    let path = rest.strip_prefix("blockstates/")?.strip_suffix(".json")?;
    Some(Identifier::new(namespace, path))
}

/// Every blockstate id under `assets/<namespace>/blockstates`.
fn blockstates_under(assets: &Path) -> Vec<Identifier> {
    let mut blocks = Vec::new();
    for namespace in directories(assets) {
        let name = namespace
            .file_name()
            .unwrap_or_default()
            .to_string_lossy()
            .into_owned();
        for file in files(&namespace.join("blockstates")) {
            if file.extension().and_then(|kind| kind.to_str()) != Some("json") {
                continue;
            }
            let stem = file.file_stem().unwrap_or_default().to_string_lossy();
            blocks.push(Identifier::new(name.clone(), stem.into_owned()));
        }
    }
    blocks
}

fn directories(path: &Path) -> Vec<PathBuf> {
    entries(path)
        .into_iter()
        .filter(|entry| entry.is_dir())
        .collect()
}

fn files(path: &Path) -> Vec<PathBuf> {
    entries(path)
        .into_iter()
        .filter(|entry| entry.is_file())
        .collect()
}

fn entries(path: &Path) -> Vec<PathBuf> {
    std::fs::read_dir(path)
        .map(|entries| entries.flatten().map(|entry| entry.path()).collect())
        .unwrap_or_default()
}
