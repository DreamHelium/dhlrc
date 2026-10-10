/*! A notification the core wants shown to the user.
 *
 * The core never draws anything: it produces these values and the frontend
 * decides how to present them (a desktop notification, a terminal escape
 * sequence, a line of text, …). That is what lets the same core drive a GUI and
 * a terminal UI without either one leaking into the other.
 */

/// How loud a notification is.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Level {
    Info,
    Warning,
    Error,
}

/// One message, with a stable `event_id` so a frontend can group, route or mute
/// it (the Qt frontend maps it to a `KNotification` event).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Notification {
    pub event_id: &'static str,
    pub level: Level,
    pub title: String,
    pub text: String,
}

impl Notification {
    pub fn new(
        event_id: &'static str,
        level: Level,
        title: impl Into<String>,
        text: impl Into<String>,
    ) -> Self {
        Self {
            event_id,
            level,
            title: title.into(),
            text: text.into(),
        }
    }
}
