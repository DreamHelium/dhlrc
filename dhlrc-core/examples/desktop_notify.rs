//! Shows one desktop notification, the way a terminal frontend would.
//!
//! Run it with the feature on:
//!
//! ```text
//! cargo run -p dhlrc-core --features desktop-notifications --example desktop_notify
//! ```

use dhlrc_core::desktop;
use dhlrc_core::notification::{Level, Notification};

fn main() {
    let notification = Notification::new(
        "configReloaded",
        Level::Info,
        "Configuration reloaded",
        "The configuration was reloaded from ~/.config/dhlrc/config.toml.",
    );

    /* The sender the `[notifications] app_id` setting names, taken here from the
     * command line for brevity; the timeout matches the default setting. */
    let app_id = std::env::args().nth(1);
    match desktop::present(&notification, app_id.as_deref(), 5000) {
        Ok(()) => println!("shown"),
        Err(error) => eprintln!("could not show the notification: {error}"),
    }
}
