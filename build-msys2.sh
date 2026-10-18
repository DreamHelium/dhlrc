#!/usr/bin/env bash
#
# Build dhlrc with the MSYS2 UCRT64 toolchain.
#
# Run it from anywhere, from any shell: if it is not already running inside the
# MSYS2 UCRT64 environment it re-executes itself there. It checks that the
# packages the build needs are installed (and offers to install the missing
# ones), configures with CMake and builds. With `--package` it also assembles a
# redistributable archive — or an installer when NSIS is available.
#
# Usage: build-msys2.sh [options]
#   -h, --help             show this help and exit
#   -d, --debug            debug build
#   -r, --release          release build (default)
#       --no-gui           headless: Rust libraries and terminal frontend only
#       --no-translations  skip the gettext catalogue, so no gettext tools needed
#       --offline          build Rust from the local crate cache only
#   -p, --package          also build a package (zip/tgz, or an NSIS installer)
#   -c, --clean            remove the build directory first
#   -B, --build-dir DIR    build directory (default: <project>/build-ucrt64)
#   -j, --jobs N           parallel build jobs (default: as many as possible)
#   -y, --yes              install missing packages without asking
set -euo pipefail

usage() {
  # The comment block at the top of this file is the help text.
  awk 'NR > 2 { if ($0 !~ /^#/) exit; sub(/^# ?/, ""); print }' "${BASH_SOURCE[0]}"
}

src_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
script_path="$src_dir/$(basename "${BASH_SOURCE[0]}")"

# --- Make sure we are in the UCRT64 environment -------------------------------
if [ "${MSYSTEM:-}" != "UCRT64" ]; then
  msys2_usr=""
  # In any MSYS2 shell the installation root is `/` and MSYSTEM is set, so bash
  # lives at /usr/bin/bash and a fresh login shell switches the environment.
  if [ -f /etc/msystem ] && [ -x /usr/bin/bash ]; then
    msys2_usr=/usr
  else
    # Started from Git Bash or Windows: go looking for the installation.
    for root in "${MSYS2_ROOT:-}" /c/msys64 /c/msys2 /c/tools/msys64 \
      "/c/Program Files/msys64" "/c/Program Files (x86)/msys64"; do
      if [ -n "$root" ] && [ -x "$root/usr/bin/bash.exe" ]; then
        msys2_usr="$root/usr"
        break
      fi
    done
  fi

  if [ -z "$msys2_usr" ]; then
    echo "error: MSYS2 was not found." >&2
    echo "Install it from https://www.msys2.org/ and run this script again," >&2
    echo "or point MSYS2_ROOT at the installation directory." >&2
    exit 1
  fi

  echo "==> Switching to the MSYS2 UCRT64 environment"
  MSYSTEM=UCRT64 CHERE_INVOKING=1 exec "$msys2_usr/bin/bash.exe" --login "$script_path" "$@"
fi

# --- Options ------------------------------------------------------------------
build_type=Release
cargo_release=ON
gui=ON
translations=ON
offline=OFF
package=OFF
clean=OFF
build_dir=""
jobs=""
assume_yes=OFF

while [ $# -gt 0 ]; do
  case "$1" in
    -h|--help) usage; exit 0 ;;
    -d|--debug) build_type=Debug; cargo_release=OFF; shift ;;
    -r|--release) build_type=Release; cargo_release=ON; shift ;;
    --no-gui) gui=OFF; shift ;;
    --no-translations) translations=OFF; shift ;;
    --offline) offline=ON; shift ;;
    -p|--package) package=ON; shift ;;
    -c|--clean) clean=ON; shift ;;
    -B|--build-dir) build_dir=$2; shift 2 ;;
    -j|--jobs) jobs=$2; shift 2 ;;
    -y|--yes) assume_yes=ON; shift ;;
    *) echo "unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
done

build_dir=${build_dir:-"$src_dir/build-ucrt64"}

# --- Required packages --------------------------------------------------------
pkgs=(
  mingw-w64-ucrt-x86_64-gcc
  mingw-w64-ucrt-x86_64-cmake
  mingw-w64-ucrt-x86_64-ninja
  mingw-w64-ucrt-x86_64-rust
  # The gettext runtime provides libintl, which the Rust translation layer links.
  mingw-w64-ucrt-x86_64-gettext-runtime
)
if [ "$gui" = ON ]; then
  pkgs+=(
    mingw-w64-ucrt-x86_64-extra-cmake-modules
    mingw-w64-ucrt-x86_64-qt6-base
    mingw-w64-ucrt-x86_64-qt6-svg
    # windeployqt, used to bundle the Qt runtime into a package.
    mingw-w64-ucrt-x86_64-qt6-tools
    # Qt's own translations: the standard dialog buttons ("OK", "Restore
    # Defaults", ...) and the command line help come from this catalogue.
    mingw-w64-ucrt-x86_64-qt6-translations
    mingw-w64-ucrt-x86_64-kcolorscheme
    # The widget style the program selects by name; the packaging step copies
    # its plugin and theme files into the bundle.
    mingw-w64-ucrt-x86_64-breeze
    mingw-w64-ucrt-x86_64-kconfigwidgets
    mingw-w64-ucrt-x86_64-kcoreaddons
    mingw-w64-ucrt-x86_64-kwidgetsaddons
    mingw-w64-ucrt-x86_64-kiconthemes
    mingw-w64-ucrt-x86_64-ki18n
  )
fi
if [ "$translations" = ON ]; then
  # xgettext and msgfmt.
  pkgs+=(mingw-w64-ucrt-x86_64-gettext-tools)
fi
if [ "$package" = ON ] && [ "$gui" = ON ]; then
  # The NSIS compiler, for the installer (the portable archive needs nothing).
  pkgs+=(mingw-w64-ucrt-x86_64-nsis)
fi

# --- Check the packages -------------------------------------------------------
missing=()
for pkg in "${pkgs[@]}"; do
  if ! pacman -Q "$pkg" >/dev/null 2>&1; then
    missing+=("$pkg")
  fi
done

if [ "${#missing[@]}" -gt 0 ]; then
  echo
  echo "==> These packages are needed but not installed:"
  for pkg in "${missing[@]}"; do
    echo "        $pkg"
  done
  echo
  echo "    Install them with:"
  echo
  echo "        pacman -S --needed ${missing[*]}"
  echo
  if [ "$assume_yes" = ON ]; then
    : # install below
  elif [ -t 0 ]; then
    read -r -p "Install them now? [y/N] " reply
    case "$reply" in
      [yY]*) ;;
      *) echo "Aborting; nothing was built."; exit 1 ;;
    esac
  else
    echo "Re-run with --yes to install them automatically." >&2
    exit 1
  fi
  pacman -S --needed --noconfirm "${missing[@]}"
fi

# --- Check the tools ----------------------------------------------------------
for tool in cmake cargo ninja; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "error: $tool is still not available after the package check" >&2
    exit 1
  fi
done

# --- Configure ----------------------------------------------------------------
[ "$clean" = ON ] && rm -rf "$build_dir"

cmake_args=(
  -S "$src_dir"
  -B "$build_dir"
  -G Ninja
  "-DCMAKE_BUILD_TYPE=$build_type"
  "-DCARGO_RELEASE=$cargo_release"
  "-DDH_BUILD_GUI=$gui"
  "-DDH_TRANSLATIONS=$translations"
  "-DDH_CARGO_OFFLINE=$offline"
)
# Keep the package self-contained unless the target already has the runtime.
[ "$package" = ON ] && [ "$gui" = ON ] && cmake_args+=("-DDH_PACKAGE_RUNTIME=ON")

echo "==> Configuring ($build_type)"
cmake "${cmake_args[@]}"

# --- Build --------------------------------------------------------------------
echo "==> Building"
if [ -n "$jobs" ]; then
  cmake --build "$build_dir" --parallel "$jobs"
else
  cmake --build "$build_dir" --parallel
fi

echo
if [ "$gui" = ON ]; then
  echo "==> Built: $build_dir/dhlrc_qt.exe"
else
  echo "==> Built: $build_dir/dhlrc-tui.exe"
fi

# --- Package ------------------------------------------------------------------
if [ "$package" = ON ]; then
  echo
  echo "==> Packaging"
  if [ "$gui" = ON ]; then
    if command -v makensis >/dev/null 2>&1; then
      cmake --build "$build_dir" --target dhlrc_installer
    else
      echo "    (makensis is not installed, so only the portable archive is built."
      echo "     pacman -S mingw-w64-ucrt-x86_64-nsis for an .exe installer.)"
    fi
  fi
  ( cd "$build_dir" && cpack )
  echo
  echo "==> Packages in $build_dir:"
  find "$build_dir" -maxdepth 1 -type f -name 'dhlrc-[0-9]*' | sed 's/^/        /'
fi
