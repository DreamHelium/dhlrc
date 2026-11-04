/*! The terminal frontend's log file sink.
 *
 * The core hands every record to a process-wide sink; this one appends them to
 * `<cache>/logs/dhlrc-tui-YYYY-MM-DD.log`, one line per record and a new file
 * at midnight. The directory and the level names are the same as the Qt
 * frontend's, so the two logs sit side by side and read the same way; the file
 * name carries the frontend so two processes never append to the same file.
 *
 * Timestamps are UTC: the local offset needs the platform's time-zone tables,
 * which would be one more thing a terminal build must carry. The Qt file keeps
 * local time with its offset; both lines are self-contained.
 */

use dhlrc_core::log::{Level, Sink};
use std::fs::{File, OpenOptions};
use std::io::Write;
use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex};
use time::OffsetDateTime;

/// One line per record: date and time (UTC), level, target, message. Line
/// breaks inside the message are escaped, so a record never spans two lines.
fn format_line(now: OffsetDateTime, level: Level, target: &str, message: &str) -> String {
    let message = message.replace('\r', "\\r").replace('\n', "\\n");
    format!(
        "{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03} +00:00 {} {} {}",
        now.year(),
        now.month() as u8,
        now.day(),
        now.hour(),
        now.minute(),
        now.second(),
        now.millisecond(),
        level.tag(),
        target,
        message
    )
}

fn date_stamp(now: OffsetDateTime) -> String {
    format!("{:04}{:02}{:02}", now.year(), now.month() as u8, now.day())
}

struct State {
    directory: PathBuf,
    level: Level,
    date: String,
    file: Option<File>,
}

/// Installs the file sink; records quieter than `level` are dropped. Fails
/// before changing the sink when the directory cannot be created, so the
/// caller can report that and keep the stderr fallback.
pub fn install(cache_dir: &Path, level: Level) -> std::io::Result<()> {
    let directory = cache_dir.join("logs");
    std::fs::create_dir_all(&directory)?;
    let state = Mutex::new(State {
        directory,
        level,
        date: String::new(),
        file: None,
    });
    let sink: Sink = Arc::new(move |level, target, message| {
        let mut state = match state.lock() {
            Ok(guard) => guard,
            Err(_) => return,
        };
        if level < state.level {
            return;
        }
        let now = OffsetDateTime::now_utc();
        let date = date_stamp(now);
        if state.file.is_none() || state.date != date {
            let path = state.directory.join(format!("dhlrc-tui-{date}.log"));
            match OpenOptions::new().create(true).append(true).open(&path) {
                Ok(file) => {
                    state.file = Some(file);
                    state.date = date;
                }
                Err(_) => {
                    state.file = None;
                    state.date = date;
                    return;
                }
            }
        }
        if let Some(file) = state.file.as_mut() {
            let _ = writeln!(file, "{}", format_line(now, level, target, message));
            /* Flushed per record so a crash still leaves the log on disk. */
            let _ = file.flush();
        }
    });
    dhlrc_core::log::set_sink(Some(sink));
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_record_is_one_escaped_line() {
        let now = OffsetDateTime::now_utc();
        let line = format_line(now, Level::Warning, "dhlrc_core::modules", "one\ntwo");
        assert!(line.contains("[WARNING] dhlrc_core::modules one\\ntwo"));
        assert!(!line.contains('\n'));
    }

    #[test]
    fn the_file_is_per_day_and_per_frontend() {
        let now = OffsetDateTime::now_utc();
        assert_eq!(date_stamp(now).len(), 8);
        assert!(date_stamp(now).chars().all(|ch| ch.is_ascii_digit()));
    }
}
