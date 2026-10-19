# Interface self-check (`--ui-check`)

A development tool that drives a real `MainWindow` and asserts what the
interface does. It exists because some interface bugs are only visible in the
interaction: the settings dialog's Apply button once stayed lit after the
edits were cancelled, because the Cancel path restored the controls but never
re-ran the "is there anything pending?" check. A check that clicks the same
buttons catches that class of bug without a human watching the window.

The mode is compiled only with `-DDH_UI_CHECK=ON`, so a normal build carries
neither the code nor the command-line option.

```bash
cmake -B build -DDH_UI_CHECK=ON && cmake --build build
# A display is needed even headless: the program initialises its icon theme
# before the application object exists, which blocks with the offscreen
# platform plugin; Xvfb is enough.
Xvfb :99 -screen 0 1400x900x24 &
DHLRC_UI_CHECK_DIR=ui-check-output DISPLAY=:99 ./build/dhlrc_qt --ui-check
echo $?    # 0 when every check passed
```

`DHLRC_UI_CHECK_DIR` is where `report.txt` and the screenshots go. Without it
the run gets a temporary directory and prints its path. The run also points
`XDG_CONFIG_HOME`, `XDG_CACHE_HOME` and `XDG_DATA_HOME` into that directory, so
applying settings never touches the user's own configuration; the check
verifies that the core's config path really is inside it.

The checks cover the invariants that have been got wrong before:

- the settings dialog opens; **Apply** starts disabled, follows edits, and
  turns off again when the edit is cancelled or the dialog is closed (Cancel,
  Esc and the window's close button all throw the pending edits away);
- **Apply** really writes the item and disables itself afterwards;
- a plugin's integer option offers the range the plugin advertises (the Sponge
  format version is 0–3, not `0..INT_MAX`) and its rows are greyed out while
  the "use saved options" switch is off;
- a value changed and changed back by hand (or by a refresh, e.g. after the
  configuration was reloaded) turns Apply off again, and so do the multi-region
  name pattern's sample values, which have no control of their own and only
  reach the dialog through the small preview window;
- one tab per region: the reader, the material list and the 3D view are pages
  inside the region's workspace tab, and the 3D view is not a window of its
  own.

Adding a check is one call: `check (name, condition)` records `PASS`/`FAIL`
and the process returns non-zero if anything failed; `skip (name, why)` is for
parts that need something the build may not have (a module that is not
loaded); `waitUntil (predicate)` spins the event loop for the asynchronous
parts instead of sleeping. Assertions read the objects the interface itself
uses (`dialog->templateFor (key)->widget`, `dialog->item (key)`, the tab
widget), so they stay honest when the layout changes.

The check runs synchronously (it never calls `QApplication::exec ()`), which
makes it usable from a script or a CI job. It is a smoke test rather than a
replacement for looking at the window: it proves the behaviours above, not
that every pixel is in place.
