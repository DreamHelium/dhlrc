/*! Prints what the renderer makes of one block: its quads, textures and tint.
 *
 * ```text
 * cargo run -p dhlrc-render --example inspect -- <client.jar|assets dir> <block>...
 * ```
 *
 * This is the tool to reach for when a block comes out wrong: it says whether
 * the model resolved at all, how many quads it has, which texture each face
 * samples and how big it turned out.
 */

use dhlrc_resource::{DirSource, Identifier, Source, ZipSource};
use dhlrc_model::{quads, resolve_parts};

fn main() {
    let mut arguments = std::env::args().skip(1);
    let Some(root) = arguments.next() else {
        eprintln!("usage: inspect <client.jar|assets dir> <block>...");
        std::process::exit(2);
    };
    let source = open(&root);

    for value in arguments {
        let (name, properties) = match value.split_once('[') {
            Some((name, properties)) => (name.to_owned(), properties.trim_end_matches(']').to_owned()),
            None => (value.clone(), String::new()),
        };
        let block = Identifier::parse(&name);
        println!("== {value}");
        match resolve_parts(source.as_ref(), &block, &properties) {
            Ok(parts) => {
                let total: usize = parts.iter().map(|model| quads(model).len()).sum();
                println!("  {} model(s), {total} quads", parts.len());
                for model in &parts {
                    let baked = quads(model);
                    println!(
                        "  -- {}: {} quads, variant {} (x={} y={} uvlock={})",
                        model.variant.model,
                        baked.len(),
                        model.variant.model,
                        model.variant.x,
                        model.variant.y,
                        model.variant.uvlock
                    );
                    let mut seen: Vec<String> = Vec::new();
                    for quad in &baked {
                        let texture = quad
                            .texture
                            .as_ref()
                            .map(ToString::to_string)
                            .unwrap_or_else(|| "<none>".to_owned());
                        if !seen.contains(&texture) {
                            seen.push(texture.clone());
                        }
                        println!(
                            "    {texture} cull={:?} tint={:?} shade={} uv={:?}",
                            quad.cullface, quad.tint, quad.shade, quad.uv[0]
                        );
                    }
                    println!("  textures: {seen:?}");
                }
            }
            Err(error) => println!("  error: {error}"),
        }
    }
}

fn open(root: &str) -> Box<dyn Source> {
    let path = std::path::Path::new(root);
    match ZipSource::open(path) {
        Some(source) => Box::new(source),
        None => Box::new(DirSource::new(path)),
    }
}
