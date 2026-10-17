/*! Turning a `Notification` into a desktop notification.
 *
 * Built only with the `desktop-notifications` feature, which pulls in
 * `notify-rust`. That crate talks to the platform's notification service (the
 * `org.freedesktop.Notifications` D-Bus service on Linux and BSD, a WinRT toast
 * on Windows, Notification Center on macOS), so a build with no desktop — a
 * terminal frontend on a server, say — can leave the feature off and print the
 * `Notification` instead.
 *
 * Only `summary` and `body` are shared by all three; the name of the sender is
 * per platform and comes from `app_id` (see `[notifications] app_id`).
 */

#[cfg(not(target_os = "macos"))]
use crate::notification::Level;
use crate::notification::Notification;
use notify_rust::Notification as DesktopNotification;
use notify_rust::Timeout;
#[cfg(not(target_os = "macos"))]
use notify_rust::Urgency;

/// Shows `notification` on the desktop.
///
/// `app_id` names the sender where the platform has its own concept of one (see
/// [`crate::config::Notifications`]); `None` leaves the platform's default.
/// `timeout_ms` is how long it stays before it disappears; zero leaves it to the
/// platform.
///
/// Fails when there is no notification service to talk to (a headless session,
/// for example); the caller decides what to do then.
pub fn present(
    notification: &Notification,
    app_id: Option<&str>,
    timeout_ms: u32,
) -> Result<(), notify_rust::error::Error> {
    /* Only Windows and macOS have a sender of their own; XDG names it with
     * `appname` below. */
    #[cfg(not(any(target_os = "windows", target_os = "macos")))]
    let _ = app_id;

    /* macOS picks the delivering application before the notification is built;
     * `appname` is a no-op there. */
    #[cfg(target_os = "macos")]
    aim_macos_at_app(app_id);

    let mut toast = DesktopNotification::new();
    toast
        .appname("dhlrc")
        .summary(&notification.title)
        .body(&notification.text);
    /* Windows names the sender with its own `System.AppUserModel.ID`. This has
     * to be set: with none, the notification crate falls back to PowerShell's
     * id and Windows shows the toast as coming from PowerShell. An empty value
     * is treated as "the default", which is the id the installer registers. */
    #[cfg(target_os = "windows")]
    {
        let id = app_id
            .map(str::trim)
            .filter(|id| !id.is_empty())
            .unwrap_or("cn.dh.dhlrc");
        toast.app_id(id);
    }
    if timeout_ms > 0 {
        toast.timeout(Timeout::Milliseconds(timeout_ms));
    }
    /* The default macOS backend has no urgency, and its newer one takes a
     * different type, so urging is left to the other two. */
    #[cfg(not(target_os = "macos"))]
    toast.urgency(urgency(notification.level));
    toast.show().map(|_handle| ())
}

/// Shows each of `notifications` as [`present`] would, ignoring any that fail.
pub fn present_all(notifications: &[Notification], app_id: Option<&str>, timeout_ms: u32) {
    for notification in notifications {
        let _ = present(notification, app_id, timeout_ms);
    }
}

/// How loud to make the desktop notification.
#[cfg(not(target_os = "macos"))]
fn urgency(level: Level) -> Urgency {
    match level {
        Level::Info => Urgency::Low,
        Level::Warning => Urgency::Normal,
        Level::Error => Urgency::Critical,
    }
}

/// Points the macOS notification stack at `app_id`, so notifications are
/// delivered as that bundle. A no-op when it is unset or unchanged.
#[cfg(target_os = "macos")]
fn aim_macos_at_app(app_id: Option<&str>) {
    use std::sync::{Mutex, OnceLock};

    static APPLIED: OnceLock<Mutex<Option<String>>> = OnceLock::new();
    let applied = APPLIED.get_or_init(|| Mutex::new(None));
    let Ok(mut applied) = applied.lock() else {
        return;
    };
    if applied.as_deref() == app_id {
        return;
    }
    if let Some(app_id) = app_id {
        let _ = notify_rust::set_application(app_id);
    }
    *applied = app_id.map(str::to_owned);
}
