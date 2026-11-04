/*! The core's logging façade.
 *
 * The core never writes a log file: it turns a record into a level, a target
 * (the module it came from) and a message, and hands it to the frontend's
 * sink. A C frontend installs one through `dhlrc_log_set_sink`; a Rust
 * frontend uses [`set_sink`]. With no sink the records go to stderr, which is
 * what a plain terminal run wants.
 *
 * The sink is process-wide and may be called from any thread. It is called
 * with the registry lock released, so it may do whatever a frontend's logging
 * needs — including writing to a file — without risking a deadlock. A sink
 * that logs again would recurse; the second record goes to stderr instead.
 */

use std::cell::Cell;
use std::ffi::{CString, c_char, c_void};
use std::fmt::Arguments;
use std::sync::{Arc, RwLock};

/// How serious a record is. The order is the usual one (debug first, error
/// last), so a minimum level filters everything quieter than it.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum Level {
    Debug,
    Info,
    Warning,
    Error,
}

impl Level {
    /// The tag the plain-text fallback puts in front of a record.
    pub fn tag(self) -> &'static str {
        match self {
            Level::Debug => "[DEBUG]",
            Level::Info => "[INFO]",
            Level::Warning => "[WARNING]",
            Level::Error => "[ERROR]",
        }
    }

    /// The number the C ABI uses (`DHLRC_LOG_*`).
    pub fn as_int(self) -> i32 {
        match self {
            Level::Debug => 0,
            Level::Info => 1,
            Level::Warning => 2,
            Level::Error => 3,
        }
    }

    /// Parses the names the command line and `DHLRC_LOG_LEVEL` accept,
    /// ignoring case and surrounding whitespace.
    pub fn from_name(name: &str) -> Option<Level> {
        match name.trim().to_ascii_lowercase().as_str() {
            "debug" => Some(Level::Debug),
            "info" => Some(Level::Info),
            "warning" | "warn" => Some(Level::Warning),
            "error" | "critical" => Some(Level::Error),
            _ => None,
        }
    }
}

/// A Rust frontend's sink: level, target (the module), message.
pub type Sink = Arc<dyn Fn(Level, &str, &str) + Send + Sync>;

/// The C ABI's sink. The strings are borrowed and only valid for the call.
pub type CSink = extern "C" fn(
    user_data: *mut c_void,
    level: i32,
    target: *const c_char,
    message: *const c_char,
);

#[derive(Clone)]
enum Installed {
    Rust(Sink),
    C(CSink, usize),
}

static SINK: RwLock<Option<Installed>> = RwLock::new(None);

thread_local! {
    /// Guards against a sink that logs through the core again.
    static INSIDE_EMIT: Cell<bool> = const { Cell::new(false) };
}

/// Installs a Rust sink; `None` restores the stderr fallback.
pub fn set_sink(sink: Option<Sink>) {
    let installed = sink.map(Installed::Rust);
    if let Ok(mut guard) = SINK.write() {
        *guard = installed;
    }
}

/// Installs a C sink; `None` restores the stderr fallback. Called by
/// `dhlrc_log_set_sink`.
pub fn set_c_sink(sink: Option<CSink>, user_data: *mut c_void) {
    let installed = sink.map(|sink| Installed::C(sink, user_data as usize));
    if let Ok(mut guard) = SINK.write() {
        *guard = installed;
    }
}

/// Writes the record currently being formatted. The macros below call this;
/// a frontend with its own messages can call it directly.
pub fn emit(level: Level, target: &str, args: Arguments<'_>) {
    let message = args.to_string();
    if INSIDE_EMIT.with(|inside| inside.replace(true)) {
        /* A sink logged from its own call: do not recurse into it. */
        eprintln!("{} {target}: {message}", level.tag());
        INSIDE_EMIT.with(|inside| inside.set(false));
        return;
    }
    match installed() {
        Some(Installed::Rust(sink)) => sink(level, target, &message),
        Some(Installed::C(sink, user_data)) => {
            let target = CString::new(target).unwrap_or_default();
            let message = CString::new(message).unwrap_or_default();
            sink(
                user_data as *mut c_void,
                level.as_int(),
                target.as_ptr(),
                message.as_ptr(),
            );
        }
        None => eprintln!("{} {target}: {message}", level.tag()),
    }
    INSIDE_EMIT.with(|inside| inside.set(false));
}

fn installed() -> Option<Installed> {
    SINK.read().ok().and_then(|guard| guard.clone())
}

/// Logs at [`Level::Debug`].
#[macro_export]
macro_rules! log_debug {
    ($($arg:tt)*) => {
        $crate::log::emit($crate::log::Level::Debug, module_path!(), format_args!($($arg)*))
    };
}

/// Logs at [`Level::Info`].
#[macro_export]
macro_rules! log_info {
    ($($arg:tt)*) => {
        $crate::log::emit($crate::log::Level::Info, module_path!(), format_args!($($arg)*))
    };
}

/// Logs at [`Level::Warning`].
#[macro_export]
macro_rules! log_warning {
    ($($arg:tt)*) => {
        $crate::log::emit($crate::log::Level::Warning, module_path!(), format_args!($($arg)*))
    };
}

/// Logs at [`Level::Error`].
#[macro_export]
macro_rules! log_error {
    ($($arg:tt)*) => {
        $crate::log::emit($crate::log::Level::Error, module_path!(), format_args!($($arg)*))
    };
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::Mutex;

    #[test]
    fn levels_parse_and_rank() {
        assert_eq!(Level::from_name(" Debug "), Some(Level::Debug));
        assert_eq!(Level::from_name("warning"), Some(Level::Warning));
        assert_eq!(Level::from_name("warn"), Some(Level::Warning));
        assert_eq!(Level::from_name("nope"), None);
        assert!(Level::Debug < Level::Info && Level::Info < Level::Error);
        assert_eq!(Level::Error.as_int(), 3);
    }

    #[test]
    fn a_sink_receives_the_record() {
        static SEEN: Mutex<Vec<(Level, String, String)>> = Mutex::new(Vec::new());
        set_sink(Some(Arc::new(|level, target, message| {
            SEEN.lock()
                .unwrap()
                .push((level, target.to_owned(), message.to_owned()));
        })));
        log_warning!("module {} did not load", "x");
        set_sink(None);

        let seen = SEEN.lock().unwrap();
        assert!(
            seen.iter().any(|(level, target, message)| {
                *level == Level::Warning
                    && target.ends_with("log::tests")
                    && message == "module x did not load"
            }),
            "captured: {seen:?}"
        );
    }
}
