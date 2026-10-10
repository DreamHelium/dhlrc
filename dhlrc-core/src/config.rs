/*! The configuration: a plain value plus how it is read, written and created.
 *
 * The old frontend used `KConfig` (`config.kcfg` → a generated `DhConfig`).
 * This replaces it with an ordinary Rust value serialised as TOML, so the core
 * needs no toolkit at all. The shape mirrors the old entries one for one; the
 * group names lost their capitals and the two `Enum`s became TOML strings.
 *
 * The old `KConfig` file (`~/.config/dhlrcrc`) is not read at all: the first
 * run always starts from the defaults.
 */

use crate::notification::{Level, Notification};
use common_rs::i18n::i18n;
use formatx::formatx;
use gettextrs::gettext;
use notify::{RecursiveMode, Watcher};
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
use std::env;
use std::fs;
use std::io;
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::{self, RecvTimeoutError};
use std::thread::JoinHandle;
use std::time::Duration;

/// Bumped when the on-disk shape changes in a way older builds cannot read.
pub const CONFIG_VERSION: u32 = 1;

fn config_version() -> u32 {
    CONFIG_VERSION
}

/// The unit a memory limit is expressed in (old `LimitUnit`).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default, Serialize, Deserialize)]
pub enum Unit {
    #[default]
    #[serde(rename = "GiB")]
    Gib,
    #[serde(rename = "MiB")]
    Mib,
    #[serde(rename = "KiB")]
    Kib,
    #[serde(rename = "Bytes")]
    Bytes,
}

/// The block information shown by the reader (old `DefaultShowOption`).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default, Serialize, Deserialize)]
pub enum ShowOption {
    #[default]
    Palette,
    Name,
}

/// A plugin option's value; the untagged form is read back by shape.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(untagged)]
pub enum OptionValue {
    Bool(bool),
    Int(i64),
}

/// `serde`'s `skip_serializing_if` wants a plain function path.
fn map_is_empty<V>(map: &BTreeMap<String, V>) -> bool {
    map.is_empty()
}

/// One plugin's saved options for one direction (old `PluginOptionsConfig`).
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct PluginDirection {
    pub use_configured: bool,
    #[serde(skip_serializing_if = "map_is_empty")]
    pub options: BTreeMap<String, OptionValue>,
}

impl Default for PluginDirection {
    fn default() -> Self {
        Self {
            use_configured: false,
            options: BTreeMap::new(),
        }
    }
}

/// The options of every plugin, keyed by the plugin's `type ()`.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct PluginConfig {
    pub input: PluginDirection,
    pub output: PluginDirection,
}

impl Default for PluginConfig {
    fn default() -> Self {
        Self {
            input: PluginDirection::default(),
            output: PluginDirection::default(),
        }
    }
}

/// The `[general]` group.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct General {
    pub memory_limit: u32,
    pub limit_unit: Unit,
    pub elapsed_milliseconds: u32,
    pub select_all_regions_in_loading: bool,
    pub loading_file_by_extension: bool,
    pub fail_then_retry: bool,
    pub strict_nbt_encoding: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub cache_directory: Option<PathBuf>,
    pub fail_download_use_cache: bool,
}

impl Default for General {
    fn default() -> Self {
        Self {
            memory_limit: 524_288_000,
            limit_unit: Unit::default(),
            elapsed_milliseconds: 500,
            select_all_regions_in_loading: false,
            loading_file_by_extension: true,
            fail_then_retry: false,
            strict_nbt_encoding: false,
            cache_directory: None,
            fail_download_use_cache: false,
        }
    }
}

/// The `[defaults]` group (old `[Default]`).
///
/// The three strings whose old default was an `i18n ()` call are `Option`s: an
/// absent value means "use the localised default", so a translation is never
/// written into the file and switching language keeps working. The accessors
/// below resolve it.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct Defaults {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub base_name: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub region_name: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub multi_region_name_pattern: Option<String>,
    pub name_pattern_sample_file: String,
    pub name_pattern_sample_region: String,
    pub description: String,
    pub author: String,
}

impl Default for Defaults {
    fn default() -> Self {
        Self {
            base_name: None,
            region_name: None,
            multi_region_name_pattern: None,
            name_pattern_sample_file: "house".to_owned(),
            name_pattern_sample_region: "main".to_owned(),
            description: String::new(),
            author: String::new(),
        }
    }
}

impl Defaults {
    pub fn base_name(&self) -> String {
        self.base_name
            .clone()
            .unwrap_or_else(|| gettext(i18n("Converted")))
    }

    pub fn region_name(&self) -> String {
        self.region_name
            .clone()
            .unwrap_or_else(|| gettext(i18n("Unnamed")))
    }

    pub fn multi_region_name_pattern(&self) -> String {
        self.multi_region_name_pattern
            .clone()
            .unwrap_or_else(|| gettext(i18n("${file} - ${region}")))
    }
}

/// The `[game]` group.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct Game {
    pub override_setting: bool,
    #[serde(skip_serializing_if = "String::is_empty")]
    pub override_version: String,
}

impl Default for Game {
    fn default() -> Self {
        Self {
            override_setting: false,
            override_version: String::new(),
        }
    }
}

/// The `[reader]` group.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct Reader {
    pub default_show_option: ShowOption,
}

impl Default for Reader {
    fn default() -> Self {
        Self {
            default_show_option: ShowOption::default(),
        }
    }
}

/// The whole configuration.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct Config {
    #[serde(default = "config_version")]
    pub version: u32,
    pub general: General,
    pub defaults: Defaults,
    pub game: Game,
    pub reader: Reader,
    #[serde(skip_serializing_if = "map_is_empty")]
    pub plugins: BTreeMap<String, PluginConfig>,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            version: CONFIG_VERSION,
            general: General::default(),
            defaults: Defaults::default(),
            game: Game::default(),
            reader: Reader::default(),
            plugins: BTreeMap::new(),
        }
    }
}

impl Config {
    /// The file's text for this configuration, header comment included. Used by
    /// a frontend that wants to write the file itself.
    pub fn to_toml(&self) -> String {
        serialize(self)
    }
}

/// Where the configuration lives.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ConfigPaths {
    /// The file this build uses (`<config_home>/dhlrc/config.toml`).
    pub file: PathBuf,
}

/// What happened when the configuration was opened.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ConfigOrigin {
    /// An existing file was read.
    Loaded,
    /// No file existed; a new one was created.
    CreatedFresh,
    /// A file existed but did not parse; it was moved aside and recreated.
    RecreatedInvalid(String),
}

/// The configuration, where it was read from, and how it came to be.
#[derive(Debug, Clone, PartialEq)]
pub struct ConfigInit {
    pub config: Config,
    pub paths: ConfigPaths,
    pub origin: ConfigOrigin,
}

impl ConfigInit {
    /// The messages a frontend should show about how the file was created.
    /// Empty for a plain load.
    pub fn notifications(&self) -> Vec<Notification> {
        match &self.origin {
            ConfigOrigin::Loaded => Vec::new(),
            ConfigOrigin::CreatedFresh => vec![Notification::new(
                "configCreated",
                Level::Info,
                gettext(i18n("Configuration created")),
                formatx!(
                    gettext(i18n("A new configuration file was created at {}.")),
                    self.paths.file.display().to_string()
                )
                .unwrap_or_default(),
            )],
            ConfigOrigin::RecreatedInvalid(error) => vec![Notification::new(
                "configRecreated",
                Level::Error,
                gettext(i18n("Configuration could not be read")),
                formatx!(
                    gettext(i18n(
                        "The configuration file could not be read ({}). It was moved aside \
                         and a new one with the default values was created at {}."
                    )),
                    error.clone(),
                    self.paths.file.display().to_string()
                )
                .unwrap_or_default(),
            )],
        }
    }
}

/// The user's config directory, following `XDG_CONFIG_HOME` (and `%APPDATA%` on
/// Windows), falling back to `~/.config`.
fn config_home() -> PathBuf {
    if let Some(dir) = env::var_os("XDG_CONFIG_HOME") {
        if !dir.is_empty() {
            return PathBuf::from(dir);
        }
    }

    #[cfg(windows)]
    if let Some(appdata) = env::var_os("APPDATA") {
        return PathBuf::from(appdata);
    }

    if let Some(home) = env::var_os("HOME") {
        return PathBuf::from(home).join(".config");
    }

    PathBuf::from(".")
}

/// The paths for the current user, without touching the filesystem.
pub fn default_paths() -> ConfigPaths {
    ConfigPaths {
        file: config_home().join("dhlrc").join("config.toml"),
    }
}

/// Reads the configuration, creating it from the defaults when it is missing.
///
/// A freshly created file is reported through the returned [`ConfigOrigin`], so
/// the caller can tell the user it appeared.
pub fn load_or_create(paths: &ConfigPaths) -> io::Result<ConfigInit> {
    if paths.file.is_file() {
        let text = fs::read_to_string(&paths.file)?;
        return match toml::from_str::<Config>(&text) {
            Ok(config) => Ok(ConfigInit {
                config,
                paths: paths.clone(),
                origin: ConfigOrigin::Loaded,
            }),
            Err(error) => {
                // Keep the unreadable file rather than dropping the user's data
                // on the floor, and start over from the defaults.
                let backup = backup_path(&paths.file);
                let _ = fs::rename(&paths.file, &backup);
                let config = Config::default();
                write_config(&paths.file, &config)?;
                Ok(ConfigInit {
                    config,
                    paths: paths.clone(),
                    origin: ConfigOrigin::RecreatedInvalid(error.to_string()),
                })
            }
        };
    }

    let config = Config::default();
    write_config(&paths.file, &config)?;
    Ok(ConfigInit {
        config,
        paths: paths.clone(),
        origin: ConfigOrigin::CreatedFresh,
    })
}

/// Watches the configuration file so an edit made while the application runs is
/// noticed and applied, the way the old `KConfig` setup did.
///
/// The file is tiny, so every `poll` simply reads and parses it again: that is
/// robust against editors that rewrite the file in place and against a coarse
/// modification time, and it needs no platform-specific watcher.
#[derive(Debug, Clone)]
pub struct ConfigWatcher {
    paths: ConfigPaths,
    config: Option<Config>,
}

impl ConfigWatcher {
    /// Watches `init`'s file, seeded with the configuration that was just
    /// loaded, so the first report is only ever a real change.
    pub fn new(init: &ConfigInit) -> Self {
        Self::from_config(init.paths.clone(), init.config.clone())
    }

    /// Watches `paths` for `config`, which seeds the last-seen value.
    pub fn from_config(paths: ConfigPaths, config: Config) -> Self {
        Self {
            paths,
            config: Some(config),
        }
    }

    pub fn paths(&self) -> &ConfigPaths {
        &self.paths
    }

    /// The last configuration this watcher saw.
    pub fn current(&self) -> Option<&Config> {
        self.config.as_ref()
    }

    /// Reads the file and returns the new configuration when it changed and
    /// parsed.
    ///
    /// A file that is missing or does not parse yields `None` and leaves the
    /// last good configuration in place, so a half-written file never takes the
    /// running configuration down with it.
    ///
    /// This is the building block `spawn` uses; a frontend that has its own
    /// event loop may call it directly instead.
    pub fn poll(&mut self) -> Option<Config> {
        reload_if_changed(&self.paths.file, &mut self.config)
    }

    /// Watches the file on a background thread, calling `on_change` with the new
    /// configuration each time the file changes and parses.
    ///
    /// The file's *directory* is watched rather than the file itself: an editor
    /// usually writes a temporary file and renames it over the target, which
    /// replaces the inode, so a watch on the file would fire once and then go
    /// silent. Every event in that directory is treated as a reason to re-read,
    /// and the re-read only reports a value that actually differs, so a burst of
    /// events and the application's own saves stay quiet.
    ///
    /// The returned [`ConfigWatch`] stops the thread when dropped.
    pub fn spawn<F>(&self, on_change: F) -> io::Result<ConfigWatch>
    where
        F: Fn(Config) + Send + 'static,
    {
        let file = self.paths.file.clone();
        let dir = file
            .parent()
            .map(Path::to_path_buf)
            .filter(|dir| !dir.as_os_str().is_empty())
            .ok_or_else(|| {
                io::Error::new(
                    io::ErrorKind::InvalidInput,
                    "the config file has no directory",
                )
            })?;

        let mut current = self.config.clone();
        let stop = Arc::new(AtomicBool::new(false));
        let thread_stop = Arc::clone(&stop);

        let (tx, rx) = mpsc::channel::<()>();
        let mut watcher =
            notify::recommended_watcher(move |event: notify::Result<notify::Event>| {
                if event.is_ok() {
                    let _ = tx.send(());
                }
            })
            .map_err(io::Error::other)?;
        watcher
            .watch(&dir, RecursiveMode::NonRecursive)
            .map_err(io::Error::other)?;

        let handle = std::thread::spawn(move || {
            // The watcher has to outlive the loop; the thread owns it.
            let _watcher = watcher;
            while !thread_stop.load(Ordering::Relaxed) {
                match rx.recv_timeout(Duration::from_millis(200)) {
                    Ok(()) => {
                        // Let a burst of events settle, then read once.
                        std::thread::sleep(Duration::from_millis(50));
                        while rx.try_recv().is_ok() {}
                        if let Some(config) = reload_if_changed(&file, &mut current) {
                            on_change(config);
                        }
                    }
                    Err(RecvTimeoutError::Timeout) => {}
                    Err(RecvTimeoutError::Disconnected) => break,
                }
            }
        });

        Ok(ConfigWatch {
            stop,
            handle: Some(handle),
        })
    }
}

/// A running background watch. Dropping it stops the thread.
pub struct ConfigWatch {
    stop: Arc<AtomicBool>,
    handle: Option<JoinHandle<()>>,
}

impl ConfigWatch {
    /// Stops watching and waits for the thread to finish.
    pub fn stop(&mut self) {
        self.stop.store(true, Ordering::Relaxed);
        if let Some(handle) = self.handle.take() {
            let _ = handle.join();
        }
    }
}

impl Drop for ConfigWatch {
    fn drop(&mut self) {
        self.stop();
    }
}

/// Re-reads `path` and returns the new configuration when it parses to something
/// other than `current`, which is updated in place.
fn reload_if_changed(path: &Path, current: &mut Option<Config>) -> Option<Config> {
    let text = fs::read_to_string(path).ok()?;
    let config = toml::from_str::<Config>(&text).ok()?;
    if current.as_ref() == Some(&config) {
        return None;
    }
    *current = Some(config.clone());
    Some(config)
}

/// `config.toml` → `config.toml.bak`.
fn backup_path(file: &Path) -> PathBuf {
    file.with_extension("toml.bak")
}

fn write_config(path: &Path, config: &Config) -> io::Result<()> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    fs::write(path, serialize(config))
}

/// Writes `config` to `paths.file`, creating the directory if needed.
pub fn save_config(paths: &ConfigPaths, config: &Config) -> io::Result<()> {
    write_config(&paths.file, config)
}

const HEADER: &str = "\
# dhlrc configuration.
#
# dhlrc reloads this file when it changes, so an edit made while it is running
# takes effect without a restart. A save from the application rewrites the whole
# file.

";

fn serialize(config: &Config) -> String {
    let mut text = String::from(HEADER);
    if let Ok(body) = toml::to_string_pretty(config) {
        text.push_str(&body);
    }
    text
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::{SystemTime, UNIX_EPOCH};

    /// A throwaway set of paths under the system temp directory.
    fn temp_paths(tag: &str) -> ConfigPaths {
        let nanos = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_nanos();
        let base = env::temp_dir().join(format!("dhlrc-core-{tag}-{nanos}"));
        ConfigPaths {
            file: base.join("dhlrc").join("config.toml"),
        }
    }

    fn cleanup(paths: &ConfigPaths) {
        if let Some(parent) = paths.file.parent().and_then(Path::parent) {
            let _ = fs::remove_dir_all(parent);
        }
    }

    #[test]
    fn defaults_round_trip() {
        let config = Config::default();
        let text = serialize(&config);
        let parsed: Config = toml::from_str(&text).unwrap();
        assert_eq!(config, parsed);
    }

    #[test]
    fn creates_fresh_when_nothing_exists() {
        let paths = temp_paths("fresh");
        let init = load_or_create(&paths).unwrap();
        assert_eq!(init.origin, ConfigOrigin::CreatedFresh);
        assert!(paths.file.is_file());
        assert_eq!(init.notifications().len(), 1);
        cleanup(&paths);
    }

    #[test]
    fn loads_an_existing_file_without_notifying() {
        let paths = temp_paths("existing");
        fs::create_dir_all(paths.file.parent().unwrap()).unwrap();
        let mut config = Config::default();
        config.general.elapsed_milliseconds = 250;
        fs::write(&paths.file, serialize(&config)).unwrap();

        let init = load_or_create(&paths).unwrap();
        assert_eq!(init.origin, ConfigOrigin::Loaded);
        assert_eq!(init.config.general.elapsed_milliseconds, 250);
        assert!(init.notifications().is_empty());
        cleanup(&paths);
    }

    #[test]
    fn recreates_an_unreadable_file_and_keeps_a_backup() {
        let paths = temp_paths("invalid");
        fs::create_dir_all(paths.file.parent().unwrap()).unwrap();
        fs::write(&paths.file, "this is = not = toml [").unwrap();

        let init = load_or_create(&paths).unwrap();
        assert!(matches!(init.origin, ConfigOrigin::RecreatedInvalid(_)));
        assert!(backup_path(&paths.file).is_file());
        assert!(paths.file.is_file());
        cleanup(&paths);
    }

    #[test]
    fn watches_external_changes() {
        let paths = temp_paths("watch");
        let init = load_or_create(&paths).unwrap();
        let mut watcher = ConfigWatcher::new(&init);

        // Nothing changed yet.
        assert!(watcher.poll().is_none());

        // An edit made outside the application is picked up once.
        let mut config = init.config.clone();
        config.general.elapsed_milliseconds = 123;
        fs::write(&paths.file, serialize(&config)).unwrap();

        let updated = watcher.poll().expect("the change should be seen");
        assert_eq!(updated.general.elapsed_milliseconds, 123);
        assert!(watcher.poll().is_none());
        cleanup(&paths);
    }

    #[test]
    fn a_broken_edit_keeps_the_last_good_configuration() {
        let paths = temp_paths("watch-broken");
        let init = load_or_create(&paths).unwrap();
        let mut watcher = ConfigWatcher::new(&init);

        fs::write(&paths.file, "not = valid = toml [").unwrap();
        assert!(watcher.poll().is_none());
        assert_eq!(watcher.current().unwrap().general.elapsed_milliseconds, 500);

        // A good edit afterwards is still applied.
        let mut config = init.config.clone();
        config.general.elapsed_milliseconds = 42;
        fs::write(&paths.file, serialize(&config)).unwrap();
        assert_eq!(watcher.poll().unwrap().general.elapsed_milliseconds, 42);
        cleanup(&paths);
    }

    #[test]
    fn spawn_delivers_changes_in_the_background() {
        let paths = temp_paths("spawn");
        let init = load_or_create(&paths).unwrap();
        let (tx, rx) = mpsc::channel();
        let _watch = ConfigWatcher::new(&init)
            .spawn(move |config| {
                let _ = tx.send(config);
            })
            .unwrap();

        let mut config = init.config.clone();
        config.general.elapsed_milliseconds = 777;
        fs::write(&paths.file, serialize(&config)).unwrap();

        let seen = rx
            .recv_timeout(Duration::from_secs(5))
            .expect("a change should arrive");
        assert_eq!(seen.general.elapsed_milliseconds, 777);
        cleanup(&paths);
    }
}
