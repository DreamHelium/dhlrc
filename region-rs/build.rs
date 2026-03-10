fn main() {
    if cfg!(windows) {
        println!("cargo:rustc-link-arg=-Wl,--out-implib=NUL");
    }
}