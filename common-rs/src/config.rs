//! Shared plumbing for plugin options ("input config" / "output config").
//!
//! A plugin exposes a small set of boolean-ish options to the host. Rather than
//! hand-writing the C ABI for every plugin, a plugin only has to:
//!
//! 1. Define a plain struct holding the options.
//! 2. Implement [`ConfigObject`], pointing at a static table of [`ConfigItem`]s
//!    and knowing how to read/write one entry.
//! 3. Forward the C entry points to [`config_new`], [`config_free`] and the
//!    per-entry accessors in [`ffi`].
//!
//! No macro generates the exported functions; the plugin writes them out so the
//! symbols stay greppable and the ABI is obvious.

use crate::i18n::i18n;
use crate::util::string_to_ptr_fail_to_null;

/// Declares what an option looks like to the host.
///
/// `key` is the machine-readable name, `kind` is one of [`ConfigKind`]'s string
/// forms and `label`/`description` are shown to the user (and translated).
#[derive(Clone, Copy, Debug)]
pub struct ConfigItem {
    /// Stable identifier, e.g. `"ignore_air"`.
    pub key: &'static str,
    /// The value type. See [`ConfigKind::as_str`].
    pub kind: ConfigKind,
    /// User-facing label.
    pub label: &'static str,
    /// One-line explanation, shown as a tooltip.
    pub description: &'static str,
}

impl ConfigItem {
    /// Renders the `"<key>:<kind>"` form the host parses.
    #[must_use]
    pub fn spec(&self) -> String {
        format!("{}:{}", self.key, self.kind.as_str())
    }
}

/// Value types an option may have.
///
/// Only [`ConfigKind::Bool`] is handled by the host today; the others exist so
/// the wire format does not have to change when they are added.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ConfigKind {
    Bool,
    Int,
    String,
}

impl ConfigKind {
    /// The text used after the `:` in a [`ConfigItem::spec`].
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            ConfigKind::Bool => "bool",
            ConfigKind::Int => "int",
            ConfigKind::String => "string",
        }
    }
}

/// Implemented by a plugin's options struct.
pub trait ConfigObject: Default + Sized {
    /// The options this plugin offers, in display order.
    const ITEMS: &'static [ConfigItem];

    /// Writes a boolean option. Returns `false` when `index` is out of range or
    /// the entry is not a boolean, so a bad index from the host is a no-op
    /// instead of a panic.
    fn set_bool(&mut self, index: usize, value: bool) -> bool;

    /// Reads a boolean option, or `None` when it is out of range / a different
    /// type.
    fn get_bool(&self, index: usize) -> Option<bool>;
}

/// Boxes a fresh, default-valued config object.
///
/// # Safety
/// The returned pointer must be released with [`config_free`].
#[must_use]
pub fn config_new<T: ConfigObject>() -> *mut T {
    Box::into_raw(Box::new(T::default()))
}

/// Releases a pointer produced by [`config_new`]. Accepts null.
///
/// # Safety
/// `config` must come from [`config_new`] and must not be used afterwards.
pub unsafe fn config_free<T: ConfigObject>(config: *mut T) {
    if !config.is_null() {
        drop(unsafe { Box::from_raw(config) });
    }
}

/// The per-entry C entry points, backed by [`ConfigObject::ITEMS`].
///
/// A plugin's exported functions are one-line forwards into these, e.g.
/// `output_config_num` becomes `ffi::num::<OutputConfig>`.
pub mod ffi {
    use super::{ConfigObject, i18n, string_to_ptr_fail_to_null};
    use std::ffi::{c_char, c_int};
    use std::ptr::null;

    /// Number of options.
    #[must_use]
    pub fn num<T: ConfigObject>() -> usize {
        T::ITEMS.len()
    }

    /// The `"<key>:<kind>"` spec of one option, or null when out of range.
    #[must_use]
    pub fn item<T: ConfigObject>(index: usize) -> *const c_char {
        match T::ITEMS.get(index) {
            Some(entry) => string_to_ptr_fail_to_null(&entry.spec()),
            None => null(),
        }
    }

    /// The translated label of one option, or null when out of range.
    #[must_use]
    pub fn item_get_name<T: ConfigObject>(index: usize) -> *const c_char {
        match T::ITEMS.get(index) {
            Some(entry) => string_to_ptr_fail_to_null(i18n(entry.label)),
            None => null(),
        }
    }

    /// The translated description of one option, or null when out of range.
    #[must_use]
    pub fn item_get_description<T: ConfigObject>(index: usize) -> *const c_char {
        match T::ITEMS.get(index) {
            Some(entry) => string_to_ptr_fail_to_null(i18n(entry.description)),
            None => null(),
        }
    }

    /// Applies a boolean option. Out-of-range or wrongly-typed indexes are
    /// ignored, and a null object is a no-op.
    ///
    /// # Safety
    /// `config` must come from [`super::config_new`] and be alive.
    pub unsafe fn item_set_bool<T: ConfigObject>(config: *mut T, index: usize, value: c_int) {
        if config.is_null() {
            return;
        }
        // SAFETY: the caller guarantees `config` is a live, exclusive pointer.
        let real = unsafe { &mut *config };
        real.set_bool(index, value != 0);
    }

    /// Reads a boolean option. Returns `0` for out-of-range, wrongly-typed or
    /// null inputs.
    ///
    /// # Safety
    /// `config` must come from [`super::config_new`] and be alive.
    #[must_use]
    pub unsafe fn item_get_bool<T: ConfigObject>(config: *const T, index: usize) -> c_int {
        if config.is_null() {
            return 0;
        }
        // SAFETY: the caller guarantees `config` is a live pointer.
        let real = unsafe { &*config };
        match real.get_bool(index) {
            Some(value) => c_int::from(value),
            None => 0,
        }
    }
}
