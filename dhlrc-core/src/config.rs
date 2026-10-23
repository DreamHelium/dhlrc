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
///
/// Version 5 moves the options that shape loading into `[manage]`, gives the
/// material list its own download toggle in `[material]`, and drops the
/// conversion code: an old file is not migrated but replaced with the defaults
/// after a backup (see [`backup_and_reset`]), so every build reads one shape
/// only. A file whose `version` is older than this one is loaded as written and
/// flagged through [`ConfigInit::needs_reset`].
pub const CONFIG_VERSION: u32 = 5;

/// A file written before the `version` field existed reads as version 1.
///
/// The absence used to mean "whatever this build writes"; now it means an old
/// file, so [`load_or_create`] reports that it needs the defaults rather than
/// treating the old shape as current.
fn legacy_config_version() -> u32 {
    1
}

/// Reads the memory limit from either a TOML integer (old files stored a byte
/// count) or a float (version 3 and later store the number in `limit_unit`).
///
/// Old files are not converted any more, only kept readable so the version check
/// can run and the user can be offered a reset instead of a parse error.
fn deserialize_limit_number<'de, D>(deserializer: D) -> Result<f64, D::Error>
where
    D: serde::Deserializer<'de>,
{
    struct NumberVisitor;

    impl serde::de::Visitor<'_> for NumberVisitor {
        type Value = f64;

        fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
            formatter.write_str("an integer or a float")
        }

        fn visit_i64<E: serde::de::Error>(self, value: i64) -> Result<f64, E> {
            Ok(value as f64)
        }

        fn visit_u64<E: serde::de::Error>(self, value: u64) -> Result<f64, E> {
            Ok(value as f64)
        }

        fn visit_f64<E: serde::de::Error>(self, value: f64) -> Result<f64, E> {
            Ok(value)
        }
    }

    deserializer.deserialize_any(NumberVisitor)
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

impl Unit {
    /// How many bytes one of this unit is.
    pub const fn factor(self) -> u64 {
        match self {
            Self::Gib => 1 << 30,
            Self::Mib => 1 << 20,
            Self::Kib => 1 << 10,
            Self::Bytes => 1,
        }
    }
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
            /* Reading uses the saved options without asking: a load does not
             * know which plugin will end up reading the file until it has been
             * decoded, so asking about every plugin would be noise. Writing
             * *does* know the format the user picked, so its options are worth
             * asking about once per export (and can be silenced the same way). */
            input: PluginDirection {
                use_configured: true,
                ..PluginDirection::default()
            },
            output: PluginDirection::default(),
        }
    }
}

/// The `[general]` group.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct General {
    /// The memory limit the application may use, counted in `limit_unit`. It
     /// may have a fractional part (`1.5` GiB), so the file stores it as a
     /// number rather than a byte count.
    #[serde(deserialize_with = "deserialize_limit_number")]
    pub memory_limit: f64,
    /// The unit `memory_limit` is counted in.
    pub limit_unit: Unit,
    pub elapsed_milliseconds: u32,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub cache_directory: Option<PathBuf>,
    pub fail_download_use_cache: bool,
    /// The language block and item names are shown in, as a Minecraft locale
    /// (`zh_cn`). Used when a caller does not name one.
    pub language: String,
    /// Use the playful catalogue (`dhlrc-cute`) instead of the plain one. It is
    /// read when the program starts, so turning it on takes a restart.
    pub cute_language_pack: bool,
}

impl Default for General {
    fn default() -> Self {
        Self {
            memory_limit: 500.0,
            limit_unit: Unit::Mib,
            elapsed_milliseconds: 500,
            cache_directory: None,
            fail_download_use_cache: false,
            language: "zh_cn".to_owned(),
            cute_language_pack: false,
        }
    }
}

impl General {
    /// The memory limit in bytes: the number the user set times its unit,
    /// rounded to the nearest byte (a fractional limit such as 0.5 GiB is
    /// exact, 0.1 GiB rounds).
    pub fn memory_limit_bytes(&self) -> u64 {
        let bytes = self.memory_limit.max(0.0) * self.limit_unit.factor() as f64;
        bytes.round().min(u64::MAX as f64) as u64
    }

    /// The directory the downloader fills and the resource layer reads: the
    /// configured one, or the platform's default when unset. Going through here
    /// (rather than the raw field) is what lets both frontends and the resource
    /// layer agree on where the cache is without any of them computing it.
    pub fn cache_dir(&self) -> PathBuf {
        self.cache_directory
            .clone()
            .unwrap_or_else(default_cache_dir)
    }
}

/// The `[manage]` options that shape how a file is loaded.
///
/// They live apart from `[general]` because they describe the *reading* step
/// rather than the application: which regions are ticked when a file is opened,
/// what happens when a module fails, and how a multi-region file is named. The
/// settings page shows them under "Manage".
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct Manage {
    /// Tick every region when a file is opened, instead of the ones the file
    /// names.
    pub select_all_regions_in_loading: bool,
    /// Try the other modules when the one the file extension points at fails.
    pub fail_then_retry: bool,
    /// The name pattern a multi-region file is saved with. Absent means the
    /// localised default rather than a translation written into the file.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub multi_region_name_pattern: Option<String>,
}

impl Default for Manage {
    fn default() -> Self {
        Self {
            select_all_regions_in_loading: false,
            fail_then_retry: true,
            multi_region_name_pattern: None,
        }
    }
}

impl Manage {
    pub fn multi_region_name_pattern(&self) -> String {
        self.multi_region_name_pattern
            .clone()
            .unwrap_or_else(|| gettext(i18n("${file} - ${region}")))
    }
}

/// The `[defaults]` group (old `[Default]`).
///
/// The two strings whose old default was an `i18n ()` call are `Option`s: an
/// absent value means "use the localised default", so a translation is never
/// written into the file and switching language keeps working. The accessors
/// below resolve it. The multi-region name pattern lives in `[manage]` for the
/// same reason.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct Defaults {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub base_name: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub region_name: Option<String>,
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

/// The `[render]` group.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct Render {
    /// Whether the client jar a version's block models and textures live in is
    /// fetched by itself when a preview needs one the cache does not have. Off,
    /// a preview of a version whose jar is missing draws nothing and says so.
    pub download_client_jar: bool,
    /// The `#rrggbb` colour the outline of the selected blocks is drawn in.
    pub highlight_colour: String,
    /// The `#rrggbb` colour behind the 3D preview.
    pub background_colour: String,
    /// The `#rrggbb` colour the region's own box is drawn in.
    pub bounds_colour: String,
    /// The `#rrggbb` colour the marker's box is drawn in: the cell picked in
    /// the selected-blocks list, which stays visible over the other boxes.
    #[serde(default = "default_marker_colour")]
    pub marker_colour: String,
}

impl Default for Render {
    fn default() -> Self {
        Self {
            download_client_jar: true,
            highlight_colour: "#000000".to_owned(),
            background_colour: "#202830".to_owned(),
            bounds_colour: "#00c8ff".to_owned(),
            marker_colour: default_marker_colour(),
        }
    }
}

/// Files written before the marker existed have no `marker_colour`; they read
/// the default instead of failing to load, so a version 5 file stays loadable.
fn default_marker_colour() -> String {
    "#ff2060".to_owned()
}

/// The `[material]` group: what the material list may fetch by itself.
///
/// The list reads item names and icons from the client jar, and a version the
/// cache has no jar for shows raw keys until one is fetched. This toggle is
/// separate from `[render]`'s because a user may want previews to download while
/// a material list stays offline (or the other way round): both toggle the same
/// file but at different moments, and the two features are used separately.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct Material {
    /// Whether the client jar for a version is fetched automatically when the
    /// material list has none.
    pub download_client_jar: bool,
}

impl Default for Material {
    fn default() -> Self {
        Self {
            download_client_jar: true,
        }
    }
}

/// The `[notifications]` group.
///
/// `app_id` names the sender of a desktop notification: the Windows
/// `System.AppUserModel.ID`, or the macOS bundle identifier.
///
/// On Windows it defaults to `cn.dh.dhlrc`, which is the id the installer
/// registers and the frontends give their process — without it the notification
/// crate falls back to PowerShell's id and Windows shows the toast as coming
/// from PowerShell. Elsewhere the default is unset, which leaves the platform's
/// own choice.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct Notifications {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub app_id: Option<String>,
    /// How long a notification stays before it disappears, in milliseconds;
    /// zero leaves it to the platform.
    pub timeout_ms: u32,
    /// Whether to hold back a desktop notification while the application is
    /// active (the in-app one is enough then).
    pub suppress_when_active: bool,
}

impl Default for Notifications {
    fn default() -> Self {
        Self {
            app_id: default_app_id(),
            timeout_ms: 5000,
            suppress_when_active: true,
        }
    }
}

/// The sender a new configuration starts with.
///
/// It has to match the AppUserModelID the installer registers
/// (`DH_APP_ID` in `CMakeLists.txt`), the one the Qt frontend gives its process
/// (`src/windowsappid.cpp`), and the one the core hands to the notification
/// crate. Without a match Windows shows the toast as coming from PowerShell,
/// which is the crate's fallback.
#[cfg(target_os = "windows")]
fn default_app_id() -> Option<String> {
    Some("cn.dh.dhlrc".to_owned())
}

#[cfg(not(target_os = "windows"))]
fn default_app_id() -> Option<String> {
    None
}

/// The whole configuration.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct Config {
    #[serde(default = "legacy_config_version")]
    pub version: u32,
    pub general: General,
    pub manage: Manage,
    pub defaults: Defaults,
    pub game: Game,
    pub reader: Reader,
    pub render: Render,
    pub material: Material,
    pub notifications: Notifications,
    #[serde(skip_serializing_if = "map_is_empty")]
    pub plugins: BTreeMap<String, PluginConfig>,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            version: CONFIG_VERSION,
            general: General::default(),
            manage: Manage::default(),
            defaults: Defaults::default(),
            game: Game::default(),
            reader: Reader::default(),
            render: Render::default(),
            material: Material::default(),
            notifications: Notifications::default(),
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

impl ConfigPaths {
    /// The message a frontend shows when the file changed on disk and the new
    /// values were applied. Used by the watcher and available to a frontend
    /// that polls the file itself.
    pub fn reloaded_notification(&self) -> Notification {
        Notification::new(
            "configReloaded",
            Level::Info,
            gettext(i18n("Configuration reloaded")),
            formatx!(
                gettext(i18n("The configuration was reloaded from {}.")),
                self.file.display().to_string()
            )
            .unwrap_or_default(),
        )
    }
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
    /// The file was written by an older build, so `config` is that old shape and
    /// must not be used as if it were current. A frontend calls
    /// [`backup_and_reset`] (after telling the user) and then starts from the
    /// defaults; until it does, the core keeps the file as it was.
    pub needs_reset: bool,
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

/// The platform's default cache directory for dhlrc, following
/// `XDG_CACHE_HOME` (and the platform's own location on Windows/macOS),
/// falling back to `~/.cache`, then the temporary directory.
///
/// Qt's `QStandardPaths::CacheLocation` resolves to the same place for an
/// application named `dhlrc`, so the GUI and this agree.
fn default_cache_dir() -> PathBuf {
    #[cfg(target_os = "windows")]
    if let Some(dir) = env::var_os("LOCALAPPDATA") {
        if !dir.is_empty() {
            return PathBuf::from(dir).join("dhlrc");
        }
    }

    #[cfg(target_os = "macos")]
    if let Some(home) = env::var_os("HOME") {
        return PathBuf::from(home).join("Library/Caches/dhlrc");
    }

    #[cfg(not(any(target_os = "windows", target_os = "macos")))]
    {
        if let Some(dir) = env::var_os("XDG_CACHE_HOME") {
            let dir = PathBuf::from(dir);
            if dir.is_absolute() {
                return dir.join("dhlrc");
            }
        }
        if let Some(home) = env::var_os("HOME") {
            return PathBuf::from(home).join(".cache/dhlrc");
        }
    }

    env::temp_dir().join("dhlrc")
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
            Ok(config) => {
                /* No conversion: an old file is read as written and flagged, so
                 * the frontend can tell the user before the file is replaced.
                 * Converting here would have to guess what an old value meant
                 * (a byte count? a count in another unit?) and would silently
                 * turn it into a different setting. */
                let needs_reset = config.version != CONFIG_VERSION;
                Ok(ConfigInit {
                    config,
                    paths: paths.clone(),
                    origin: ConfigOrigin::Loaded,
                    needs_reset,
                })
            }
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
                    needs_reset: false,
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
        needs_reset: false,
    })
}

/// The backup a reset keeps the replaced file under, next to the config file.
///
/// The name is fixed rather than derived from the file's name so a second reset
/// overwrites the previous backup instead of leaving a pile of them; the newest
/// old file is the one worth keeping.
fn reset_backup_path(file: &Path) -> PathBuf {
    file.parent()
        .unwrap_or_else(|| Path::new(""))
        .join("config.toml.bak")
}

/// Replaces `paths.file` with the default configuration, keeping the old file as
/// `config.toml.bak` next to it.
///
/// This is what a frontend calls when [`ConfigInit::needs_reset`] is set. The old
/// file is copied first and only then replaced, so a failed write leaves the
/// original where it was. Returns the backup's path, or `None` when there was no
/// file to keep.
pub fn backup_and_reset(paths: &ConfigPaths) -> io::Result<Option<PathBuf>> {
    let backup = if paths.file.is_file() {
        let backup = reset_backup_path(&paths.file);
        fs::copy(&paths.file, &backup)?;
        Some(backup)
    } else {
        None
    };
    write_config(&paths.file, &Config::default())?;
    Ok(backup)
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
    fn an_older_file_is_reported_instead_of_converted() {
        let paths = temp_paths("needs-reset");
        fs::create_dir_all(paths.file.parent().unwrap()).unwrap();
        /* Version 2 stored bytes and had no `[manage]` or `[material]`: the old
         * file is kept exactly as it is, and the caller is told it is stale. */
        let old = "version = 2\n\n[general]\nmemory_limit = 524288000\n\
                   limit_unit = \"GiB\"\n";
        fs::write(&paths.file, old).unwrap();
        let init = load_or_create(&paths).unwrap();
        assert_eq!(init.origin, ConfigOrigin::Loaded);
        assert!(init.needs_reset);
        /* No migration: the number is still the old byte count, which is why the
         * frontend must not use it. */
        assert_eq!(init.config.version, 2);
        assert_eq!(init.config.general.memory_limit, 524_288_000.0);
        assert_eq!(fs::read_to_string(&paths.file).unwrap(), old);
        cleanup(&paths);
    }

    #[test]
    fn a_file_without_a_version_field_needs_a_reset() {
        let paths = temp_paths("needs-reset-unversioned");
        fs::create_dir_all(paths.file.parent().unwrap()).unwrap();
        /* A build old enough not to write `version` at all reads as version 1,
         * so it is as stale as any other old file. */
        let old = "[general]\nmemory_limit = 524288000\nlimit_unit = \"GiB\"\n";
        fs::write(&paths.file, old).unwrap();
        let init = load_or_create(&paths).unwrap();
        assert!(init.needs_reset);
        assert_eq!(init.config.version, legacy_config_version());
        assert_eq!(fs::read_to_string(&paths.file).unwrap(), old);
        cleanup(&paths);
    }

    #[test]
    fn backup_and_reset_keeps_the_old_file_and_writes_the_defaults() {
        let paths = temp_paths("backup-reset");
        fs::create_dir_all(paths.file.parent().unwrap()).unwrap();
        let old = "version = 4\n\n[general]\nelapsed_milliseconds = 250\n";
        fs::write(&paths.file, old).unwrap();
        assert!(load_or_create(&paths).unwrap().needs_reset);

        let backup = backup_and_reset(&paths)
            .unwrap()
            .expect("an existing file is backed up");
        assert_eq!(backup, paths.file.parent().unwrap().join("config.toml.bak"));
        assert_eq!(fs::read_to_string(&backup).unwrap(), old);
        let text = fs::read_to_string(&paths.file).unwrap();
        assert!(text.contains("version = 5"), "config was {text}");

        let init = load_or_create(&paths).unwrap();
        assert!(!init.needs_reset);
        assert_eq!(init.config, Config::default());
        cleanup(&paths);
    }

    #[test]
    fn backup_and_reset_without_a_file_just_writes_the_defaults() {
        let paths = temp_paths("backup-reset-fresh");
        assert_eq!(backup_and_reset(&paths).unwrap(), None);
        assert!(paths.file.is_file());
        assert!(!load_or_create(&paths).unwrap().needs_reset);
        cleanup(&paths);
    }

    #[test]
    fn a_version_five_file_without_marker_colour_still_loads() {
        let paths = temp_paths("v5-colour");
        fs::create_dir_all(paths.file.parent().unwrap()).unwrap();
        /* Version 5 files written before the marker existed have no
         * `marker_colour`; the field's serde default lets them load as they
         * are, with no reset. */
        let old = "version = 5\n\n[render]\nhighlight_colour = \"#000000\"\n\
                   background_colour = \"#202830\"\n\
                   bounds_colour = \"#00c8ff\"\n";
        fs::write(&paths.file, old).unwrap();
        let init = load_or_create(&paths).unwrap();
        assert_eq!(init.origin, ConfigOrigin::Loaded);
        assert!(!init.needs_reset);
        assert_eq!(init.config.version, 5);
        assert_eq!(init.config.render.marker_colour, "#ff2060");
        cleanup(&paths);
    }

    #[test]
    fn an_unset_cache_directory_resolves_to_a_platform_default() {
        /* Unset stays unset in the file, but reading it yields a real place to
         * put the cache. */
        let general = General::default();
        assert!(general.cache_directory.is_none());
        assert!(!general.cache_dir().as_os_str().is_empty());

        /* A configured directory wins over the default. */
        let configured = General {
            cache_directory: Some(PathBuf::from("/tmp/dhlrc-cache")),
            ..General::default()
        };
        assert_eq!(configured.cache_dir(), PathBuf::from("/tmp/dhlrc-cache"));
    }

    #[cfg(target_os = "windows")]
    #[test]
    fn the_windows_default_app_id_is_the_one_the_installer_registers() {
        /* Three places name the same id: the installer (`DH_APP_ID`), the Qt
         * frontend's process (src/windowsappid.cpp) and this default. */
        assert_eq!(
            super::Notifications::default().app_id.as_deref(),
            Some("cn.dh.dhlrc")
        );
    }

    #[test]
    fn notifications_app_id_round_trips_and_is_left_out_unset() {
        // Unset: no `app_id`, but the group's other values are written.
        let unset = serialize(&Config::default());
        assert!(unset.contains("[notifications]"), "text was {unset}");
        assert!(!unset.contains("app_id"), "text was {unset}");

        // Set: it survives a write and a read.
        let mut config = Config::default();
        config.notifications.app_id = Some("cn.dh.dhlrc".to_owned());
        let text = serialize(&config);
        let parsed: Config = toml::from_str(&text).unwrap();
        assert_eq!(parsed.notifications.app_id.as_deref(), Some("cn.dh.dhlrc"));
        assert_eq!(parsed.notifications.timeout_ms, 5000);
        assert!(parsed.notifications.suppress_when_active);
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
        assert!(!init.needs_reset);
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
