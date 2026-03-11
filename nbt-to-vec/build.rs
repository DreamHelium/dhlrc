fn main(){
    if cfg!(debug_assertions) {
        println!("cargo:rustc-link-search=native=target/debug");
    }
    else {
        println!("cargo:rustc-link-search=native=target/release");
    }
    println!("cargo:rustc-link-lib=dylib=region_rs");
    println!("cargo:rustc-link-arg=-Wl,--undefined-version");
}