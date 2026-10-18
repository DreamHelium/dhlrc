#!/usr/bin/env bash
#
# Assemble the MSYS2 runtime a packaged dhlrc needs, in two parts:
#
#   <base_dir>  what the terminal frontend and the modules need — the MinGW and
#               gettext libraries. A terminal-only install needs nothing else.
#   <gui_dir>   the Qt/KF6 runtime and plugins the Qt frontend loads, plus the
#               Breeze style. The Qt frontend needs everything in <base_dir>
#               too, so an install that takes this part takes both.
#
# Called by the `dhlrc_runtime` build target (see CMakeLists.txt) with the two
# output directories and the build directory.
#
# `windeployqt` brings the Qt libraries and, above all, the plugins that are
# loaded at run time and so never show up in an import scan — the `platforms`
# plugin first of all. It knows nothing about the KF6, gettext or MinGW runtime,
# so those are resolved with `ldd`, whose paths tell the MSYS2 libraries
# (`/ucrt64/...`, which must be shipped) apart from the system ones
# (`/c/WINDOWS/...`, which must not).
set -euo pipefail
shopt -s nullglob

base_dir=${1:?usage: deploy-msys2-runtime.sh <base_dir> <gui_dir> <build_dir>}
gui_dir=${2:?usage: deploy-msys2-runtime.sh <base_dir> <gui_dir> <build_dir>}
build_dir=${3:?usage: deploy-msys2-runtime.sh <base_dir> <gui_dir> <build_dir>}
prefix=${MINGW_PREFIX:-/ucrt64}

exe="$build_dir/dhlrc_qt.exe"

mkdir -p "$base_dir" "$gui_dir"

# Qt libraries and the run-time plugins.
deployqt=$(command -v windeployqt6 || command -v windeployqt || true)
if [ -n "$deployqt" ]; then
  "$deployqt" --no-translations --no-compiler-runtime --dir "$gui_dir" "$exe" >/dev/null
else
  echo "warning: windeployqt not found; Qt plugins will be missing from the package" >&2
fi

# Plugins the application never loads: it downloads through the Rust core
# (`ureq`), so Qt's networking stack and its TLS/GLib backends are not used.
# Qt6Network itself stays, because Qt Quick — which the Breeze style links —
# imports it.
rm -rf "$gui_dir/tls" "$gui_dir/networkinformation" "$gui_dir/generic"

# `windeployqt` only ships the platform's own style. The application selects
# Breeze by name (`QApplication::setStyle ("breeze")` in src/main.cpp), so its
# plugin is copied in as well; its own dependencies — Qt Quick and OpenGL among
# them — then come in through the closure below.
#
# The package that provides it is `mingw-w64-ucrt-x86_64-breeze`; without it
# there is no Breeze style anywhere, so say so instead of silently shipping a
# package that falls back to the platform style.
breeze_found=OFF
for plugin_root in "$prefix/share/qt6/plugins" "$prefix/lib/qt6/plugins" \
  /ucrt64/share/qt6/plugins; do
  if [ -d "$plugin_root/styles" ]; then
    for style in breeze6.dll breeze.dll; do
      if [ -f "$plugin_root/styles/$style" ]; then
        mkdir -p "$gui_dir/styles"
        cp -f "$plugin_root/styles/$style" "$gui_dir/styles/"
        breeze_found=ON
      fi
    done
    if [ "$breeze_found" = ON ]; then
      break
    fi
  fi
done
if [ "$breeze_found" = OFF ]; then
  echo "warning: the Breeze style plugin was not found; the packaged window" >&2
  echo "         falls back to the platform style. Install" >&2
  echo "         mingw-w64-ucrt-x86_64-breeze before packaging." >&2
else
  # The style reads its theme description and colour schemes from the data
  # directory; copy them beside the executable, so the layout is the same as in
  # an MSYS2 installation.
  for data_file in "kstyle/themes/breeze.themerc" "color-schemes/BreezeLight.colors" \
    "color-schemes/BreezeDark.colors" "color-schemes/BreezeClassic.colors"; do
    if [ -f "$prefix/bin/data/$data_file" ]; then
      mkdir -p "$gui_dir/data/$(dirname "$data_file")"
      cp -f "$prefix/bin/data/$data_file" "$gui_dir/data/$data_file"
    fi
  done
fi

# KIconEngine is the icon engine that recolours the icon theme to match the
# active colour scheme; without it Breeze's monochrome icons stay black when a
# dark scheme is chosen. It is a Qt plugin, and windeployqt knows nothing about
# it, so it is copied by hand. KIconTheme::initTheme() adds
# <libraryPath>/kiconthemes6 to the plugin search path, which is why the plugin
# goes into a `kiconthemes6/iconengines` directory beside the executable.
icon_engine_found=OFF
for plugin_root in "$prefix/share/qt6/plugins" "$prefix/lib/qt6/plugins" \
  /ucrt64/share/qt6/plugins; do
  if [ -f "$plugin_root/kiconthemes6/iconengines/KIconEnginePlugin.dll" ]; then
    mkdir -p "$gui_dir/kiconthemes6/iconengines"
    cp -f "$plugin_root/kiconthemes6/iconengines/"*.dll \
      "$gui_dir/kiconthemes6/iconengines/"
    icon_engine_found=ON
    break
  fi
done
if [ "$icon_engine_found" = OFF ]; then
  echo "warning: the KIconEngine plugin was not found; the packaged window gets" >&2
  echo "         plain icons that do not follow the colour scheme (they stay" >&2
  echo "         dark in dark mode). Install mingw-w64-ucrt-x86_64-kiconthemes" >&2
  echo "         before packaging." >&2
fi

# KF6's own data. The catalogues for KF6's strings (gettext .mo as well as the
# *_qt.qm Qt uses) live in <prefix>/bin/data/locale on Windows, and the program
# looks for them in <exe>/data/locale. Only the languages the program itself is
# translated into are needed; the English source strings are the fallback.
languages=()
kde_locale_found=OFF
for locale_dir in "$build_dir"/locale/*/; do
  [ -d "$locale_dir" ] || continue
  language=$(basename "$locale_dir")
  if [ -d "$prefix/bin/data/locale/$language/LC_MESSAGES" ]; then
    mkdir -p "$gui_dir/data/locale/$language"
    cp -rf "$prefix/bin/data/locale/$language/LC_MESSAGES" \
      "$gui_dir/data/locale/$language/"
    languages+=("$language")
    kde_locale_found=ON
  fi
done
if [ "${#languages[@]}" -gt 0 ] && [ "$kde_locale_found" = OFF ]; then
  echo "warning: KF6's translation catalogues were not found; the packaged" >&2
  echo "         window keeps KF6's own strings (menu titles and the like) in" >&2
  echo "         English. They live in <prefix>/bin/data/locale; install" >&2
  echo "         mingw-w64-ucrt-x86_64-ki18n and its dependencies." >&2
fi

# Qt's own catalogue. It holds the standard dialog buttons ("OK", "Restore
# Defaults", ...) and the command-line help; the program loads it explicitly
# from <exe>/translations. Qt installs it under share/qt6/translations.
qt_found=OFF
if [ "${#languages[@]}" -gt 0 ] && [ -d "$prefix/share/qt6/translations" ]; then
  mkdir -p "$gui_dir/translations"
  for language in "${languages[@]}"; do
    for catalog in qtbase qt; do
      if [ -f "$prefix/share/qt6/translations/${catalog}_${language}.qm" ]; then
        cp -f "$prefix/share/qt6/translations/${catalog}_${language}.qm" \
          "$gui_dir/translations/"
        qt_found=ON
      fi
    done
  done
fi
if [ "${#languages[@]}" -gt 0 ] && [ "$qt_found" = OFF ]; then
  echo "warning: Qt's translations were not found; the packaged window keeps" >&2
  echo "         Qt's own strings (the standard dialog buttons) in English." >&2
  echo "         Install mingw-w64-ucrt-x86_64-qt6-translations before packaging." >&2
fi

# Every MSYS2 library the given files import, directly or not. `ldd` prints the
# MSYS2 ones as POSIX paths (`/ucrt64/...`) and everything else — system
# libraries, and this project's own output — under the `/c/` mount, so keeping
# the former is both the whole set needed and free of system DLLs.
closure_of() {
  ldd "$@" 2>/dev/null \
    | awk '$2 == "=>" && $3 ~ /^\// && tolower($3) !~ /^\/c\// { print $3 }' \
    | sort -u
}

copy_into() {
  local dir=$1
  shift
  local lib
  for lib in "$@"; do
    if [ -f "$lib" ]; then
      cp -f "$lib" "$dir/"
    fi
  done
}

# Base: the terminal frontend and both kinds of module.
base_libs=$(closure_of "$build_dir/dhlrc-tui.exe" \
                       "$build_dir"/load_module/*.dll \
                       "$build_dir"/region_module/*.dll)

# GUI: the Qt frontend and the plugins Qt loads from the deployed tree.
gui_libs=$(closure_of "$exe" \
                      "$gui_dir"/platforms/*.dll \
                      "$gui_dir"/styles/*.dll \
                      "$gui_dir"/imageformats/*.dll \
                      "$gui_dir"/iconengines/*.dll \
                      "$gui_dir"/kiconthemes6/iconengines/*.dll)

copy_into "$base_dir" $base_libs
# Everything the GUI needs on top of the base goes next to the Qt libraries.
copy_into "$gui_dir" $(comm -13 <(printf '%s\n' $base_libs) <(printf '%s\n' $gui_libs))
