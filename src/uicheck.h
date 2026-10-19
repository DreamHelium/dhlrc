#ifndef DHLRC_UICHECK_H
#define DHLRC_UICHECK_H

#include <QString>

/* The built-in interface self-check, compiled only with `-DDH_UI_CHECK=ON`.
 *
 * It drives a real `MainWindow` the way a user would (open the settings
 * dialog, change controls, cancel, reopen, apply), asserts what the interface
 * should do, writes a report and a few screenshots into a throw-away
 * directory, and returns a process exit code: 0 when every check passed.
 *
 * The checks run without `QApplication::exec ()`: they spin the event loop
 * themselves while waiting for the asynchronous parts (loading a projection,
 * for instance), so the mode can be used from a script or from CI. */

/* Where `--ui-check` writes `report.txt` and the screenshots. Set from
 * `main ()` before the run; empty means "do not write files". */
void dhSetUiCheckArtifactsDir (const QString &directory);

/* Runs the checks and returns the exit code. */
int dhRunUiCheck ();

#endif /* DHLRC_UICHECK_H */
