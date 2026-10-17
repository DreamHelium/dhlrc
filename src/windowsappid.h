#ifndef WINDOWSAPPID_H
#define WINDOWSAPPID_H

/* Gives the process a Windows AppUserModelID and registers the name and icon
 * Windows shows for it. Without it, a toast notification is attributed to the
 * process that creates it (PowerShell's ID is what the notification crate
 * falls back to when it has none), which is why Windows would show "Windows
 * PowerShell" as the sender.
 *
 * Call once at startup, before the first window is created. A no-op on other
 * platforms. */
void applyWindowsAppUserModelID ();

#endif /* WINDOWSAPPID_H */
