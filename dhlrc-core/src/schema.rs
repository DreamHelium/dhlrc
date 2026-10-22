/*! The settings a frontend can edit, described once.
 *
 * Each `SettingDef` names a key, its page, the control it wants, its text and
 * its default, plus the functions that read and write the matching field of
 * `Config`. A frontend walks `SETTINGS`, builds one control per entry and moves
 * values through `Config::setting` / `Config::set_setting`, so adding a setting
 * is one entry here instead of code in every frontend.
 *
 * The texts are message ids, not translations: a Qt frontend localises them
 * with `_()`, a Rust one with `gettext`.
 */

use crate::config::{Config, ShowOption, Unit};
use common_rs::i18n::i18n;
use std::path::PathBuf;

/// Which control a setting wants. `to_int` yields the `DHLRC_SETTING_*` values
/// the C header declares.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SettingType {
    Bool,
    Int,
    String,
    Path,
    Enum,
}

impl SettingType {
    pub fn to_int(self) -> i32 {
        match self {
            SettingType::Bool => 0,
            SettingType::Int => 1,
            SettingType::String => 2,
            SettingType::Path => 3,
            SettingType::Enum => 4,
        }
    }
}

/// One editable setting.
pub struct SettingDef {
    /// Stable id, also the key `Config::setting` looks a value up by.
    pub key: &'static str,
    /// The page it belongs to (a message id).
    pub group: &'static str,
    pub kind: SettingType,
    /// The control's label (a message id; empty when it has none).
    pub label: &'static str,
    /// The control's tooltip (a message id; empty when it has none).
    pub tooltip: &'static str,
    /// The `Enum` choices (message ids); empty otherwise.
    pub choices: &'static [&'static str],
    /// Edited in a multi-line box rather than a single line.
    pub long_text: bool,
    /// The value "Restore Defaults" shows (a message id or a plain value).
    pub default: &'static str,
    /// Reads the current value as text.
    pub get: fn(&Config) -> String,
    /// Writes a value given as text.
    pub set: fn(&mut Config, &str),
}

const GENERAL: &str = i18n("General");
const DEFAULT: &str = i18n("Default");
const GAME: &str = i18n("Game");
const MANAGE: &str = i18n("Manage");
const READER: &str = i18n("Reader");
const MATERIAL: &str = i18n("Material List");
const NOTIFICATIONS: &str = i18n("Notifications");
const RENDER: &str = i18n("3D Preview");

const UNITS: &[&str] = &[i18n("GiB"), i18n("MiB"), i18n("KiB"), i18n("Bytes")];
const SHOW_OPTIONS: &[&str] = &[i18n("Palette"), i18n("Name")];
const NO_CHOICES: &[&str] = &[];

/// Every editable setting, in the order the pages should be shown.
pub static SETTINGS: &[SettingDef] = &[
    SettingDef {
        key: "MemoryLimit",
        group: GENERAL,
        kind: SettingType::String,
        label: i18n("Application Memory Limit"),
        tooltip: i18n("The memory the application may use, counted in the unit below. Loading and saving stop while less than this is free. A decimal is allowed, for example 1.5."),
        choices: NO_CHOICES,
        long_text: false,
        default: "500",
        get: get_memory_limit,
        set: set_memory_limit,
    },
    SettingDef {
        key: "LimitUnit",
        group: GENERAL,
        kind: SettingType::Enum,
        label: i18n("Memory Limit Unit"),
        tooltip: i18n("The unit the memory limit is counted in."),
        choices: UNITS,
        long_text: false,
        default: "1",
        get: get_limit_unit,
        set: set_limit_unit,
    },
    SettingDef {
        key: "ElapsedMilliseconds",
        group: GENERAL,
        kind: SettingType::Int,
        label: i18n("Elapsed Milliseconds"),
        tooltip: i18n(
            "The elapsed milliseconds for the progress bar, if set too small will affect the speed.",
        ),
        choices: NO_CHOICES,
        long_text: false,
        default: "500",
        get: get_elapsed,
        set: set_elapsed,
    },
    SettingDef {
        key: "CuteLanguagePack",
        group: GENERAL,
        kind: SettingType::Bool,
        label: i18n("Cute Language Pack"),
        tooltip: i18n("Use the playful catalogue (喵, 捏, kaomoji, emoji) instead of the plain one. It is read when the program starts, so a restart is needed."),
        choices: NO_CHOICES,
        long_text: false,
        default: "false",
        get: get_cute_language_pack,
        set: set_cute_language_pack,
    },
    SettingDef {
        key: "CacheDirectory",
        group: GENERAL,
        kind: SettingType::Path,
        label: i18n("Cache Directory"),
        tooltip: i18n("The directory to store cache files."),
        choices: NO_CHOICES,
        long_text: false,
        default: "",
        get: get_cache_directory,
        set: set_cache_directory,
    },
    SettingDef {
        key: "FailDownloadUseCache",
        group: GENERAL,
        kind: SettingType::Bool,
        label: i18n("Use Cached File when Downloading Failed"),
        tooltip: i18n("When downloading failed, use the cached file."),
        choices: NO_CHOICES,
        long_text: false,
        default: "false",
        get: get_fail_use_cache,
        set: set_fail_use_cache,
    },
    SettingDef {
        key: "SelectAllRegionsInLoading",
        group: MANAGE,
        kind: SettingType::Bool,
        label: i18n("Select All Regions In Loading"),
        tooltip: i18n("If you check this, all regions will be loaded in the loading progress."),
        choices: NO_CHOICES,
        long_text: false,
        default: "false",
        get: get_select_all,
        set: set_select_all,
    },
    SettingDef {
        key: "FailThenRetry",
        group: MANAGE,
        kind: SettingType::Bool,
        label: i18n("Try Other Modules when Loading Fails"),
        tooltip: i18n("A file is first read by the module its extension points at; when that fails, the other modules are tried as well. Turn this off to make a file whose extension is wrong fail instead."),
        choices: NO_CHOICES,
        long_text: false,
        default: "true",
        get: get_fail_then_retry,
        set: set_fail_then_retry,
    },
    SettingDef {
        key: "MultiRegionNamePattern",
        group: MANAGE,
        kind: SettingType::String,
        label: i18n("Multi-Region Display Name"),
        tooltip: i18n(
            "The display name used for a region that comes from a multi-region file. Use ${file} for the file name and ${region} for the region name inside the file.",
        ),
        choices: NO_CHOICES,
        long_text: false,
        default: i18n("${file} - ${region}"),
        get: get_multi_region_pattern,
        set: set_multi_region_pattern,
    },
    SettingDef {
        key: "BaseName",
        group: DEFAULT,
        kind: SettingType::String,
        label: i18n("Region Base Name"),
        tooltip: i18n("The default base name for the region."),
        choices: NO_CHOICES,
        long_text: false,
        default: i18n("Converted"),
        get: get_base_name,
        set: set_base_name,
    },
    SettingDef {
        key: "RegionName",
        group: DEFAULT,
        kind: SettingType::String,
        label: i18n("Region Name"),
        tooltip: i18n("The default name for the region."),
        choices: NO_CHOICES,
        long_text: false,
        default: i18n("Unnamed"),
        get: get_region_name,
        set: set_region_name,
    },
    SettingDef {
        key: "NamePatternSampleFile",
        group: DEFAULT,
        kind: SettingType::String,
        label: i18n("Sample File Name"),
        tooltip: i18n(
            "The sample file name used by the preview of the multi-region display name. It should not include a file extension.",
        ),
        choices: NO_CHOICES,
        long_text: false,
        default: "house",
        get: get_sample_file,
        set: set_sample_file,
    },
    SettingDef {
        key: "NamePatternSampleRegion",
        group: DEFAULT,
        kind: SettingType::String,
        label: i18n("Sample Region Name"),
        tooltip: i18n(
            "The sample region name used by the preview of the multi-region display name.",
        ),
        choices: NO_CHOICES,
        long_text: false,
        default: "main",
        get: get_sample_region,
        set: set_sample_region,
    },
    SettingDef {
        key: "Description",
        group: DEFAULT,
        kind: SettingType::String,
        label: i18n("Region Description"),
        tooltip: i18n("The default description for the region."),
        choices: NO_CHOICES,
        long_text: true,
        default: "",
        get: get_description,
        set: set_description,
    },
    SettingDef {
        key: "Author",
        group: DEFAULT,
        kind: SettingType::String,
        label: i18n("Region Author"),
        tooltip: i18n("The default author for the region."),
        choices: NO_CHOICES,
        long_text: false,
        default: "",
        get: get_author,
        set: set_author,
    },
    SettingDef {
        key: "OverrideSetting",
        group: GAME,
        kind: SettingType::Bool,
        label: i18n("Override Settings"),
        tooltip: i18n("Whether to override setting or not."),
        choices: NO_CHOICES,
        long_text: false,
        default: "false",
        get: get_override_setting,
        set: set_override_setting,
    },
    SettingDef {
        key: "OverrideVersion",
        group: GAME,
        kind: SettingType::String,
        label: i18n("Override Version"),
        tooltip: i18n("The version to override in translation stage."),
        choices: NO_CHOICES,
        long_text: false,
        default: "",
        get: get_override_version,
        set: set_override_version,
    },
    SettingDef {
        key: "Language",
        group: READER,
        kind: SettingType::String,
        label: i18n("Block Name Language"),
        tooltip: i18n(
            "The Minecraft locale (like zh_cn) block and item names are shown in. The language file is read from the downloaded assets, so it works as soon as that version's assets are there.",
        ),
        choices: NO_CHOICES,
        long_text: false,
        default: "zh_cn",
        get: get_language,
        set: set_language,
    },
    SettingDef {
        key: "DefaultShowOption",
        group: READER,
        kind: SettingType::Enum,
        label: i18n("Default Show Option"),
        tooltip: i18n("The default option of showing block information."),
        choices: SHOW_OPTIONS,
        long_text: false,
        default: "0",
        get: get_show_option,
        set: set_show_option,
    },
    SettingDef {
        key: "DownloadClientJar",
        group: RENDER,
        kind: SettingType::Bool,
        label: i18n("Download the Client Jar Automatically"),
        tooltip: i18n(
            "A block's models and textures are only in its version's client jar. When a 3D preview needs one the cache directory does not have, download it (about 30 MB) instead of drawing nothing.",
        ),
        choices: NO_CHOICES,
        long_text: false,
        default: "true",
        get: get_download_client_jar,
        set: set_download_client_jar,
    },
    SettingDef {
        key: "DownloadClientJarForMaterials",
        group: MATERIAL,
        kind: SettingType::Bool,
        label: i18n("Download the Client Jar for Item Names and Icons"),
        tooltip: i18n(
            "When the material list has no client jar for a version, fetch it automatically so item names and icons are available.",
        ),
        choices: NO_CHOICES,
        long_text: false,
        default: "true",
        get: get_download_client_jar_for_materials,
        set: set_download_client_jar_for_materials,
    },
    SettingDef {
        key: "NotificationAppId",
        group: NOTIFICATIONS,
        kind: SettingType::String,
        label: i18n("Notification Application ID"),
        tooltip: i18n(
            "The application a desktop notification is sent as: the Windows AppUserModelID, or the macOS bundle identifier. Windows needs it registered, or the toast may not show. Empty uses the platform default.",
        ),
        choices: NO_CHOICES,
        long_text: false,
        default: "",
        get: get_notification_app_id,
        set: set_notification_app_id,
    },
    SettingDef {
        key: "NotificationTimeout",
        group: NOTIFICATIONS,
        kind: SettingType::Int,
        label: i18n("Notification Timeout (ms)"),
        tooltip: i18n(
            "How long a notification stays before it disappears, in milliseconds. Zero leaves it to the platform.",
        ),
        choices: NO_CHOICES,
        long_text: false,
        default: "5000",
        get: get_notification_timeout,
        set: set_notification_timeout,
    },
    SettingDef {
        key: "SuppressWhenActive",
        group: NOTIFICATIONS,
        kind: SettingType::Bool,
        label: i18n("Only Notify When Inactive"),
        tooltip: i18n(
            "Do not show a desktop notification while the application is active; the in-app one is enough then.",
        ),
        choices: NO_CHOICES,
        long_text: false,
        default: "true",
        get: get_suppress_when_active,
        set: set_suppress_when_active,
    },
];

impl Config {
    /// The current value of `key` as text, or `None` when `key` is unknown.
    pub fn setting(&self, key: &str) -> Option<String> {
        SETTINGS
            .iter()
            .find(|def| def.key == key)
            .map(|def| (def.get)(self))
    }

    /// Writes `value` (as text) into the field `key` names; returns false when
    /// `key` is unknown.
    pub fn set_setting(&mut self, key: &str, value: &str) -> bool {
        match SETTINGS.iter().find(|def| def.key == key) {
            Some(def) => {
                (def.set)(self, value);
                true
            }
            None => false,
        }
    }
}

/// A check box's value as text.
fn bool_text(value: bool) -> String {
    value.to_string()
}

/// A check box's "on" spellings.
fn parse_bool(text: &str) -> bool {
    matches!(text.trim(), "true" | "1" | "yes" | "on")
}

/// The unit a `LimitUnit` enum index names.
fn unit_from_index(index: i32) -> Unit {
    match index {
        1 => Unit::Mib,
        2 => Unit::Kib,
        3 => Unit::Bytes,
        _ => Unit::Gib,
    }
}

fn unit_index(unit: Unit) -> i32 {
    match unit {
        Unit::Gib => 0,
        Unit::Mib => 1,
        Unit::Kib => 2,
        Unit::Bytes => 3,
    }
}

/// The option a `DefaultShowOption` enum index names.
fn show_from_index(index: i32) -> ShowOption {
    match index {
        1 => ShowOption::Name,
        _ => ShowOption::Palette,
    }
}

fn show_index(option: ShowOption) -> i32 {
    match option {
        ShowOption::Palette => 0,
        ShowOption::Name => 1,
    }
}

/// An integer field: keep the old value when the text is not a number.
fn set_uint(slot: &mut u32, text: &str) {
    if let Ok(value) = text.trim().parse() {
        *slot = value;
    }
}

/// An optional string: an empty text means "unset", so the localised default
/// applies instead of a translation being written into the file.
fn set_optional_string(slot: &mut Option<String>, text: &str) {
    *slot = if text.trim().is_empty() {
        None
    } else {
        Some(text.to_owned())
    };
}

fn get_memory_limit(config: &Config) -> String {
    /* Rust's Display gives the shortest text that reads back to the same
     * number ("500", "1.5"), so a decimal typed in the dialog round-trips. */
    format!("{}", config.general.memory_limit)
}

fn set_memory_limit(config: &mut Config, text: &str) {
    if let Ok(value) = text.trim().parse::<f64>() {
        if value.is_finite() {
            config.general.memory_limit = value.max(0.0);
        }
    }
}

fn get_limit_unit(config: &Config) -> String {
    unit_index(config.general.limit_unit).to_string()
}

fn set_limit_unit(config: &mut Config, text: &str) {
    if let Ok(index) = text.trim().parse() {
        config.general.limit_unit = unit_from_index(index);
    }
}

fn get_elapsed(config: &Config) -> String {
    config.general.elapsed_milliseconds.to_string()
}

fn set_elapsed(config: &mut Config, text: &str) {
    set_uint(&mut config.general.elapsed_milliseconds, text);
}

fn get_select_all(config: &Config) -> String {
    bool_text(config.manage.select_all_regions_in_loading)
}

fn set_select_all(config: &mut Config, text: &str) {
    config.manage.select_all_regions_in_loading = parse_bool(text);
}

fn get_fail_then_retry(config: &Config) -> String {
    bool_text(config.manage.fail_then_retry)
}

fn set_fail_then_retry(config: &mut Config, text: &str) {
    config.manage.fail_then_retry = parse_bool(text);
}

fn get_cute_language_pack(config: &Config) -> String {
    bool_text(config.general.cute_language_pack)
}

fn set_cute_language_pack(config: &mut Config, text: &str) {
    config.general.cute_language_pack = parse_bool(text);
}

fn get_fail_use_cache(config: &Config) -> String {
    bool_text(config.general.fail_download_use_cache)
}

fn set_fail_use_cache(config: &mut Config, text: &str) {
    config.general.fail_download_use_cache = parse_bool(text);
}

fn get_cache_directory(config: &Config) -> String {
    /* The configured path, or the platform default when unset, so the file a
     * frontend writes the setting from is a usable directory. */
    config.general.cache_dir().to_string_lossy().into_owned()
}

fn set_cache_directory(config: &mut Config, text: &str) {
    config.general.cache_directory = if text.trim().is_empty() {
        None
    } else {
        Some(PathBuf::from(text))
    };
}

fn get_language(config: &Config) -> String {
    config.general.language.clone()
}

fn set_language(config: &mut Config, text: &str) {
    config.general.language = text.trim().to_owned();
}

fn get_base_name(config: &Config) -> String {
    config.defaults.base_name()
}

fn set_base_name(config: &mut Config, text: &str) {
    set_optional_string(&mut config.defaults.base_name, text);
}

fn get_region_name(config: &Config) -> String {
    config.defaults.region_name()
}

fn set_region_name(config: &mut Config, text: &str) {
    set_optional_string(&mut config.defaults.region_name, text);
}

fn get_multi_region_pattern(config: &Config) -> String {
    config.manage.multi_region_name_pattern()
}

fn set_multi_region_pattern(config: &mut Config, text: &str) {
    set_optional_string(&mut config.manage.multi_region_name_pattern, text);
}

fn get_sample_file(config: &Config) -> String {
    config.defaults.name_pattern_sample_file.clone()
}

fn set_sample_file(config: &mut Config, text: &str) {
    config.defaults.name_pattern_sample_file = text.to_owned();
}

fn get_sample_region(config: &Config) -> String {
    config.defaults.name_pattern_sample_region.clone()
}

fn set_sample_region(config: &mut Config, text: &str) {
    config.defaults.name_pattern_sample_region = text.to_owned();
}

fn get_description(config: &Config) -> String {
    config.defaults.description.clone()
}

fn set_description(config: &mut Config, text: &str) {
    config.defaults.description = text.to_owned();
}

fn get_author(config: &Config) -> String {
    config.defaults.author.clone()
}

fn set_author(config: &mut Config, text: &str) {
    config.defaults.author = text.to_owned();
}

fn get_override_setting(config: &Config) -> String {
    bool_text(config.game.override_setting)
}

fn set_override_setting(config: &mut Config, text: &str) {
    config.game.override_setting = parse_bool(text);
}

fn get_override_version(config: &Config) -> String {
    config.game.override_version.clone()
}

fn set_override_version(config: &mut Config, text: &str) {
    config.game.override_version = text.to_owned();
}

fn get_show_option(config: &Config) -> String {
    show_index(config.reader.default_show_option).to_string()
}

fn set_show_option(config: &mut Config, text: &str) {
    if let Ok(index) = text.trim().parse() {
        config.reader.default_show_option = show_from_index(index);
    }
}

fn get_notification_app_id(config: &Config) -> String {
    config.notifications.app_id.clone().unwrap_or_default()
}

fn set_notification_app_id(config: &mut Config, text: &str) {
    set_optional_string(&mut config.notifications.app_id, text);
}

fn get_notification_timeout(config: &Config) -> String {
    config.notifications.timeout_ms.to_string()
}

fn set_notification_timeout(config: &mut Config, text: &str) {
    set_uint(&mut config.notifications.timeout_ms, text);
}

fn get_suppress_when_active(config: &Config) -> String {
    bool_text(config.notifications.suppress_when_active)
}

fn set_suppress_when_active(config: &mut Config, text: &str) {
    config.notifications.suppress_when_active = parse_bool(text);
}

fn get_download_client_jar(config: &Config) -> String {
    bool_text(config.render.download_client_jar)
}

fn set_download_client_jar(config: &mut Config, text: &str) {
    config.render.download_client_jar = parse_bool(text);
}

fn get_download_client_jar_for_materials(config: &Config) -> String {
    bool_text(config.material.download_client_jar)
}

fn set_download_client_jar_for_materials(config: &mut Config, text: &str) {
    config.material.download_client_jar = parse_bool(text);
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::BTreeSet;

    #[test]
    fn keys_are_unique() {
        let mut seen = BTreeSet::new();
        for def in SETTINGS {
            assert!(seen.insert(def.key), "duplicate key {}", def.key);
        }
    }

    #[test]
    fn every_setting_round_trips_through_text() {
        for def in SETTINGS {
            let mut config = Config::default();
            let text = config.setting(def.key).expect("known key");
            assert!(config.set_setting(def.key, &text));
            let again = config.setting(def.key).expect("known key");
            assert_eq!(text, again, "round trip for {}", def.key);
        }
    }

    #[test]
    fn unknown_keys_are_rejected() {
        let mut config = Config::default();
        assert!(config.setting("NoSuchSetting").is_none());
        assert!(!config.set_setting("NoSuchSetting", "1"));
    }

    #[test]
    fn the_pages_are_in_the_order_a_frontend_shows_them() {
        let mut pages: Vec<&str> = Vec::new();
        for def in SETTINGS {
            if pages.last() != Some(&def.group) {
                pages.push(def.group);
            }
        }
        assert_eq!(
            pages,
            [
                GENERAL,
                MANAGE,
                DEFAULT,
                GAME,
                READER,
                MATERIAL,
                NOTIFICATIONS,
                RENDER,
            ]
        );

        /* Within a page the entries follow the fields they edit, so a
         * frontend's tab order is stable. */
        let keys = |group: &'static str| -> Vec<&'static str> {
            SETTINGS
                .iter()
                .filter(|def| def.group == group)
                .map(|def| def.key)
                .collect()
        };
        assert_eq!(
            keys(GENERAL),
            [
                "MemoryLimit",
                "LimitUnit",
                "ElapsedMilliseconds",
                "CuteLanguagePack",
                "CacheDirectory",
                "FailDownloadUseCache",
            ]
        );
        assert_eq!(
            keys(MANAGE),
            [
                "SelectAllRegionsInLoading",
                "FailThenRetry",
                "MultiRegionNamePattern",
            ]
        );
    }

    #[test]
    fn the_client_jar_settings_move_the_configuration() {
        let mut config = Config::default();
        assert_eq!(config.setting("DownloadClientJar").as_deref(), Some("true"));
        assert!(config.set_setting("DownloadClientJar", "false"));
        assert!(!config.render.download_client_jar);

        /* The material list has its own toggle: turning it off leaves the
         * preview's download on. */
        assert_eq!(
            config.setting("DownloadClientJarForMaterials").as_deref(),
            Some("true")
        );
        assert!(config.set_setting("DownloadClientJarForMaterials", "false"));
        assert!(!config.material.download_client_jar);
        assert!(!config.render.download_client_jar);
    }
}
