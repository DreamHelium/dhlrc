/*! The Qt-free core of dhlrc.
 *
 * Everything the frontends (Qt Widgets, QML, terminal) have in common lives
 * here, so none of them has to depend on a GUI toolkit. The first piece is the
 * configuration: a plain `Config` value, read from and written to a TOML file
 * under the user's config directory.
 *
 * The `desktop-notifications` feature adds `desktop`, which shows the core's
 * notifications on the desktop with no GUI toolkit involved. The `download`
 * module fetches the resource files, so no frontend has to. The `render`
 * feature adds [`render`], the C ABI the Qt view draws blocks through; it is
 * off by default so a frontend that never draws does not carry the geometry.
 */

pub mod config;
#[cfg(feature = "desktop-notifications")]
pub mod desktop;
pub mod download;
pub mod ffi;
pub mod material;
pub mod modules;
pub mod notification;
#[cfg(feature = "render")]
pub mod render;
pub mod schema;

/// The resource layer, re-exported so `dhlrc_core::resource` keeps working for
/// every existing reader while the code itself lives in `dhlrc-resource` — the
/// crate the renderer links without the core, so this library can offer
/// drawing without a second copy of itself ending up in the process.
pub use dhlrc_resource as resource;

pub use config::{
    Config, ConfigInit, ConfigOrigin, ConfigPaths, ConfigWatch, ConfigWatcher, default_paths,
    load_or_create,
};
pub use notification::{Level, Notification};
