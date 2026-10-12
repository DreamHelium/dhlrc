# dhlrc-tui

An experimental terminal frontend for dhlrc, built on the Qt-free `dhlrc-core`.
It exists so dhlrc can eventually run where there is no GUI.

**Status:** only the command line exists. The interactive UI is not written
yet, so running `dhlrc-tui` with no command prints the help.

## Commands

### `modules`

Reports what the core can load and, when both lists are shown, points out the
region modules whose object codec is not installed.

```console
$ dhlrc-tui modules
Region modules (2)
  /path/to/build/region_module
  #  type       suffix     codec  regions  encoding
  0  litematic  litematic  NBT    multi    big-endian
  1  nbt        nbt        NBT    single   big-endian

Object codecs (1)
  /path/to/build/load_module
  #  codec
  0  NBT
```

| Option                       | Meaning                                                         |
| ---------------------------- | --------------------------------------------------------------- |
| `--region-dir <DIR>`         | Region modules; default is `region_module/` next to the binary. |
| `--load-dir <DIR>`           | Object codecs; default is `load_module/` next to the binary.    |
| `--kind <all\|region\|load>` | Which list to report.                                           |
| `-v`, `--verbose`            | Also print the path of every library.                           |

The defaults sit next to the _binary_. The CMake build stages the binary in its
build directory (`build/dhlrc-tui`), beside `region_module/`, `load_module/` and
`locale/`, so running it there needs no arguments:

```console
$ ./build/dhlrc-tui modules
```

A plain `cargo build` leaves the binary in `target/debug/`, where those
directories are not; point `--region-dir` and `--load-dir` at a build's
directories then. The region modules also depend on `libregion_rs.so`, which the
CMake build makes findable with a runpath; a bare cargo binary needs it on the
loader path (`LD_LIBRARY_PATH=build`).

### `completions`

Installs a completion script into the directory the shell loads completions
from for the current user, generated from the same description the parser uses.

```console
$ dhlrc-tui completions bash   # ~/.local/share/bash-completion/completions/dhlrc-tui
$ dhlrc-tui completions zsh    # ~/.local/share/zsh/site-functions/_dhlrc-tui
$ dhlrc-tui completions fish   # ~/.config/fish/completions/dhlrc-tui.fish
```

`$XDG_DATA_HOME` and `$XDG_CONFIG_HOME` are honoured. `--output <DIR>` installs
into another directory, and `--print` writes the script to standard output
instead — handy for packaging, and the fallback for PowerShell, which has no
such directory.

zsh only reads completions from `$fpath`, so its per-user directory has to be
added to it; the command prints a reminder naming that directory.

## Translation

The messages go through gettext under the project-wide `dhlrc` domain, so they
share the Qt frontend's catalogue. The catalogue is looked up in `locale/` next
to the executable, which the CMake build already satisfies for
`build/dhlrc-tui`. Set `LANG` to check them:

```console
$ LANG=zh_CN.UTF-8 ./build/dhlrc-tui modules
```
