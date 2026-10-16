// The crate links against `region_rs` by name (`#[link(name = "region_rs")]`),
// so the linker needs the directory holding it. Cargo writes the final artifact
// of every crate to `<target-dir>/<profile>/`, which is three levels above
// `OUT_DIR` (`<target-dir>/<profile>/build/<pkg>-<hash>/out`). Deriving it from
// `OUT_DIR` keeps the build working whatever `--target-dir` is used.
//
// The `-l` itself comes from the `#[link]` attribute, which the unit tests turn
// off: they exercise the format layer and never call into `region_rs`, so they
// should not need it to be built first.
fn main() {
    let out_dir = std::env::var("OUT_DIR").expect("OUT_DIR is set by Cargo");
    let profile_dir = std::path::Path::new(&out_dir)
        .ancestors()
        .nth(3)
        .expect("OUT_DIR is <target-dir>/<profile>/build/<pkg>-<hash>/out");
    println!("cargo:rustc-link-search=native={}", profile_dir.display());
    println!("cargo:rustc-link-arg=-Wl,--undefined-version");
}
