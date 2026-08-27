fn main() {
    if cfg!(debug_assertions) {
        println!("cargo:rustc-link-search=native=build/cargo_build/debug");
    } else {
        println!("cargo:rustc-link-search=native=build/cargo_build/release");
    }
    println!("cargo:rustc-link-lib=dylib=region_rs");
    println!("cargo:rustc-link-arg=-Wl,--undefined-version");
}
