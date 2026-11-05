# Working on dhlrc

dhlrc is a Minecraft structure/region editor: a Qt 6/KF6 frontend in `src/`, a
Rust core and libraries elsewhere, and file formats loaded as plugins.

Start with [dev-doc/architecture.md](dev-doc/architecture.md) — it maps the
targets, the data flow and the page to read for each part of the code. Then:

- [dev-doc/build.md](dev-doc/build.md) — build, package and test commands;
- [dev-doc/conventions.md](dev-doc/conventions.md) — style, naming, ownership
  and the rules for adding a file;
- [dev-doc/reuse.md](dev-doc/reuse.md) — the shared helpers; check it before
  writing new code.

Interactive behaviour checks: if a `temp/` directory exists in your working
copy, read `temp/HARNESSES.md` first and use those harnesses — they link the
real build with only `main.cpp` replaced. `temp/` is ignored by git, so never
commit it and never assume a fresh clone has it; fall back to
`dev-doc/build.md` when it is missing.

Changes are expected to keep the docs true: update the relevant `dev-doc/`
page in the same change as the code.
