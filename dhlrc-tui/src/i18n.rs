/*! The gettext setup, shared with the GUI frontend.
 *
 * The whole project extracts its messages into one catalogue (`dhlrc`) via
 * `make_po.sh`, and the core looks them up with `gettext ()`. That lookup is a
 * process-wide setting, so pointing it at the catalogue here is enough for the
 * core's own strings to come out translated too.
 */

use gettextrs::{LocaleCategory, bind_textdomain_codeset, bindtextdomain, setlocale, textdomain};
use std::path::PathBuf;
use std::sync::Once;

/// The one domain the project extracts into.
const DOMAIN: &str = "dhlrc";

/// Points gettext at the `locale/` directory next to the executable, the same
/// place the Qt frontend uses. Safe to call more than once.
pub fn init() {
    static ONCE: Once = Once::new();
    ONCE.call_once(|| {
        /* Follow the user's locale, as `main.cpp` does. */
        let _ = setlocale(LocaleCategory::LcAll, "");
        let _ = bindtextdomain(DOMAIN, catalogue_dir());
        let _ = bind_textdomain_codeset(DOMAIN, "UTF-8");
        let _ = textdomain(DOMAIN);
    });
}

/// `locale/` next to the executable, or a relative `locale/` when the path
/// cannot be found.
fn catalogue_dir() -> PathBuf {
    std::env::current_exe()
        .ok()
        .and_then(|exe| exe.parent().map(|dir| dir.join("locale")))
        .unwrap_or_else(|| PathBuf::from("locale"))
}

pub use gettextrs::gettext;
