/*! The Qt-free core of dhlrc.
 *
 * Everything the frontends (Qt Widgets, QML, terminal) have in common lives
 * here, so none of them has to depend on a GUI toolkit. The first piece is the
 * configuration: a plain `Config` value, read from and written to a TOML file
 * under the user's config directory.
 *
 * The `desktop-notifications` feature adds `desktop`, which shows the core's
 * notifications on the desktop with no GUI toolkit involved. The `download`
 * module fetches the resource files, so no frontend has to.
 */

pub mod config;
#[cfg(feature = "desktop-notifications")]
pub mod desktop;
pub mod download;
pub mod ffi;
pub mod modules;
pub mod notification;
pub mod resource;
pub mod schema;

pub use config::{
    Config, ConfigInit, ConfigOrigin, ConfigPaths, ConfigWatch, ConfigWatcher, default_paths,
    load_or_create,
};
pub use notification::{Level, Notification};
