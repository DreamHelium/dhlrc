/*! Prints the default configuration, for eyeballing the on-disk format. */

fn main() {
    print!("{}", dhlrc_core::Config::default().to_toml());
}
