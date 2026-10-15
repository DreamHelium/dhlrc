/*! Fetching the files the resource layer reads.
 *
 * The frontends do not each talk to Mojang's servers: the version manifest, the
 * version json, the asset index and the objects are fetched here, into the same
 * cache layout `resource::cache` names. Keeping it in the core is what lets the
 * GUI and the terminal frontend share one downloader, and a headless build use
 * it with no toolkit at all.
 *
 * Everything here blocks, on purpose: the caller runs it on a thread of its own
 * and gets status through a [`Sink`]. Nothing needs an event loop, so a
 * terminal frontend can run a download on a plain thread unchanged.
 */

use std::fs;
use std::io::{Read, Write};
use std::path::{Path, PathBuf};
use std::time::{Duration, SystemTime};

use common_rs::i18n::i18n;
use gettextrs::gettext;
use serde_json::Value;

use crate::resource::{bundled, cache};

/// The manifest every download starts from.
const VERSION_MANIFEST: &str = "https://launchermeta.mojang.com/mc/game/version_manifest.json";
/// Where the asset objects live (`<base>/<first two of hash>/<hash>`).
const OBJECT_BASE: &str = "https://resources.download.minecraft.net";
/// The chunk a body is read in, and so how often cancellation is checked.
const CHUNK: usize = 64 * 1024;
/// How long a single request may take before it is given up on.
const TIMEOUT: Duration = Duration::from_secs(60);

/// Why a download did not finish.
#[derive(Debug)]
pub enum Error {
    /// The request could not be made at all (DNS, TLS, connection, timeout).
    Transport(String),
    /// The server answered with an error status.
    Status(u16),
    /// Writing to, or reading from, a cache file failed.
    Io(std::io::Error),
    /// What the server sent is not the JSON that was expected.
    Parse(String),
    /// The caller asked to stop.
    Cancelled,
    /// The bundled table has no version for this data version.
    UnknownVersion(i32),
}

impl std::fmt::Display for Error {
    fn fmt(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Error::Transport(message) => write!(formatter, "{message}"),
            Error::Status(code) => write!(formatter, "the server answered HTTP {code}"),
            Error::Io(error) => write!(formatter, "{error}"),
            Error::Parse(message) => write!(formatter, "{message}"),
            Error::Cancelled => write!(formatter, "the download was cancelled"),
            Error::UnknownVersion(version) => {
                write!(formatter, "no version found for data version {version}")
            }
        }
    }
}

impl std::error::Error for Error {}

impl From<std::io::Error> for Error {
    fn from(error: std::io::Error) -> Self {
        Error::Io(error)
    }
}

impl From<serde_json::Error> for Error {
    fn from(error: serde_json::Error) -> Self {
        Error::Parse(error.to_string())
    }
}

/// Where a download reports what it is doing. A GUI marshals these onto its
/// thread; a terminal frontend prints them.
pub trait Sink {
    /// A status line, for example "Downloading manifest.".
    fn info(&self, message: &str);
    /// Bytes received and the total (`total` is 0 when the server does not say).
    fn progress(&self, received: u64, total: u64);
}

/// Asks a running download to stop; checked between reads.
pub trait Cancel {
    fn cancelled(&self) -> bool;
}

/// A [`Sink`] that discards everything, for a caller that only wants the file.
pub struct Quiet;

impl Sink for Quiet {
    fn info(&self, _message: &str) {}
    fn progress(&self, _received: u64, _total: u64) {}
}

/// A [`Cancel`] that never fires.
pub struct Never;

impl Cancel for Never {
    fn cancelled(&self) -> bool {
        false
    }
}

/// Fetches files into one cache directory.
pub struct Downloader {
    cache_dir: PathBuf,
    /// Prefer the cached file when the server gives no freshness information
    /// (the `fail_download_use_cache` setting).
    prefer_cache: bool,
    agent: ureq::Agent,
}

impl Downloader {
    pub fn new(cache_dir: impl Into<PathBuf>) -> Self {
        Self {
            cache_dir: cache_dir.into(),
            prefer_cache: false,
            agent: ureq::AgentBuilder::new().timeout(TIMEOUT).build(),
        }
    }

    /// Sets whether a cached file is preferred when the server does not say how
    /// old it is.
    pub fn set_prefer_cache(&mut self, value: bool) {
        self.prefer_cache = value;
    }

    /// Fetches `object` (`minecraft/lang/zh_cn.json`) for the version whose data
    /// version is `data_version`, following the manifest and the asset index,
    /// and returns its path in the object store.
    pub fn download_object(
        &self,
        data_version: i32,
        object: &str,
        sink: &dyn Sink,
        cancel: &dyn Cancel,
    ) -> Result<PathBuf, Error> {
        let id = version_id(data_version).ok_or(Error::UnknownVersion(data_version))?;

        let manifest = self.download(
            VERSION_MANIFEST,
            &self.cache_dir,
            &gettext(i18n("Downloading manifest.")),
            sink,
            cancel,
        )?;
        let version_url = version_url(&manifest, &id)?;

        let version_json = self.download(
            &version_url,
            &cache::index_json(&self.cache_dir),
            &gettext(i18n("Downloading index json of this version.")),
            sink,
            cancel,
        )?;
        let asset_index_url = json_text(&read(&version_json)?, &["assetIndex", "url"])?;

        let asset_index = self.download(
            &asset_index_url,
            &cache::asset_json(&self.cache_dir),
            &gettext(i18n("Downloading asset index of this version.")),
            sink,
            cancel,
        )?;
        let hash = json_text(&read(&asset_index)?, &["objects", object, "hash"])?;

        let prefix = hash.get(..2).unwrap_or_default();
        let object_url = format!("{OBJECT_BASE}/{prefix}/{hash}");
        let path = self.download(
            &object_url,
            &cache::objects(&self.cache_dir),
            &gettext(i18n("Downloading object.")),
            sink,
            cancel,
        )?;
        sink.info(&gettext(i18n("Downloading finished!")));
        Ok(path)
    }

    /// Fetches `url` into `dir` (created when missing) and returns the file's
    /// path. A file already there is reused unless the source is newer; a
    /// partial download is resumed rather than started over.
    pub fn download(
        &self,
        url: &str,
        dir: &Path,
        info: &str,
        sink: &dyn Sink,
        cancel: &dyn Cancel,
    ) -> Result<PathBuf, Error> {
        let name = file_name(url).ok_or_else(|| Error::Parse(format!("no file name in {url}")))?;
        fs::create_dir_all(dir)?;
        let dest = dir.join(name);

        if dest.exists() && !self.source_newer(url, &dest) {
            return Ok(dest);
        }

        sink.info(info);
        self.fetch(url, &dest, sink, cancel)?;
        Ok(dest)
    }

    /// Fetches the client jar of the version `data_version` names into the
    /// cache directory's `client_jar/` folder and returns its path.
    ///
    /// This is the one file a 3D preview cannot do without: a version's
    /// blockstates, models and block textures are inside its client jar and not
    /// in the asset objects (which carry sounds, language files and a handful
    /// of GUI textures). A jar already in the cache is used as it is — it is
    /// named after the version and never changes — so opening a preview a
    /// second time downloads nothing.
    pub fn download_client(
        &self,
        data_version: i32,
        sink: &dyn Sink,
        cancel: &dyn Cancel,
    ) -> Result<PathBuf, Error> {
        let id = version_id(data_version).ok_or(Error::UnknownVersion(data_version))?;
        self.download_client_version(&id, sink, cancel)
    }

    /// The same, for a caller that already knows the version's id (`1.21.4`)
    /// rather than the data version a region carries.
    pub fn download_client_version(
        &self,
        version: &str,
        sink: &dyn Sink,
        cancel: &dyn Cancel,
    ) -> Result<PathBuf, Error> {
        let dest = cache::client_jar(&self.cache_dir, version);
        if is_a_file(&dest) {
            return Ok(dest);
        }

        let manifest = self.download(
            VERSION_MANIFEST,
            &self.cache_dir,
            &gettext(i18n("Downloading manifest.")),
            sink,
            cancel,
        )?;
        let version_url = version_url(&manifest, version)?;
        let version_json = self.download(
            &version_url,
            &cache::index_json(&self.cache_dir),
            &gettext(i18n("Downloading index json of this version.")),
            sink,
            cancel,
        )?;
        let client_url = json_text(&read(&version_json)?, &["downloads", "client", "url"])?;

        self.download_to(
            &client_url,
            &dest,
            &gettext(i18n("Downloading the client jar of this version.")),
            sink,
            cancel,
        )?;
        sink.info(&gettext(i18n("Downloading finished!")));
        Ok(dest)
    }

    /// Fetches `url` into the file `dest`, creating the directories above it.
    /// Unlike [`Downloader::download`], the file's name is the caller's rather
    /// than the one in the url, which is what the client jar needs: the url
    /// ends in `client.jar`, while the resource layer looks for
    /// `<version>.jar`.
    pub fn download_to(
        &self,
        url: &str,
        dest: &Path,
        info: &str,
        sink: &dyn Sink,
        cancel: &dyn Cancel,
    ) -> Result<(), Error> {
        if let Some(parent) = dest.parent() {
            fs::create_dir_all(parent)?;
        }
        if is_a_file(dest) && !self.source_newer(url, dest) {
            return Ok(());
        }
        sink.info(info);
        self.fetch(url, dest, sink, cancel)
    }

    /// Whether the file at `url` is newer than the one already at `dest`. When
    /// the server does not say, the cache is preferred exactly when the
    /// configuration asks for it.
    fn source_newer(&self, url: &str, dest: &Path) -> bool {
        let last_modified = self
            .agent
            .head(url)
            .call()
            .ok()
            .and_then(|response| response.header("Last-Modified").map(str::to_owned));
        let Some(last_modified) = last_modified else {
            return !self.prefer_cache;
        };
        let Ok(source) = httpdate::parse_http_date(&last_modified) else {
            return !self.prefer_cache;
        };
        let Ok(metadata) = fs::metadata(dest) else {
            return true;
        };
        let local = metadata
            .modified()
            .or_else(|_| metadata.created())
            .unwrap_or(SystemTime::UNIX_EPOCH);
        local < source
    }

    /// Downloads `url` to `dest` through a temporary sibling, so an interrupted
    /// run leaves the finished file untouched and can be resumed.
    fn fetch(
        &self,
        url: &str,
        dest: &Path,
        sink: &dyn Sink,
        cancel: &dyn Cancel,
    ) -> Result<(), Error> {
        let temp = temp_path(dest);
        let offset = fs::metadata(&temp).map(|meta| meta.len()).unwrap_or(0);

        let mut request = self.agent.get(url);
        if offset > 0 {
            request = request.set("Range", &format!("bytes={offset}-"));
        }
        let response = request.call().map_err(transport_error)?;

        /* A server that ignores the range answers 200 with the whole file. */
        let resume = offset > 0 && response.status() == 206;
        if !resume {
            fs::write(&temp, b"")?;
        }
        let mut received = if resume { offset } else { 0 };

        let total = response
            .header("Content-Length")
            .and_then(|value| value.parse::<u64>().ok())
            .map(|length| length + received)
            .unwrap_or(received);

        let mut file = fs::OpenOptions::new()
            .create(true)
            .append(true)
            .open(&temp)?;
        let mut reader = response.into_reader();
        let mut buffer = vec![0u8; CHUNK];
        sink.progress(received, total);
        loop {
            if cancel.cancelled() {
                return Err(Error::Cancelled);
            }
            let read = reader.read(&mut buffer)?;
            if read == 0 {
                break;
            }
            file.write_all(&buffer[..read])?;
            received += read as u64;
            sink.progress(received, total);
        }
        file.flush()?;
        drop(file);

        if dest.exists() {
            fs::remove_file(dest)?;
        }
        fs::rename(&temp, dest)?;
        Ok(())
    }
}

/// Whether `path` is a file with something in it: a download that died half way
/// leaves an empty file, which must not be mistaken for the real thing.
fn is_a_file(path: &Path) -> bool {
    fs::metadata(path)
        .map(|metadata| metadata.is_file() && metadata.len() > 0)
        .unwrap_or(false)
}

/// Whether the client jar of `version` is already in `cache_dir`, which is what
/// a frontend asks before it offers to download one.
pub fn client_jar_is_there(cache_dir: &Path, version: &str) -> bool {
    is_a_file(&cache::client_jar(cache_dir, version))
}

/// The version id (`1.21.4`) a data version names, from the bundled table.
///
/// Public because a frontend that has to point the resource layer at a
/// version's client jar — the renderer's models and textures are only in there
/// — resolves it exactly as the downloader does, rather than keeping a second
/// copy of the table or of the lookup.
pub fn version_id(data_version: i32) -> Option<String> {
    for line in bundled::data_version_csv().lines() {
        let line = line.trim();
        if line.is_empty() {
            continue;
        }
        let Some((name, version)) = line.split_once(',') else {
            continue;
        };
        if version.trim().parse::<i32>().ok() == Some(data_version) {
            return Some(name.trim().to_owned());
        }
    }
    None
}

/// The url of `id` inside the downloaded version manifest.
fn version_url(manifest: &Path, id: &str) -> Result<String, Error> {
    let json: Value = serde_json::from_str(&read(manifest)?)?;
    json.get("versions")
        .and_then(Value::as_array)
        .and_then(|versions| {
            versions
                .iter()
                .find(|entry| entry.get("id").and_then(Value::as_str) == Some(id))
        })
        .and_then(|entry| entry.get("url"))
        .and_then(Value::as_str)
        .map(str::to_owned)
        .ok_or_else(|| Error::Parse(format!("no url for version {id} in the manifest")))
}

/// A nested string value (`["assetIndex", "url"]`) of some JSON text.
fn json_text(text: &str, path: &[&str]) -> Result<String, Error> {
    let json: Value = serde_json::from_str(text)?;
    let mut value = &json;
    for key in path {
        value = value
            .get(key)
            .ok_or_else(|| Error::Parse(format!("missing {}", path.join("."))))?;
    }
    value
        .as_str()
        .map(str::to_owned)
        .ok_or_else(|| Error::Parse(format!("{} is not text", path.join("."))))
}

fn read(path: &Path) -> Result<String, Error> {
    Ok(fs::read_to_string(path)?)
}

/// The last path segment of `url`, with any query or fragment dropped.
fn file_name(url: &str) -> Option<&str> {
    let path = url.split(['?', '#']).next().unwrap_or(url);
    path.rsplit('/').next().filter(|name| !name.is_empty())
}

/// The temporary sibling a download is written to before it is moved into place.
fn temp_path(dest: &Path) -> PathBuf {
    let mut name = dest.file_name().unwrap_or_default().to_os_string();
    name.push(".dhtmpf");
    dest.with_file_name(name)
}

fn transport_error(error: ureq::Error) -> Error {
    match error {
        ureq::Error::Status(code, _) => Error::Status(code),
        ureq::Error::Transport(transport) => Error::Transport(transport.to_string()),
    }
}

#[cfg(test)]
mod tests {
    use super::{Downloader, Error, Quiet, Never, file_name, is_a_file, json_text, temp_path, version_id, version_url};
    use std::path::Path;

    #[test]
    fn a_data_version_maps_to_its_id() {        /* The table is `id,data version`, newest first. */
        assert_eq!(version_id(4903).as_deref(), Some("26.2"));
        assert!(version_id(-1).is_none());
    }

    #[test]
    fn a_url_names_its_file() {
        assert_eq!(file_name("https://host/a/b.json"), Some("b.json"));
        assert_eq!(file_name("https://host/a/b.json?v=1"), Some("b.json"));
        assert_eq!(file_name("https://host/a/"), None);
    }

    #[test]
    fn the_temporary_sibling_keeps_the_directory() {
        assert_eq!(
            temp_path(Path::new("/cache/object/ab12")),
            Path::new("/cache/object/ab12.dhtmpf")
        );
    }

    #[test]
    fn a_nested_string_is_read_from_json() {
        let text = r#"{"assetIndex":{"url":"https://host/index.json"}}"#;
        assert_eq!(
            json_text(text, &["assetIndex", "url"]).unwrap(),
            "https://host/index.json"
        );
        assert!(matches!(
            json_text(text, &["assetIndex", "missing"]),
            Err(Error::Parse(_))
        ));
    }

    #[test]
    fn the_version_url_is_found_in_the_manifest() {
        let dir = std::env::temp_dir().join(format!("dhlrc-download-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let manifest = dir.join("manifest.json");
        std::fs::write(
            &manifest,
            r#"{"versions":[{"id":"1.21.4","url":"https://host/1.21.4.json"}]}"#,
        )
        .unwrap();
        assert_eq!(
            version_url(&manifest, "1.21.4").unwrap(),
            "https://host/1.21.4.json"
        );
        assert!(matches!(
            version_url(&manifest, "9.9"),
            Err(Error::Parse(_))
        ));
        std::fs::remove_dir_all(&dir).ok();
    }

    /// A cache directory of this test's own, named by `suffix`.
    fn cache(suffix: &str) -> std::path::PathBuf {
        let dir = std::env::temp_dir().join(format!(
            "dhlrc-download-{}-{suffix}",
            std::process::id()
        ));
        std::fs::create_dir_all(&dir).unwrap();
        dir
    }

    #[test]
    fn a_client_jar_already_in_the_cache_is_used_as_it_is() {
        /* No network: the version's jar is there, so nothing is fetched. A
         * second preview of the same version must not download 30 MB again. */
        let dir = cache("client");
        let dest = crate::resource::cache::client_jar(&dir, "1.21.4");
        std::fs::create_dir_all(dest.parent().unwrap()).unwrap();
        std::fs::write(&dest, b"a jar").unwrap();

        let downloader = Downloader::new(&dir);
        let path = downloader
            .download_client(4189, &Quiet, &Never)
            .expect("the cached jar");
        assert_eq!(path, dest);
        assert_eq!(std::fs::read(&path).unwrap(), b"a jar");

        std::fs::remove_dir_all(&dir).ok();
    }

    #[test]
    fn an_empty_client_jar_is_not_mistaken_for_one() {
        let dir = cache("empty");
        let dest = crate::resource::cache::client_jar(&dir, "1.21.4");
        std::fs::create_dir_all(dest.parent().unwrap()).unwrap();
        std::fs::write(&dest, b"").unwrap();
        assert!(!is_a_file(&dest));
        std::fs::remove_dir_all(&dir).ok();
    }

    #[test]
    fn a_data_version_no_version_has_cannot_be_downloaded() {
        let dir = cache("unknown");
        let downloader = Downloader::new(&dir);
        assert!(matches!(
            downloader.download_client(-1, &Quiet, &Never),
            Err(Error::UnknownVersion(-1))
        ));
        std::fs::remove_dir_all(&dir).ok();
    }
}
