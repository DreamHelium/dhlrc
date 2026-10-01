/* I just need a prefix, so I do this.
 *
 * `const` so callers can use it inside a `const` table, e.g. the option
 * descriptors in `common_rs::config::ConfigItem::ITEMS`. */
#[must_use]
pub const fn i18n(string: &str) -> &str {
    string
}
