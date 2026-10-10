/*! The Qt-free core of dhlrc.
 *
 * Everything the frontends (Qt Widgets, QML, terminal) have in common lives
 * here, so none of them has to depend on a GUI toolkit. The first piece is the
 * configuration: a plain `Config` value, read from and written to a TOML file
 * under the user's config directory.
 */

pub mod config;
pub mod ffi;
pub mod notification;

pub use config::{
    Config, ConfigInit, ConfigOrigin, ConfigPaths, ConfigWatch, ConfigWatcher, default_paths,
    load_or_create,
};
pub use notification::{Level, Notification};
