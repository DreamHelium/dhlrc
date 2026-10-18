/*! Lists the blocks a client jar has, and the properties each takes.
 *
 * This is what a block chooser is built from: `catalog <client.jar>` prints
 * every block and, for the ones named after it, their property values. */

fn main() {
    let path = match std::env::args().nth(1) {
        Some(path) => path,
        None => {
            eprintln!("usage: catalog <client.jar> [block ...]");
            std::process::exit(2);
        }
    };
    let source = match dhlrc_resource::ZipSource::open(&path) {
        Some(source) => source,
        None => {
            eprintln!("{path} is not a zip");
            std::process::exit(1);
        }
    };
    let blocks = dhlrc_model::catalog(&source);
    println!("{} blocks", blocks.len());

    let wanted: Vec<String> = std::env::args().skip(2).collect();
    for block in &blocks {
        if wanted.is_empty() {
            continue;
        }
        if !wanted.iter().any(|name| block.name == *name) {
            continue;
        }
        println!("{}", block.name);
        for (name, values) in &block.properties {
            println!("    {name}: {}", values.join(", "));
        }
    }

    if wanted.is_empty() {
        /* What a chooser has to cope with: blocks with no properties at all,
         * and blocks with several. */
        let bare = blocks.iter().filter(|block| block.properties.is_empty()).count();
        let many = blocks
            .iter()
            .map(|block| block.properties.len())
            .max()
            .unwrap_or(0);
        println!("{bare} blocks take no properties; at most {many} properties on one");
    }
}
