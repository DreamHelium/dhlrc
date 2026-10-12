/*! An identity stand-in for `gettext-rs`, used where libintl cannot be built.
 *
 * GNU gettext's Rust bindings need a C `libintl`. On Android/Termux there is no
 * such library, and `gettext-sys` then falls back to compiling GNU gettext from
 * its bundled sources, which fails at link time (no `iconv`). Every crate that
 * translates strings depends on `gettextrs`; on Android that dependency points
 * here instead, so the build needs no C library and every message comes out in
 * the source language (English).
 *
 * The signatures match `gettext-rs` exactly, so no call site has to change.
 */

use std::io;
use std::path::PathBuf;

/// Mirrors `gettext_rs::LocaleCategory`, so the enum can be passed through.
#[derive(Debug, PartialEq, Clone, Copy)]
pub enum LocaleCategory {
    /// Character classification and case conversion.
    LcCType = 0,
    /// Non-monetary numeric formats.
    LcNumeric = 1,
    /// Date and time formats.
    LcTime = 2,
    /// Collation order.
    LcCollate = 3,
    /// Monetary formats.
    LcMonetary = 4,
    /// Formats of informative and diagnostic messages and interactive responses.
    LcMessages = 5,
    /// For all.
    LcAll = 6,
    /// Paper size.
    LcPaper = 7,
    /// Name formats.
    LcName = 8,
    /// Address formats and location information.
    LcAddress = 9,
    /// Telephone number formats.
    LcTelephone = 10,
    /// Measurement units (Metric or Other).
    LcMeasurement = 11,
    /// Metadata about the locale information.
    LcIdentification = 12,
}

/// Returns `msgid` unchanged; there is no catalogue to look it up in.
pub fn gettext<T: Into<String>>(msgid: T) -> String {
    msgid.into()
}

/// Does nothing, and echoes the directory back as the current one.
pub fn bindtextdomain<T, U>(_domainname: T, dirname: U) -> Result<PathBuf, io::Error>
where
    T: Into<Vec<u8>>,
    U: Into<PathBuf>,
{
    Ok(dirname.into())
}

/// Does nothing, and echoes the domain back as the current one.
pub fn textdomain<T: Into<Vec<u8>>>(domainname: T) -> Result<Vec<u8>, io::Error> {
    Ok(domainname.into())
}

/// Does nothing, and echoes the codeset back.
pub fn bind_textdomain_codeset<T, U>(
    _domainname: T,
    codeset: U,
) -> Result<Option<String>, io::Error>
where
    T: Into<Vec<u8>>,
    U: Into<String>,
{
    Ok(Some(codeset.into()))
}

/// Does nothing. `None` is what a failed `setlocale` returns, and every caller
/// here ignores the value.
pub fn setlocale<T: Into<Vec<u8>>>(_category: LocaleCategory, _locale: T) -> Option<Vec<u8>> {
    None
}
