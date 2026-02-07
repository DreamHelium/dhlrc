fn main(){
    println!("cargo:rustc-link-search=native=../");
    println!("cargo:rustc-link-lib=dylib=region_rs");
    println!("cargo:rustc-link-arg=-Wl,--undefined-version");
}