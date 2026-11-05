# Conventions

Short rules that keep the codebase boring to work on. They describe what the
existing code already does; follow it unless there is a reason not to.

## Language and formatting

- C++23 + Qt 6/KF6 in `src/`; Rust edition 2024 elsewhere.
- C++ is formatted with `.clang-format` (GNU base): two-space indent, braces on
  their own line, a space before the parentheses of a call or definition
  (`foo ()`, `MainWindow::open ()`). Run `clang-format` on the files you touch.
- Rust uses the default `rustfmt`; do not hand-wrap.

## Naming

| Pattern                       | Meaning                                                       |
| ----------------------------- | ------------------------------------------------------------- |
| `DhXxx` (`DhCore`, `DhLoadJob`) | shared infrastructure widget/class                          |
| `XxxUI` (`BlockListUI`, `ManageRegionUI`) | a page, tab or dialog the user sees              |
| `XxxJob` (`SaveRegionJob`)    | a unit of background work, a `KJob` on the thread pool        |
| `dh::` namespaced free function | helpers in `utility.h`, `version.h`, `dhlog.h`                |
| `ModuleBase` / `Single`/`Multi` | the C++ wrapper over one loaded region plugin                |
| `dhlrc_core_*`, `dhlrc_render_*` | C ABI functions; the types they expose are `Dhlrc*`         |

Methods are `lowerCamelCase`, types `UpperCamelCase`, members plain
`lowerCamelCase` (`ui`, `base`); keep whatever a file already uses rather than
renaming for taste.

## Comments

Comments explain **why** and the constraint being respected, never the syntax.
Class and header comments carry the ownership, threading and lifecycle rules —
that is where a future editor looks before touching the class. Rust modules use
`/*! ... */` and public items `///`. When a rule outgrows a comment, put the
detail in `dev-doc/` and link it; when behaviour changes, fix the comment in the
same commit. A commented-out block is not documentation — delete it.

## Strings, translation, logging

- C++: `#define _(str) gettext (str)` at the top of each translation unit;
  `N_()` marks a string that is translated later (help-topic titles).
- Rust: `i18n()` (core/plugins) or `gettext_text` (NBT helpers); a plugin's file
  filter string is translated by the plugin, never twice.
- New user-visible strings need `make_po.sh` run and the `.po` update, or they
  never get translated.
- Log through `qDebug ()`/`qWarning ()`/`qCritical ()` (the handler in
  `dhlog.cpp` writes the daily file) or the core's log module. Never `printf` in
  library code; never log a failure and silently continue.

## Errors and ownership

- A user-facing failure reaches the notification centre (Qt) or is returned as
  an error string (Rust); `qWarning ()` alone is not error handling.
- C ABI: a borrowed `const char *` lives as long as its object, a caller-kept
  `char *` is freed with `dhlrc_string_free`; region objects use `region_free`.
  Never mix the allocators.
- Qt: use parent ownership, `deleteLater ()` across signal boundaries, and keep
  widget access on the GUI thread — workers report through signals.
- Plugins are loaded by the core once and stay loaded; nothing frontend-side
  calls `dlopen`.

## Adding code

- A new `.cpp`/`.h` goes into the right list in `src/CMakeLists.txt`
  (`TEMP_PROJECT_SOURCES`), a new `.ui` into `ui_files`.
- Before writing a helper, check [reuse.md](reuse.md); shared logic belongs in
  those files, not in a second copy.
- A new setting goes into the core schema, not into the dialog by hand; a new
  page follows the existing workspace/tab pattern (see
  [frontend.md](frontend.md)).
- Behaviour covered by `--ui-check` or `--preview` must keep those checks true;
  extend them when the invariant changes ([uicheck.md](uicheck.md)).

## Tests and commits

- Run `cargo test --workspace` and the relevant GUI check from
  [build.md](build.md) before calling work done. For interactive behaviour,
  prefer the local `temp/` harnesses when that directory exists — read
  `temp/HARNESSES.md` first.
- Commit subjects are short and imperative (`Add …`, `Fix …`), one behaviour per
  commit; the body says why when the diff cannot.
- The docs in `dev-doc/` are part of the change: update the page that describes
  the behaviour you touched and keep the list in `dev-doc/README.md` current.
