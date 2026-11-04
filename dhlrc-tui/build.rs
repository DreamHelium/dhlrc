/*! Stamps the binary with the date the frontends show.
 *
 * CMake fixes `DHLRC_COMPILE_DATE` at configure time and passes it to both the
 * Qt sources (as a macro) and Cargo (as an environment variable), so
 * `dhlrc_qt` and `dhlrc-tui` report the same build date. A build outside CMake
 * (a plain `cargo build`) falls back to today's date in UTC, which keeps the
 * `--version` output a date even then; the local date needs the platform's
 * time-zone tables, which a build script should not depend on.
 */

use time::OffsetDateTime;

fn main() {
    println!("cargo:rerun-if-env-changed=DHLRC_COMPILE_DATE");
    let stamp = std::env::var("DHLRC_COMPILE_DATE")
        .ok()
        .filter(|value| value.len() == 8 && value.chars().all(|ch| ch.is_ascii_digit()))
        .unwrap_or_else(today);
    println!("cargo:rustc-env=DHLRC_BUILD_STAMP={stamp}");
}

/// Today as `%Y%m%d`, in UTC.
fn today() -> String {
    let now = OffsetDateTime::now_utc();
    format!("{:04}{:02}{:02}", now.year(), now.month() as u8, now.day())
}
